#include "BackgroundLayer.h"
#include "frontend/windowing/SecondaryOutputWindow.h"
#include <iostream>
#include <chrono>
#include <GL/glew.h>
#include <unordered_map>

namespace ProyecThor::Core {

    namespace {
        struct BlitResources {
            GLuint vao = 0, vbo = 0, prog = 0;
        };

        static std::unordered_map<GLFWwindow*, BlitResources> s_ResourcesPerContext;

        // Registrado una sola vez: cuando una ventana secundaria (Proyector,
        // Stage, o cualquier otra a futuro) se destruye, purgamos su entrada
        // del cache. Sin esto, si GLFW reutiliza esa direccion de puntero
        // para una ventana nueva, BlitTexture() bindearia un VAO de un
        // contexto GL que ya no existe.
        static bool s_DestroyHookRegistered = [] {
            SecondaryOutputWindow::RegisterContextDestroyCallback(
                [](GLFWwindow* ctx) { s_ResourcesPerContext.erase(ctx); });
            return true;
        }();

        static const float k_QuadVerts[] = {
            -1.0f,  1.0f,  0.0f, 1.0f,
            -1.0f, -1.0f,  0.0f, 0.0f,
             1.0f, -1.0f,  1.0f, 0.0f,
            -1.0f,  1.0f,  0.0f, 1.0f,
             1.0f, -1.0f,  1.0f, 0.0f,
             1.0f,  1.0f,  1.0f, 1.0f,
        };

        static const char* k_BlitVert = R"GLSL(
#version 330 core
layout(location = 0) in vec2 a_Pos;
layout(location = 1) in vec2 a_UV;
out vec2 v_UV;
void main() {
    v_UV        = a_UV;
    gl_Position = vec4(a_Pos, 0.0, 1.0);
}
)GLSL";

        static const char* k_BlitFrag = R"GLSL(
#version 330 core
in  vec2      v_UV;
out vec4      fragColor;
uniform sampler2D u_Tex;
uniform float     u_Alpha;
uniform float     u_FlipY;
void main() {
    vec2 uv = vec2(v_UV.x, mix(v_UV.y, 1.0 - v_UV.y, u_FlipY));
    vec4 c = texture(u_Tex, uv);
    fragColor = vec4(c.rgb, c.a * u_Alpha);
}
)GLSL";

        static BlitResources& EnsureBlitResources()
        {
            GLFWwindow* ctx = glfwGetCurrentContext();
            auto it = s_ResourcesPerContext.find(ctx);
            if (it != s_ResourcesPerContext.end())
                return it->second;

            BlitResources res;
            auto compile = [](GLenum type, const char* src) -> GLuint {
                GLuint id = glCreateShader(type);
                glShaderSource(id, 1, &src, nullptr);
                glCompileShader(id);
                GLint ok = 0;
                glGetShaderiv(id, GL_COMPILE_STATUS, &ok);
                if (!ok) {
                    char log[512];
                    glGetShaderInfoLog(id, 512, nullptr, log);
                    std::cerr << "[BackgroundLayer] Shader error: " << log << "\n";
                    glDeleteShader(id);
                    return 0;
                }
                return id;
            };

            GLuint vert = compile(GL_VERTEX_SHADER,   k_BlitVert);
            GLuint frag = compile(GL_FRAGMENT_SHADER, k_BlitFrag);
            res.prog = glCreateProgram();
            glAttachShader(res.prog, vert);
            glAttachShader(res.prog, frag);
            glLinkProgram(res.prog);
            glDeleteShader(vert);
            glDeleteShader(frag);

            glGenVertexArrays(1, &res.vao);
            glGenBuffers(1, &res.vbo);
            glBindVertexArray(res.vao);
            glBindBuffer(GL_ARRAY_BUFFER, res.vbo);
            glBufferData(GL_ARRAY_BUFFER, sizeof(k_QuadVerts), k_QuadVerts, GL_STATIC_DRAW);
            glEnableVertexAttribArray(0);
            glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
            glEnableVertexAttribArray(1);
            glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
            glBindVertexArray(0);

            return s_ResourcesPerContext.emplace(ctx, res).first->second;
        }

        static void BlitTexture(GLuint tex, float alpha = 1.0f, float flipY = 0.0f)
        {
            BlitResources& res = EnsureBlitResources();
            glUseProgram(res.prog);
            glUniform1i(glGetUniformLocation(res.prog, "u_Tex"), 0);
            glUniform1f(glGetUniformLocation(res.prog, "u_Alpha"), alpha);
            glUniform1f(glGetUniformLocation(res.prog, "u_FlipY"), flipY);
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, tex);
            glBindVertexArray(res.vao);
            glDrawArrays(GL_TRIANGLES, 0, 6);
            glBindVertexArray(0);
            glBindTexture(GL_TEXTURE_2D, 0);
            glUseProgram(0);
        }

        static double NowSeconds()
        {
            using namespace std::chrono;
            return duration<double>(steady_clock::now().time_since_epoch()).count();
        }
    } // anonymous namespace

    // useHardwareDecode=false: el parametro de VLCBasePlayer existe
    // justamente para diagnosticar "contencion de sesiones de decode de
    // hardware" (ver el comentario en VLCBasePlayer.h) — muchas GPU de
    // consumo (tipico en NVIDIA GeForce) limitan cuantas sesiones NVDEC/
    // VAAPI concurrentes se pueden abrir a la vez. Esta app abre varios
    // VLCBasePlayer en simultaneo (Active+Standby de Fondos/Overlays/Cola
    // del Monitor, mas preview, mas overlay), y pedirle decode por
    // hardware a todos a la vez puede pasarse de ese limite: el sintoma es
    // exactamente "no aparece ningun frame nuevo" o glitches visuales al
    // arrancar un clip nuevo, que es lo que se reportaba con la cola en
    // Linux. Decode por software es un poco mas de CPU, pero no tiene techo
    // de sesiones concurrentes — para una herramienta de produccion en vivo,
    // confiabilidad gana sobre el ahorro de CPU.
    BackgroundLayer::BackgroundLayer(bool forceSilentAudio)
        : m_PlayerA(2, false, forceSilentAudio)
        , m_PlayerB(2, false, forceSilentAudio)
        , m_ForceSilentAudio(forceSilentAudio)
    {
    }

    VLCBasePlayer& BackgroundLayer::Active()  { return m_ActiveIsA ? m_PlayerA : m_PlayerB; }
    VLCBasePlayer& BackgroundLayer::Standby() { return m_ActiveIsA ? m_PlayerB : m_PlayerA; }

    void BackgroundLayer::Update()
    {
        // Destruye los NativePlayback retirados (ver RetireActiveNative())
        // una vez que paso tiempo de sobra para que su detach+stop
        // encolado en m_NativeLoader haya terminado — recien AHI es seguro
        // destruir su ventana (hilo principal, GLFW) y su VLCBasePlayer
        // (cuyo destructor llama Stop() de nuevo, inofensivo si ya esta
        // parado). Corre siempre, sin importar el motor actual.
        if (!m_RetiringNative.empty())
        {
            double now = NowSeconds();
            m_RetiringNative.erase(
                std::remove_if(m_RetiringNative.begin(), m_RetiringNative.end(),
                    [now](const std::unique_ptr<NativePlayback>& np) {
                        return (now - np->retiredAt) > kNativeRetireSeconds;
                    }),
                m_RetiringNative.end());
        }

        // Revela la ventana nueva (y recien ahi retira la anterior) cuando
        // ya este lista — ver comentario largo de m_NativeRevealPending en
        // el .h. Corre siempre, sin importar el motor actual.
        PollNativeReveal();

        // Contenido activo por motor nativo: Active() reproduce el mismo video
        // en silencio para mantener su textura OpenGL actualizada y alimentar
        // la Vista en Vivo (ViewPanel).
        if (m_ActiveIsNative)
        {
            Active().UpdateTexture();
            Active().SetAudioActive(false);
            Active().SetMute(true);
            Active().SetVolume(0);

            if (m_ActiveNative)
            {
                bool nativePaused = m_ActiveNative->player.IsPaused();
                if (Active().IsPaused() != nativePaused)
                    Active().SetPause(nativePaused);

                double now = NowSeconds();
                if (now - m_LastNativeSyncTime > 1.5)
                {
                    m_LastNativeSyncTime = now;
                    int64_t nativeTime = m_ActiveNative->player.GetTime();
                    int64_t activeTime = Active().GetTime();
                    int64_t len = m_ActiveNative->player.GetLength();
                    if (len > 0 && std::abs(nativeTime - activeTime) > 1500)
                    {
                        Active().SetPosition(static_cast<float>(nativeTime) / static_cast<float>(len));
                    }
                }
            }
            return;
        }

        Active().UpdateTexture();
        Active().EnforceSilenceIfNeeded();
        Standby().EnforceSilenceIfNeeded();

        // FIX: antes solo se subia la textura del player Active — mientras
        // habia un swap pendiente, Standby().GetTextureID() seguia
        // mostrando lo que ESE player object tenia de la ultima vez que fue
        // Active (textura vieja/de otro clip), no el clip nuevo que se esta
        // cargando ahora, hasta el mismo frame en que PerformSwap() corria.
        // El crossfade en Render() (que arranca a mostrar standby en cuanto
        // hay HasVideoFrame()) podia entonces blendear con una textura
        // incorrecta durante esa ventana.
        if (m_SwapPending)
            Standby().UpdateTexture();

        // El prefetch (ver Prefetch()) tambien necesita su textura al dia,
        // aunque todavia no este armado el swap — asi cuando CommitPrefetch()
        // lo arme, Render() ya tiene algo correcto para mostrar de una.
        if (m_PrefetchArmed && !m_SwapPending)
        {
            Standby().UpdateTexture();

            // Clave para que el corte sea instantaneo Y no se desincronice:
            // dejamos que Standby() decodifique un rato (kSwapSettleSeconds,
            // ver comentario en el .h) y RECIEN AHI lo pausamos — no
            // apenas aparece el primer frame. Los primeros frames de un
            // codec recien abierto a veces son artefactos del decoder
            // "calentando" (frame parcial/con colores mal); pausar de
            // entrada podia dejar el standby congelado en uno de esos
            // frames rotos para siempre (al estar pausado, nunca mas
            // decodifica otro que lo corrija). Una vez asentado, se busca
            // la posicion 0 ANTES de pausar — asi sigue arrancando
            // exactamente desde el principio, no desde donde se asento.
            // Si se dejara correr sin pausar nunca (version vieja): (1)
            // llegaria al swap ya avanzado, (2) podia alcanzar su propio
            // fin mientras seguia oculto y la cola lo saltaba apenas se
            // mostraba, y (3) doblaba la carga de decode sostenida — un
            // aporte real al desfasaje bajo sobrecarga.
            if (!Standby().IsPaused())
            {
                if (m_PrefetchReadyAt == 0.0 &&
                    Standby().GetLoadState() == VLCBasePlayer::LoadState::Ready)
                {
                    m_PrefetchReadyAt = NowSeconds();
                }

                if (m_PrefetchReadyAt != 0.0 &&
                    (NowSeconds() - m_PrefetchReadyAt) >= kSwapSettleSeconds)
                {
                    Standby().SetPosition(0.0f);
                    Standby().SetPause(true);
                }
            }
        }
        else
        {
            m_PrefetchReadyAt = 0.0;
        }

        if (m_SwapPending)
        {
            VLCBasePlayer& standby = Standby();
            auto standbyState = standby.GetLoadState();

            bool ready   = standbyState == VLCBasePlayer::LoadState::Ready;
            // Un error real (codec no soportado, archivo corrupto, ruta rota)
            // nunca va a convertirse en Ready por mas que se espere — swapear
            // de inmediato hace visible el fin-de-clip/error ya seteado en
            // este player (ver OnVlcEvent), asi MonitorQueueEngine::Update()
            // lo detecta y salta al siguiente item en el mismo frame, en vez
            // de quedar pegado.
            bool errored = standbyState == VLCBasePlayer::LoadState::Error;
            double now   = NowSeconds();

            if (errored)
            {
                PerformSwap();
                m_SwapReadyAt   = 0.0;
                m_SwapSettledAt = 0.0;
            }
            else if (ready)
            {
                // FIX: antes, si standby no llegaba a Ready dentro de 3s, el
                // swap se forzaba IGUAL — mostrando lo que sea que esa
                // instancia tuviera cargado en su textura de una carga
                // ANTERIOR (nunca se limpia al hacer Stop()), no el contenido
                // nuevo. El publico veia "un fondo sin ninguna relacion" por
                // un instante. Ahora NUNCA se corta a algo que no este
                // realmente listo — se espera lo que haga falta (ver el
                // limite de emergencia mas abajo, que abandona el swap sin
                // mostrar nada raro en vez de forzarlo).
                //
                // Una vez listo, en vez de un corte seco se hace un
                // crossfade corto y fijo (frame final del fondo anterior ->
                // frame inicial del nuevo, nada mas en el medio) via
                // m_TransitionProgress, que ya consume Render().
                if (m_SwapReadyAt == 0.0)
                {
                    m_SwapReadyAt = now;
                    float loadedIn = static_cast<float>(now - m_PendingSwapStart);
                    m_RecentLoadDurations.push_back(loadedIn);
                    if (m_RecentLoadDurations.size() > kMaxLoadSamples)
                        m_RecentLoadDurations.pop_front();
                }

                double sinceReady = now - m_SwapReadyAt;
                bool   settled    = sinceReady >= kSwapSettleSeconds;

                if (!settled)
                {
                    // Todavia "asentando": no mostrar nada de standby en el
                    // blend por ahora (ver comentario de kSwapSettleSeconds
                    // en el .h — mismo margen que ya aplico el prefetch
                    // antes de pausar, asi que en el camino de cola esto
                    // suele resolverse casi al instante).
                    m_TransitionProgress = 0.0f;
                }
                else
                {
                    if (m_SwapSettledAt == 0.0)
                        m_SwapSettledAt = now;

                    double blendElapsed = now - m_SwapSettledAt;
                    m_TransitionProgress = static_cast<float>(
                        std::clamp(blendElapsed / m_BlendSeconds, 0.0, 1.0));

                    if (blendElapsed >= m_BlendSeconds)
                    {
                        PerformSwap();
                        m_SwapReadyAt   = 0.0;
                        m_SwapSettledAt = 0.0;
                    }
                }
            }
            else if ((now - m_PendingSwapStart) > kSwapGiveUpSeconds)
            {
                // Limite de emergencia (carga colgada, no error ni Ready):
                // se abandona el swap y se sigue mostrando lo de antes, en
                // vez de forzar un corte a contenido no listo.
                std::cerr << "[BackgroundLayer] Swap abandonado tras "
                          << kSwapGiveUpSeconds << "s sin quedar listo ni dar error; "
                          << "se mantiene el fondo anterior.\n";
                m_SwapPending   = false;
                m_SwapReadyAt   = 0.0;
                m_SwapSettledAt = 0.0;
            }
            // si no, sigue esperando sin forzar nada.
        }

        // ── Ping-pong "bucle falso" ──────────────────────────────────────
        // Ver comentario largo del miembro m_PingPongEnabled en el .h. Solo
        // corre sobre contenido de Fondos (allowAudio=false) ya asentado
        // (nunca en medio de un swap/prefetch, para no interferir con esa
        // logica de carga). MonitorQueueEngine::Update() consume
        // EndReached de forma independiente y solo cuando hay una cola de
        // Videos activa, asi que no hay conflicto por consumir el evento
        // aca tambien.
        if (m_PingPongEnabled && m_IsVideo && !m_ContentAllowsAudio && !m_SwapPending)
        {
            VLCBasePlayer& active = Active();
            double now = NowSeconds();

            if (!m_PingPongReverse)
            {
                if (active.ConsumeEndReached())
                {
                    m_PingPongReverse    = true;
                    m_PingPongLastStepAt = now;
                    // De aca en mas la posicion se mueve a mano (ver abajo):
                    // pausar corta el avance natural, asi cada SetPosition()
                    // refleja un paso limpio hacia atras sin que el decode
                    // en curso lo compense.
                    active.SetPause(true);
                }
            }
            else
            {
                int64_t lenMs = active.GetLength();
                double  dt    = now - m_PingPongLastStepAt;

                if (lenMs <= 0)
                {
                    // Sin duracion valida (raro, pero mejor no quedar
                    // trabado pausado para siempre): se abandona la reversa.
                    active.SetPause(false);
                    m_PingPongReverse = false;
                }
                else if (dt >= kPingPongStepSeconds)
                {
                    m_PingPongLastStepAt = now;
                    int64_t curMs  = active.GetTime();
                    int64_t stepMs = static_cast<int64_t>(dt * 1000.0);
                    int64_t newMs  = curMs - stepMs;

                    if (newMs <= 0)
                    {
                        active.SetPosition(0.0f);
                        active.SetPause(false);
                        m_PingPongReverse = false;
                    }
                    else
                    {
                        active.SetPosition(static_cast<float>(newMs) / static_cast<float>(lenMs));
                    }
                }
            }
        }
    }

    void BackgroundLayer::Render(int outputW, int outputH)
    {
        // Contenido activo por motor nativo: la ventana de VLC se muestra
        // por su cuenta, fuera de este compositor GL — nada que blitear.
        if (m_ActiveIsNative) return;

        GLuint rawTex = static_cast<GLuint>(
            reinterpret_cast<uintptr_t>(Active().GetTextureID()));

        if (rawTex == 0)
            return;

        int srcW = 0, srcH = 0;
        Active().GetVideoSize(srcW, srcH);

        if (srcW <= 0 || srcH <= 0)
            return;

        int viewX = 0;
        int viewY = 0;
        int viewW = outputW;
        int viewH = outputH;

        if (!m_StretchToFill) {

            float videoRatio  = static_cast<float>(srcW) / static_cast<float>(srcH);
            float screenRatio = static_cast<float>(outputW) / static_cast<float>(outputH);

            if (videoRatio > screenRatio + 0.001f) {
                viewW = outputW;
                viewH = static_cast<int>(static_cast<float>(outputW) / videoRatio);
                viewX = 0;
                viewY = (outputH - viewH) / 2;
            } else if (videoRatio < screenRatio - 0.001f) {
                viewH = outputH;
                viewW = static_cast<int>(static_cast<float>(outputH) * videoRatio);
                viewX = (outputW - viewW) / 2;
                viewY = 0;
            }
        }

        GLuint finalTex = rawTex;

        if (m_FSREnabled) {
            bool needReinit = (!m_FSR.IsInitialized() ||
                               m_FSR.GetOutputW() != viewW ||
                               m_FSR.GetOutputH() != viewH);

            if (needReinit) {
                bool ok = m_FSR.Init(viewW, viewH);
                if (ok) {
                    m_FSR.SetSharpness(m_FSRSharpness);
                    m_FSR.SetEnabled(true);
                } else {
                    m_FSREnabled = false;
                    std::cout << "[BG] FSR no disponible, usando blit directo.\n";
                }
            }

            if (m_FSREnabled && m_FSR.IsInitialized() && (srcW < viewW || srcH < viewH)) {
                GLuint upscaled = m_FSR.Process(rawTex, srcW, srcH);
                if (upscaled != 0)
                    finalTex = upscaled;
            }
        } else if (m_NISEnabled) {
            bool needReinit = (!m_NIS.IsInitialized() ||
                               m_NIS.GetOutputW() != viewW ||
                               m_NIS.GetOutputH() != viewH);

            if (needReinit) {
                bool ok = m_NIS.Init(viewW, viewH);
                if (ok) {
                    m_NIS.SetSharpness(m_NISSharpness);
                    m_NIS.SetEnabled(true);
                } else {
                    m_NISEnabled = false;
                    std::cout << "[BG] NIS no disponible, usando blit directo.\n";
                }
            }

            if (m_NISEnabled && m_NIS.IsInitialized() && (srcW < viewW || srcH < viewH)) {
                GLuint upscaled = m_NIS.Process(rawTex, srcW, srcH);
                if (upscaled != 0)
                    finalTex = upscaled;
            }
        }

        if (m_SwapPending && Standby().HasVideoFrame())
        {
            int stW = 0, stH = 0;
            Standby().GetVideoSize(stW, stH);
            int sbX = 0, sbY = 0, sbW = outputW, sbH = outputH;
            if (stW > 0 && stH > 0 && !m_StretchToFill) {
                float videoRatio  = static_cast<float>(stW) / static_cast<float>(stH);
                float screenRatio = static_cast<float>(outputW) / static_cast<float>(outputH);
                if (videoRatio > screenRatio + 0.001f) {
                    sbW = outputW;
                    sbH = static_cast<int>(static_cast<float>(outputW) / videoRatio);
                    sbX = 0;
                    sbY = (outputH - sbH) / 2;
                } else if (videoRatio < screenRatio - 0.001f) {
                    sbH = outputH;
                    sbW = static_cast<int>(static_cast<float>(outputH) * videoRatio);
                    sbX = (outputW - sbW) / 2;
                    sbY = 0;
                }
            }

            GLuint standbyTex = static_cast<GLuint>(reinterpret_cast<uintptr_t>(Standby().GetTextureID()));
            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

            if (finalTex != 0 && (1.0f - m_TransitionProgress) > 0.001f) {
                glViewport(viewX, viewY, viewW, viewH);
                BlitTexture(finalTex, 1.0f - m_TransitionProgress, m_FlipVideoY ? 1.0f : 0.0f);
            }

            glViewport(sbX, sbY, sbW, sbH);
            BlitTexture(standbyTex, m_TransitionProgress, m_FlipVideoY ? 1.0f : 0.0f);
            glDisable(GL_BLEND);
        }
        else
        {
            glDisable(GL_BLEND);
            glViewport(viewX, viewY, viewW, viewH);
            BlitTexture(finalTex, 1.0f, m_FlipVideoY ? 1.0f : 0.0f);
        }

        glViewport(0, 0, outputW, outputH);
    }

    void BackgroundLayer::RenderLogo(unsigned int logoTex, int logoW, int logoH, int outputW, int outputH)
    {
        if (logoTex == 0 || logoW <= 0 || logoH <= 0 || outputW <= 0 || outputH <= 0) return;

        // Mismo criterio de letterbox que Render() (no estira, mantiene la
        // proporcion real del logo dentro del viewport de salida).
        int viewX = 0, viewY = 0, viewW = outputW, viewH = outputH;

        float logoRatio   = static_cast<float>(logoW) / static_cast<float>(logoH);
        float screenRatio = static_cast<float>(outputW) / static_cast<float>(outputH);

        if (logoRatio > screenRatio + 0.001f) {
            viewW = outputW;
            viewH = static_cast<int>(static_cast<float>(outputW) / logoRatio);
            viewX = 0;
            viewY = (outputH - viewH) / 2;
        } else if (logoRatio < screenRatio - 0.001f) {
            viewH = outputH;
            viewW = static_cast<int>(static_cast<float>(outputH) * logoRatio);
            viewX = (outputW - viewW) / 2;
            viewY = 0;
        }

        glDisable(GL_BLEND);
        glViewport(0, 0, outputW, outputH);
        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        glViewport(viewX, viewY, viewW, viewH);
        BlitTexture(static_cast<GLuint>(logoTex), 1.0f, m_FlipVideoY ? 1.0f : 0.0f);

        glViewport(0, 0, outputW, outputH);
    }

    void BackgroundLayer::SetStretchToFill(bool stretch)
    {
        m_StretchToFill = stretch;
    }

    bool BackgroundLayer::GetStretchToFill() const
    {
        return m_StretchToFill;
    }

    void BackgroundLayer::SetFSREnabled(bool enabled)
    {
        m_FSREnabled = enabled;
        m_FSR.SetEnabled(enabled);
    }

    bool BackgroundLayer::GetFSREnabled() const
    {
        return m_FSREnabled;
    }

    void BackgroundLayer::SetFSRSharpness(float sharpness)
    {
        m_FSRSharpness = sharpness;
        m_FSR.SetSharpness(sharpness);
    }

    float BackgroundLayer::GetFSRSharpness() const
    {
        return m_FSRSharpness;
    }

    void BackgroundLayer::SetNISEnabled(bool enabled)
    {
        m_NISEnabled = enabled;
        m_NIS.SetEnabled(enabled);
    }

    bool BackgroundLayer::GetNISEnabled() const
    {
        return m_NISEnabled;
    }

    void BackgroundLayer::SetNISSharpness(float sharpness)
    {
        m_NISSharpness = sharpness;
        m_NIS.SetSharpness(sharpness);
    }

    float BackgroundLayer::GetNISSharpness() const
    {
        return m_NISSharpness;
    }

    void* BackgroundLayer::GetTextureID()
    {
        return Active().GetTextureID();
    }

    void* BackgroundLayer::GetStandbyTextureID()
    {
        return Standby().GetTextureID();
    }

    bool BackgroundLayer::StandbyHasFrame()
    {
        return Standby().HasVideoFrame();
    }

    VLCBasePlayer* BackgroundLayer::GetPlayer()
    {
        // Punto unico usado por la cola del Monitor (MonitorQueueEngine::
        // Update -> ConsumeEndReached/ConsumeHadError, sin lo cual la cola
        // nunca avanza) y por los controles de transporte/preview (play,
        // pausa, seek, VU meter) para llegar al reproductor que
        // REALMENTE tiene el contenido activo — con el motor libvlc
        // (m_ActiveIsNative), eso es m_ActiveNative->player, no Active().
        if (m_ActiveIsNative && m_ActiveNative) return &m_ActiveNative->player;
        return &Active();
    }

    void BackgroundLayer::SeekSync(float pos)
    {
        if (m_ActiveIsNative)
            Active().SetPosition(pos);
    }

    void BackgroundLayer::GetActiveVideoSize(int& width, int& height)
    {
        if (m_SwapPending && Standby().HasVideoFrame())
        {
            int sw = 0, sh = 0;
            Standby().GetVideoSize(sw, sh);
            if (sw > 0 && sh > 0)
            {
                width = sw;
                height = sh;
                return;
            }
        }

        if (m_ActiveIsNative && m_ActiveNative)
        {
            m_ActiveNative->player.GetVideoSize(width, height);
            if (width > 0 && height > 0) return;
        }

        Active().GetVideoSize(width, height);
        if ((width <= 0 || height <= 0) && Standby().HasVideoFrame())
        {
            Standby().GetVideoSize(width, height);
        }
    }

    void BackgroundLayer::SetVideo(const std::string& path, bool allowAudio)
    {
        // 0. Seguridad absoluta: Los fondos de pantalla (assets/backgrounds) NUNCA deben sonar.
        // Solo videos de medios (assets/videos) o streams pueden permitir audio si allowAudio=true.
        std::string normPath = path;
        std::replace(normPath.begin(), normPath.end(), '\\', '/');
        std::transform(normPath.begin(), normPath.end(), normPath.begin(),
                       [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        if (normPath.find("/backgrounds/") != std::string::npos ||
            normPath.find("assets/backgrounds") != std::string::npos)
        {
            allowAudio = false;
        }

        m_ContentAllowsAudio = allowAudio;

        // Si el contenido a reproducir NO permite audio (ej: es un Background decorativo),
        // debemos SILENCIAR INMEDIATAMENTE cualquier audio activo (del video anterior)
        // para que no siga sonando durante el swap/crossfade/transicion.
        if (!allowAudio)
        {
            Active().SetAudioActive(false);
            Active().SetMute(true);
            Active().SetVolume(0);
            Standby().SetAudioActive(false);
            Standby().SetMute(true);
            Standby().SetVolume(0);
            if (m_ActiveNative)
            {
                m_ActiveNative->player.SetAudioActive(false);
                m_ActiveNative->player.SetMute(true);
                m_ActiveNative->player.SetVolume(0);
            }
        }

        // El motor nativo aplica SOLO a video real (allowAudio=true —
        // Videos/cola del Monitor): Fondos/imagenes (allowAudio=false)
        // siempre necesitan overlays/texto encima, asi que siempre van
        // por OpenGL sin importar este ajuste (ver comentario del
        // miembro m_UseNativeEngine en el .h).
        if (m_UseNativeEngine && allowAudio)
        {
            // Sin crossfade/standby en este motor: corte directo.
            m_IsVideo             = true;
            m_ActiveIsNative      = true;

            // Reproducir el video en Active() (OpenGL) en modo 100% silencioso
            // para proveer la textura en tiempo real a ViewPanel (Vista en Vivo).
            Active().Play(path, /*loop=*/false, /*startMuted=*/true);
            Active().SetAudioActive(false);
            Active().SetMute(true);
            Active().SetVolume(0);

            Standby().SetAudioActive(false);
            Standby().SetMute(true);
            Standby().SetVolume(0);
            Standby().Stop();

            m_SwapPending        = false;
            m_SwapReadyAt        = 0.0;
            m_SwapSettledAt      = 0.0;
            m_TransitionProgress = 0.0f;
            m_PrefetchArmed      = false;
            m_PrefetchedPath.clear();

            if (m_PendingRetireNative) {
                // Ya habia una transicion en danza cuando aparecio esta
                // tercera — ese "anterior" quedo doblemente obsoleto.
                RetireNativePlayback(std::move(m_PendingRetireNative));
            }
            if (m_ActiveNative) {
                m_ActiveNative->player.SetAudioActive(false);
                m_ActiveNative->player.SetMute(true);
                m_ActiveNative->player.SetVolume(0);
                m_PendingRetireNative = std::move(m_ActiveNative);
            }

            auto fresh = std::make_unique<NativePlayback>(m_ForceSilentAudio);
            // Reproductor recien construido, sin usar todavia: seguro
            // aplicar el dispositivo de audio elegido aca mismo (hilo
            // principal), antes de que este NativePlayback haga nada.
            if (!m_AudioDeviceId.empty())
                fresh->player.SetAudioDevice(m_AudioDeviceId);

            void* handle = m_IsLiveToPublic ? fresh->window.CreateHidden(m_LastKnownMonitorIndex) : nullptr;
            VLCBasePlayer* newPlayerPtr = &fresh->player;

            // Adjuntar la ventana tiene que pasar ANTES de Play() (la doc
            // de libVLC dice que set_hwnd/set_xwindow "toma efecto cuando
            // arranca la reproduccion").
            bool allowAudioCopy = allowAudio;
            m_NativeLoader.Request([this, newPlayerPtr, path, handle, allowAudioCopy]() {
                if (handle) newPlayerPtr->AttachNativeWindow(handle);
                // Los fondos (allowAudio=false) siempre deben repetirse en
                // loop; los videos reales (allowAudio=true, cola del
                // Monitor) NO -- MonitorQueueEngine::Update() depende de
                // que ConsumeEndReached() dispare de verdad al terminar
                // para avanzar la cola, cosa que nunca pasaria si loopean.
                newPlayerPtr->Play(path, /*loop=*/!allowAudioCopy, /*startMuted=*/true);

                bool live       = m_IsLiveToPublic.load(std::memory_order_relaxed);
                bool muted      = m_TargetMuted.load(std::memory_order_relaxed);
                int  volume     = m_TargetVolume.load(std::memory_order_relaxed);
                bool wantActive = live && allowAudioCopy;
                newPlayerPtr->SetAudioActive(wantActive);
                newPlayerPtr->SetMute(muted || !wantActive);
                newPlayerPtr->SetVolume((wantActive && !muted) ? volume : 0);
            });

            m_NativeRevealPending = true;
            m_NativeRevealStart   = NowSeconds();

            m_ActiveNative = std::move(fresh);
            return;
        }

        // Esto va por OpenGL: si el contenido activo ANTERIOR era nativo,
        // hay que apagarlo primero — la ventana nativa no debe seguir
        // tapando "ProjectorLive" con un video viejo mientras esto nuevo
        // carga.
        if (m_ActiveIsNative)
        {
            m_ActiveIsNative = false;
            RetireActiveNative();
        }

        // FIX (freeze/flash en clicks repetidos): si esto es EXACTAMENTE lo
        // que ya se esta mostrando (nada en swap, misma ruta ya activa), es
        // un pedido redundante — ignorarlo evita reiniciar innecesariamente
        // el clip y evita el ping-pong entre Active/Standby que un guard a
        // nivel VLCBasePlayer por si solo no cubre (cada swap deja al
        // player saliente con la ruta limpiada por su propio Stop()).
        if (!m_SwapPending && GetTextureID() != nullptr && path == Active().GetCurrentPath())
            return;

        m_IsVideo = true;

        // Fondos con ping-pong activo NO deben loopear via libVLC
        // (input-repeat): necesitamos que llegue un EndReached real al
        // terminar el pase hacia adelante para poder arrancar la fase de
        // reversa manual (ver Update()). Videos reales (allowAudio=true)
        // nunca loopean de todos modos, con o sin ping-pong.
        bool wantNativeLoop = !allowAudio && !m_PingPongEnabled;
        m_PingPongReverse    = false;
        m_PingPongLastStepAt = 0.0;

        // Cualquier carga directa (click manual en Fondos/Videos, etc.)
        // toma standby para si misma — invalida un prefetch de cola que
        // pudiera estar esperando ahi, para que CommitPrefetch() no lo
        // confunda despues con contenido que ya no es el que arranco.
        m_PrefetchArmed = false;
        m_PrefetchedPath.clear();

        if (m_SwapPending || GetTextureID() != nullptr)
        {
            // Ver comentario de wantNativeLoop arriba: los fondos
            // (allowAudio=false) loopean via libVLC salvo que ping-pong
            // este activo, los videos reales nunca.
            Standby().Play(path, /*loop=*/wantNativeLoop, /*startMuted=*/true);
            Standby().SetAudioActive(false);
            Standby().SetMute(true);
            Standby().SetVolume(0);
            m_SwapPending      = true;
            m_PendingSwapStart = NowSeconds();
            m_SwapReadyAt   = 0.0;
            m_SwapSettledAt = 0.0;
            m_TransitionProgress = 0.0f;
        }
        else
        {
            Active().Play(path, /*loop=*/wantNativeLoop, /*startMuted=*/true);
            if (!m_IsLiveToPublic || !allowAudio)
            {
                Active().SetAudioActive(false);
                Active().SetMute(true);
                Active().SetVolume(0);
            }
            else
            {
                Active().SetAudioActive(true);
                Active().SetMute(m_TargetMuted);
                Active().SetVolume(m_TargetMuted ? 0 : m_TargetVolume.load());
                ReapplyLiveEqualizer(Active());
            }
        }
    }

    float BackgroundLayer::GetEstimatedLoadSeconds() const
    {
        if (m_RecentLoadDurations.empty()) return kDefaultEtaSeconds;
        float sum = 0.0f;
        for (float v : m_RecentLoadDurations) sum += v;
        return sum / static_cast<float>(m_RecentLoadDurations.size());
    }

    void BackgroundLayer::Prefetch(const std::string& path, bool allowAudio)
    {
        std::string normPath = path;
        std::replace(normPath.begin(), normPath.end(), '\\', '/');
        std::transform(normPath.begin(), normPath.end(), normPath.begin(),
                       [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        if (normPath.find("/backgrounds/") != std::string::npos ||
            normPath.find("assets/backgrounds") != std::string::npos)
        {
            allowAudio = false;
        }

        // Sin crossfade en el motor nativo, no hay nada util que precargar
        // (ver CommitPrefetch(), que en este modo cae directo a SetVideo()).
        // Igual que en SetVideo(): solo aplica a video real (allowAudio).
        if (m_UseNativeEngine && allowAudio) return;

        // Si ya hay un swap en curso, Standby() es justo el player que esta
        // por pasar a Active — pisarlo aca corromperia ese swap en vuelo.
        // Se descarta este prefetch; quien llama puede reintentar en un
        // frame posterior (la cola lo hace de forma natural, ver
        // MonitorQueueEngine: solo prefetch-ea recien cuando SU swap ya
        // termino).
        if (m_SwapPending) return;

        // Igual que la rama "ya hay algo al aire" de SetVideo(), pero SIN
        // armar m_SwapPending: el clip queda cargando (y luego pausado, ver
        // Update()) en standby, listo para cuando CommitPrefetch() lo pida,
        // sin disparar el crossfade por su cuenta.
        m_IsVideo = true;
        m_ContentAllowsAudio = allowAudio;
        m_PrefetchedPath = path;
        m_PrefetchArmed  = true;
        m_PrefetchReadyAt = 0.0; // arranca de cero el asentamiento para ESTE prefetch

        // Ver comentario de wantNativeLoop en SetVideo(): fondos loopean via
        // libVLC salvo que ping-pong este activo, videos reales nunca.
        bool wantNativeLoop = !allowAudio && !m_PingPongEnabled;
        Standby().Play(path, /*loop=*/wantNativeLoop, /*startMuted=*/true);
        Standby().SetAudioActive(false);
        Standby().SetMute(true);
        Standby().SetVolume(0);
    }

    void BackgroundLayer::CommitPrefetch(const std::string& path, bool allowAudio)
    {
        std::string normPath = path;
        std::replace(normPath.begin(), normPath.end(), '\\', '/');
        std::transform(normPath.begin(), normPath.end(), normPath.begin(),
                       [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        if (normPath.find("/backgrounds/") != std::string::npos ||
            normPath.find("assets/backgrounds") != std::string::npos)
        {
            allowAudio = false;
        }

        if (!allowAudio)
        {
            Active().SetAudioActive(false);
            Active().SetMute(true);
            Active().SetVolume(0);
            Standby().SetAudioActive(false);
            Standby().SetMute(true);
            Standby().SetVolume(0);
            if (m_ActiveNative)
            {
                m_ActiveNative->player.SetAudioActive(false);
                m_ActiveNative->player.SetMute(true);
                m_ActiveNative->player.SetVolume(0);
            }
        }

        // Sin prefetch en el motor nativo (ver Prefetch()): cae directo a
        // un corte simple, igual que si nunca se hubiera precargado nada.
        if (m_UseNativeEngine && allowAudio) { SetVideo(path, allowAudio); return; }

        // Esto va por OpenGL: mismo apagado del nativo que en SetVideo(),
        // por si el contenido activo anterior venia de ahi.
        if (m_ActiveIsNative)
        {
            m_ActiveIsNative = false;
            RetireActiveNative();
        }

        if (m_PrefetchArmed && m_PrefetchedPath == path)
        {
            // Ya esta listo (Update() lo pauso apenas decodifico su primer
            // frame): solo hace falta armar el swap. El gate normal de
            // Update() lo encuentra Ready de inmediato y PerformSwap()
            // lo despausa ahi mismo, asi que el corte es instantaneo.
            m_ContentAllowsAudio = allowAudio;
            m_SwapPending        = true;
            m_PendingSwapStart   = NowSeconds();
            // Mismo motivo que en SetVideo(): nunca heredar timing NI valor
            // de blend de un swap anterior (ver el comentario ahi para el
            // detalle completo de por que ambas cosas hacen falta).
            m_SwapReadyAt        = 0.0;
            m_SwapSettledAt      = 0.0;
            m_TransitionProgress = 0.0f;
            m_PrefetchArmed      = false;
            m_PrefetchedPath.clear();
            return;
        }

        // Nada precargado (o precargo otra cosa, ej. la cola se reordeno):
        // carga en frio normal, mismo camino que un click directo.
        m_PrefetchArmed = false;
        m_PrefetchedPath.clear();
        SetVideo(path, allowAudio);
    }

    void BackgroundLayer::PerformSwap()
    {
        VLCBasePlayer& oldActive = Active();
        m_ActiveIsA = !m_ActiveIsA;
        VLCBasePlayer& newActive = Active();

        // Ahora el swap respeta el permiso asociado al contenido que se esta
        // por mostrar, no solo el estado global "al aire".
        if (m_IsLiveToPublic && m_ContentAllowsAudio.load(std::memory_order_relaxed))
        {
            newActive.SetAudioActive(true);
            newActive.SetMute(m_TargetMuted);
            newActive.SetVolume(m_TargetMuted ? 0 : m_TargetVolume.load());
            ReapplyLiveEqualizer(newActive);
        }
        else
        {
            newActive.SetAudioActive(false);
            newActive.SetMute(true);
            newActive.SetVolume(0);
        }
        newActive.SetPause(false);

        oldActive.SetAudioActive(false);
        oldActive.SetMute(true);
        oldActive.SetVolume(0);
        oldActive.Stop();

        m_SwapPending = false;

        // El contenido que acaba de quedar activo arranca su propio pase
        // desde cero (ver ping-pong en Update()), sin arrastrar la fase de
        // reversa de lo que se estaba mostrando antes.
        m_PingPongReverse    = false;
        m_PingPongLastStepAt = 0.0;
    }

    void BackgroundLayer::SetSolidColor(float r, float g, float b)
    {
        m_ContentAllowsAudio = false;
        m_IsVideo    = false;
        m_BgColor[0] = r;
        m_BgColor[1] = g;
        m_BgColor[2] = b;

        m_SwapPending   = false;
        m_SwapReadyAt   = 0.0;
        m_SwapSettledAt = 0.0;
        m_PrefetchArmed = false;
        m_PrefetchedPath.clear();

        m_PlayerA.SetAudioActive(false);
        m_PlayerA.SetMute(true);
        m_PlayerA.SetVolume(0);
        m_PlayerA.Stop();

        m_PlayerB.SetAudioActive(false);
        m_PlayerB.SetMute(true);
        m_PlayerB.SetVolume(0);
        m_PlayerB.Stop();

        // Un color solido nunca es "video nativo" — si lo activo hasta
        // ahora era eso, apagarlo y revelar "ProjectorLive" de nuevo.
        if (m_ActiveIsNative)
        {
            m_ActiveIsNative = false;
            RetireActiveNative();
        }
    }

    void* BackgroundLayer::GetProcessedTexture(int targetW, int targetH) {
        GLuint rawTex = static_cast<GLuint>(reinterpret_cast<uintptr_t>(Active().GetTextureID()));
        if (rawTex == 0 || targetW <= 0 || targetH <= 0)
            return nullptr;

        if (!m_FSREnabled && !m_NISEnabled)
            return (void*)(uintptr_t)rawTex;

        int srcW = 0, srcH = 0;
        Active().GetVideoSize(srcW, srcH);

        if (srcW <= 0 || srcH <= 0 || (srcW >= targetW && srcH >= targetH))
            return (void*)(uintptr_t)rawTex;

        if (m_FSREnabled) {
            bool needReinit = (!m_FSR.IsInitialized() ||
                               m_FSR.GetOutputW() != targetW ||
                               m_FSR.GetOutputH() != targetH);

            if (needReinit) {
                if (m_FSR.Init(targetW, targetH)) {
                    m_FSR.SetSharpness(m_FSRSharpness);
                    m_FSR.SetEnabled(true);
                } else {
                    m_FSREnabled = false;
                    return (void*)(uintptr_t)rawTex;
                }
            }

            GLuint upscaled = m_FSR.Process(rawTex, srcW, srcH);
            return upscaled ? (void*)(uintptr_t)upscaled : (void*)(uintptr_t)rawTex;
        }

        // m_NISEnabled
        bool needReinit = (!m_NIS.IsInitialized() ||
                           m_NIS.GetOutputW() != targetW ||
                           m_NIS.GetOutputH() != targetH);

        if (needReinit) {
            if (m_NIS.Init(targetW, targetH)) {
                m_NIS.SetSharpness(m_NISSharpness);
                m_NIS.SetEnabled(true);
            } else {
                m_NISEnabled = false;
                return (void*)(uintptr_t)rawTex;
            }
        }

        GLuint upscaled = m_NIS.Process(rawTex, srcW, srcH);
        return upscaled ? (void*)(uintptr_t)upscaled : (void*)(uintptr_t)rawTex;
    }

    void* BackgroundLayer::GetBlurredFillTexture(int workW, int workH) {
        if (!m_FillBlurEnabled) return nullptr;

        GLuint rawTex = static_cast<GLuint>(reinterpret_cast<uintptr_t>(Active().GetTextureID()));
        if (rawTex == 0 || workW <= 0 || workH <= 0) return nullptr;

        // Resolucion de trabajo baja a proposito (1/4, igual que
        // GlassRenderer): esto es un relleno ambiental fuera de foco, no
        // hace falta nitidez ni resolucion real, y sale mucho mas barato.
        int blurW = std::max(1, workW / 4);
        int blurH = std::max(1, workH / 4);

        if (!m_FillBlur.IsInitialized())
            m_FillBlur.Init(blurW, blurH);
        else
            m_FillBlur.Resize(blurW, blurH);

        GLuint blurred = m_FillBlur.Process(rawTex);
        return blurred ? (void*)(uintptr_t)blurred : nullptr;
    }

    void BackgroundLayer::RetireNativePlayback(std::unique_ptr<NativePlayback> np)
    {
        if (!np) return;

        VLCBasePlayer* p = &np->player;
        p->SetAudioActive(false);
        p->SetMute(true);
        p->SetVolume(0);
        np->window.Hide(); // GLFW: hilo principal
        np->retiredAt = NowSeconds();

        // Detach+Stop combinados en UNA accion en el worker (nunca en el
        // hilo principal directo — ver comentario de PreviewLoadWorker.h).
        m_NativeLoader.Request([p]() {
            p->SetAudioActive(false);
            p->DetachNativeWindow();
            p->Stop();
        });

        m_RetiringNative.push_back(std::move(np));
    }

    void BackgroundLayer::RetireActiveNative()
    {
        // Cancela cualquier revelado pendiente: ya no hay "nuevo" que
        // esperar, ambos se retiran.
        m_NativeRevealPending = false;
        if (m_PendingRetireNative) RetireNativePlayback(std::move(m_PendingRetireNative));
        if (m_ActiveNative)        RetireNativePlayback(std::move(m_ActiveNative));
    }

    void BackgroundLayer::PollNativeReveal()
    {
        if (!m_NativeRevealPending || !m_ActiveNative) return;

        bool ready = m_ActiveNative->player.GetLoadState() == VLCBasePlayer::LoadState::Ready;
        bool timedOut = (NowSeconds() - m_NativeRevealStart) > kNativeRevealGiveUpSeconds;
        if (!ready && !timedOut) return;

        // Listo (o se agoto el tiempo de gracia): revelar el nuevo YA —
        // recien ahora, nunca antes, para no exponer el instante de
        // inicializacion del modulo de video de VLC (ver comentario largo
        // de m_NativeRevealPending en el .h).
        if (m_IsLiveToPublic) m_ActiveNative->window.Reveal();
        m_NativeRevealPending = false;

        // El anterior seguia visible (mudo) tapando la transicion — recien
        // ahora se retira de verdad.
        if (m_PendingRetireNative) RetireNativePlayback(std::move(m_PendingRetireNative));
    }

    void BackgroundLayer::SyncNativeWindowVisibility()
    {
        if (!m_ActiveNative) return; // nada cargado por este motor todavia

        // Mostrar/ocultar la ventana (GLFW) se hace aca mismo, en el hilo
        // que llama (siempre el principal) — son llamadas GLFW, tienen que
        // correr ahi. Adjuntar/desvincular la ventana en libVLC (Attach/
        // DetachNativeWindow) y el gate de audio, en cambio, SIEMPRE van
        // combinados en UNA sola accion despachada a m_NativeLoader: nunca
        // deben correr en el hilo principal directo, ni repartidos entre
        // dos pedidos separados al worker (ver el comentario largo en
        // PreviewLoadWorker.h — asi se corrigio un deadlock real).
        if (m_IsLiveToPublic && m_ActiveIsNative)
        {
            // Si esta ventana todavia esta esperando a revelarse (ver
            // m_NativeRevealPending/PollNativeReveal), NO forzar Show() aca
            // — se revelaria antes de tiempo, exponiendo el instante de
            // inicializacion de VLC que todo este mecanismo existe para
            // evitar. Solo se reposiciona/adjunta (CreateHidden, sin
            // mostrar); PollNativeReveal es el UNICO que la muestra.
            void* handle = m_NativeRevealPending
                ? m_ActiveNative->window.CreateHidden(m_LastKnownMonitorIndex)
                : m_ActiveNative->window.Show(m_LastKnownMonitorIndex);
            VLCBasePlayer* p = &m_ActiveNative->player;

            // Gate de audio recalculado ADENTRO del lambda con valores
            // frescos (atomics) al momento de ejecutar, no capturados de
            // antemano — mismo motivo que en SetVideo() (ver su comentario
            // largo: evita que un SetLiveMute()/ApplyAV() posterior quede
            // pisado por un valor viejo).
            m_NativeLoader.Request([this, p, handle]() {
                if (handle) p->AttachNativeWindow(handle);
                bool active = m_ContentAllowsAudio.load(std::memory_order_relaxed);
                bool muted  = m_TargetMuted.load(std::memory_order_relaxed);
                int  volume = m_TargetVolume.load(std::memory_order_relaxed);
                p->SetAudioActive(active);
                p->SetMute(muted || !active);
                p->SetVolume((active && !muted) ? volume : 0);
            });
        }
        else
        {
            m_ActiveNative->window.Hide();
            VLCBasePlayer* p = &m_ActiveNative->player;

            m_NativeLoader.Request([p]() {
                p->SetAudioActive(false);
                p->DetachNativeWindow();
            });
        }
    }

    void BackgroundLayer::SetPubliclyLive(bool live, int monitorIndex)
    {
        m_IsLiveToPublic = live;
        if (monitorIndex >= 0) m_LastKnownMonitorIndex = monitorIndex;

        // Independiente de si lo activo AHORA es nativo o no: sincroniza
        // la ventana nativa (la esconde si live paso a false, o si lo
        // activo no es nativo) y, mas abajo, el audio del path OpenGL de
        // siempre (inofensivo aunque Active()/Standby() no tengan nada
        // relevante cargado en este momento).
        SyncNativeWindowVisibility();

        if (live)
        {
            // Al pasar a "en vivo", el player activo adopta el target de
            // volumen/mute que el operador ya haya configurado (ver
            // SetLiveVolume/SetLiveMute). Sin embargo, si el contenido
            // actualmente cargado no permite audio, debe permanecer mudo.
            bool activeAudioAllowed = m_ContentAllowsAudio.load(std::memory_order_relaxed);
            Active().SetAudioActive(activeAudioAllowed);
            Active().SetMute(m_TargetMuted || !activeAudioAllowed);
            Active().SetVolume(activeAudioAllowed && !m_TargetMuted ? m_TargetVolume.load() : 0);
            Standby().SetAudioActive(false);
            Standby().SetMute(true);
            Standby().SetVolume(0);
        }
        else
        {
            // Cortar audio de raiz en ambos players, sin importar el
            // volumen/mute configurado.
            m_PlayerA.SetAudioActive(false);
            m_PlayerA.SetMute(true);
            m_PlayerA.SetVolume(0);
            m_PlayerB.SetAudioActive(false);
            m_PlayerB.SetMute(true);
            m_PlayerB.SetVolume(0);
            if (m_ActiveNative)
            {
                m_ActiveNative->player.SetAudioActive(false);
                m_ActiveNative->player.SetMute(true);
                m_ActiveNative->player.SetVolume(0);
            }
        }
    }

    void BackgroundLayer::SetLiveVolume(int volume0to200)
    {
        m_TargetVolume = volume0to200;
        if (!m_IsLiveToPublic) return;

        bool allow = m_ContentAllowsAudio.load(std::memory_order_relaxed);
        if (m_ActiveIsNative && m_ActiveNative)
            m_ActiveNative->player.SetVolume((allow && !m_TargetMuted) ? m_TargetVolume.load() : 0);
        else
            Active().SetVolume((allow && !m_TargetMuted) ? m_TargetVolume.load() : 0);
    }

    void BackgroundLayer::SetLiveMute(bool mute)
    {
        m_TargetMuted = mute;
        if (!m_IsLiveToPublic) return;

        bool allow = m_ContentAllowsAudio.load(std::memory_order_relaxed);
        VLCBasePlayer& target = (m_ActiveIsNative && m_ActiveNative) ? m_ActiveNative->player : Active();
        target.SetAudioActive(allow && !mute);
        target.SetMute(mute || !allow);
        target.SetVolume((allow && !mute) ? m_TargetVolume.load() : 0);
    }

    // ── Ecualizador en vivo ──────────────────────────────────────────────

    void BackgroundLayer::ReapplyLiveEqualizer(VLCBasePlayer& target)
    {
        target.SetEqualizerPreamp(m_TargetEqPreamp);
        for (int b = 0; b < VLCBasePlayer::kEqualizerBands; b++)
            target.SetEqualizerBand(b, m_TargetEqBands[b]);
        target.SetEqualizerEnabled(m_TargetEqEnabled);
    }

    void BackgroundLayer::SetLiveEqualizerEnabled(bool enabled)
    {
        m_TargetEqEnabled = enabled;
        if (!m_IsLiveToPublic) return;
        VLCBasePlayer& target = (m_ActiveIsNative && m_ActiveNative) ? m_ActiveNative->player : Active();
        target.SetEqualizerEnabled(enabled);
    }

    void BackgroundLayer::SetLiveEqualizerPreamp(float preampDb)
    {
        m_TargetEqPreamp = preampDb;
        if (!m_IsLiveToPublic) return;
        VLCBasePlayer& target = (m_ActiveIsNative && m_ActiveNative) ? m_ActiveNative->player : Active();
        target.SetEqualizerPreamp(preampDb);
    }

    void BackgroundLayer::SetLiveEqualizerBand(int index, float ampDb)
    {
        if (index < 0 || index >= VLCBasePlayer::kEqualizerBands) return;
        m_TargetEqBands[index] = ampDb;
        if (!m_IsLiveToPublic) return;
        VLCBasePlayer& target = (m_ActiveIsNative && m_ActiveNative) ? m_ActiveNative->player : Active();
        target.SetEqualizerBand(index, ampDb);
    }

    // ── Dispositivo de salida de audio ──────────────────────────────────

    std::vector<VLCBasePlayer::AudioDevice> BackgroundLayer::GetAvailableAudioDevices()
    {
        // Cualquiera de los dos players sirve para enumerar: ambos corren
        // en el mismo proceso y ven los mismos dispositivos del sistema.
        return m_PlayerA.GetAvailableAudioDevices();
    }

    void BackgroundLayer::SetAudioOutputDevice(const std::string& deviceId)
    {
        m_AudioDeviceId = deviceId;

        // Se aplica a m_PlayerA/B (el standby puede pasar a ser el activo
        // en cualquier momento via PerformSwap()) y, si hay uno cargado
        // ahora, al reproductor nativo actual — m_AudioDeviceId tambien
        // queda guardado para que SetVideo() lo aplique a cada
        // NativePlayback nuevo que cree de ahi en mas (ver su motor
        // "libvlc": cada clip usa un reproductor fresco, no reutilizado).
        m_PlayerA.SetAudioDevice(deviceId);
        m_PlayerB.SetAudioDevice(deviceId);
        if (m_ActiveNative) m_ActiveNative->player.SetAudioDevice(deviceId);
    }

    void BackgroundLayer::BlockPath(const std::string& path)
    {
        m_PlayerA.BlockPath(path);
        m_PlayerB.BlockPath(path);
    }

    void BackgroundLayer::UnblockPath()
    {
        m_PlayerA.UnblockPath();
        m_PlayerB.UnblockPath();
    }

} // namespace ProyecThor::Core