#pragma once
#include <GL/glew.h>
#include "backend/media/VLCBasePlayer.h"
#include "backend/shaders/PostProcessorFSR.h"
#include "backend/shaders/PostProcessorNIS.h"
#include "backend/shaders/BackgroundFillBlur.h"
#include "backend/core/PreviewLoadWorker.h"
#include "frontend/windowing/NativeVideoOutputWindow.h"
#include <string>
#include <vector>
#include <deque>
#include <memory>
#include <atomic>
#include <algorithm>

namespace ProyecThor::Core {

    class BackgroundLayer {
    private:
        VLCBasePlayer m_PlayerA;
        VLCBasePlayer m_PlayerB;
        bool          m_ActiveIsA = true;

        bool   m_SwapPending      = false;
        double m_PendingSwapStart = 0.0;

        // Momento (NowSeconds()) en que Standby() quedo Ready durante un
        // swap pendiente — 0.0 mientras no lo esta. Desde ahi se cuenta un
        // crossfade corto y fijo (kSwapBlendSeconds) antes de completar el
        // swap de verdad, para que el cambio de fondo se vea como una
        // transicion fluida (frame final -> frame inicial) en vez de un
        // corte seco. Ver Update().
        double m_SwapReadyAt = 0.0;
        // Momento en que se cumplio el asentamiento (kSwapSettleSeconds
        // despues de Ready) — 0.0 hasta entonces. Desde ahi se cuenta el
        // crossfade real (kSwapBlendSeconds).
        double m_SwapSettledAt = 0.0;

        // Margen de "asentamiento": los primeros frames decodificados de un
        // codec recien abierto a veces son artefactos del decoder
        // "calentando" (frame parcial/negro/con colores mal, comun en
        // hardware decode o con B-frames) — HasVideoFrame() ya da true con
        // el primer frame, que puede ser justo uno de esos. Sin esperar un
        // poco antes de empezar a MOSTRAR standby en el blend, el publico
        // podia ver un flash breve de ese frame roto. Este margen NO
        // demora el swap final (sigue siendo kSwapBlendSeconds despues de
        // asentar) — solo demora el INICIO del blend visible. El mismo
        // margen tambien se usa antes de PAUSAR el prefetch (ver Update()),
        // asi que cuando llega a esta parte el standby ya viene de un
        // frame confirmado estable, no solo del primero que aparecio.
        static constexpr double kSwapSettleSeconds = 0.3;

        // Duracion del crossfade real (ya no es fija): configurable desde
        // Diseño > Transiciones cuando un preset tiene "Afecta a Fondos"
        // activo (ver SetBlendSeconds / PresentationCore::
        // SetBackgroundBlendDuration) -- 0.2s es el default de siempre,
        // usado mientras ningun preset la toca.
        double m_BlendSeconds = 0.2;
        // Mismo rol que m_SwapReadyAt pero para el prefetch (ver Update()):
        // momento en que el prefetch quedo Ready por primera vez, para
        // saber cuando ya paso kSwapSettleSeconds y es seguro pausarlo.
        double m_PrefetchReadyAt = 0.0;
        // Limite de emergencia: si standby no llega a Ready ni a Error en
        // este tiempo (carga realmente colgada), se abandona el swap y se
        // sigue mostrando el fondo anterior — nunca se fuerza un corte a
        // contenido que no esta listo (ver Update()).
        static constexpr double kSwapGiveUpSeconds = 15.0;

        // Precarga adelantada sin swap automatico (ver Prefetch()/
        // CommitPrefetch()) — usada por la cola del Monitor para dejar el
        // SIGUIENTE clip abierto y pausado en su primer frame mientras el
        // actual todavia se esta reproduciendo, para que la transicion en
        // CommitPrefetch() sea un corte instantaneo (nada que esperar) en
        // vez de recien empezar a abrir el archivo en ese momento.
        // Separado de m_SwapPending: Prefetch() llena standby pero NO arma
        // el gate de Update(), asi que no se dispara solo.
        bool        m_PrefetchArmed = false;
        std::string m_PrefetchedPath;

        // Historial de cuanto tardo en quedar listo (Ready) el ultimo
        // puñado de swaps, para poder mostrarle al operador un tiempo
        // estimado de carga (ver GetEstimatedLoadSeconds()).
        std::deque<float> m_RecentLoadDurations;
        static constexpr size_t kMaxLoadSamples = 8;
        static constexpr float  kDefaultEtaSeconds = 1.5f;

        // Atomics (no simples bool/int): el motor libvlc los lee desde el
        // hilo de m_NativeLoader (ver SetVideo/SyncNativeWindowVisibility)
        // para recalcular el gate de audio real EN EL MOMENTO en que Play()
        // termina de correr, no con un valor viejo capturado al encolar —
        // sin esto, un SetLiveMute()/ApplyAV() que corriera en el hilo
        // principal DESPUES de encolar pero ANTES de que el worker
        // terminara, quedaba pisado por el valor viejo (bug real: video
        // quedaba mudo pese a haberse desmuteado).
        std::atomic<int>  m_TargetVolume{100};
        std::atomic<bool> m_TargetMuted{true};

        // ── Ecualizador en vivo (ver SetLiveEqualizer*) ──────────────────
        bool  m_TargetEqEnabled = false;
        float m_TargetEqPreamp  = 0.0f;
        float m_TargetEqBands[VLCBasePlayer::kEqualizerBands] = { 0.0f };
        void  ReapplyLiveEqualizer(VLCBasePlayer& target);
        float m_TransitionProgress = 1.0f;

        // Gate real de audio al publico. Solo cuando esta en true el
        // player activo puede sonar de verdad (ver SetPubliclyLive). Sin
        // esto, cargar un video de fondo (SetVideo) o mover el doble
        // buffer (PerformSwap) podia dejar audio sonando sin que el
        // operador hubiese puesto nada al aire todavia.
        std::atomic<bool> m_IsLiveToPublic{false};

        // Indica si el contenido actualmente cargado (o pendiente de swap)
        // tiene PERMITIDO sonar cuando m_IsLiveToPublic sea true. Se fija
        // en cada llamada a SetVideo() segun quien la invoque:
        // true  -> viene de "Videos"/cola (audio permitido)
        // false -> viene de "Fondos" (BackgroundsPanel/LayersBgTab), NUNCA
        //          suena sin importar el estado de m_IsLiveToPublic.
        std::atomic<bool> m_ContentAllowsAudio{false};

        // Dispositivo de salida de audio seleccionado por el operador
        // (vacio o "default" = predeterminado del sistema). Se aplica a
        // AMBOS players (m_PlayerA y m_PlayerB) apenas se selecciona, para
        // que no importe cual este activo hoy ni cual pase a estarlo tras
        // un swap: el audio siempre sale por este dispositivo.
        std::string m_AudioDeviceId;

#ifdef _WIN32
    bool m_FlipVideoY = false;
#else
    bool m_FlipVideoY = true;  // default: VAAPI en Linux suele invertir
#endif
        bool  m_IsVideo = false;
        float m_BgColor[3] = { 0.0f, 0.0f, 0.0f };

        ProyecThor::Shaders::PostProcessorFSR m_FSR;
        bool  m_FSREnabled   = true;
        float m_FSRSharpness = 0.2f;

        // Escalador alternativo exclusivo de NVIDIA (ver PostProcessorNIS.h).
        // Mutuamente excluyente con FSR -- la UI (ShadersPanel) y
        // PresentationCore::SetNISEnabled se encargan de que activar uno
        // apague el otro, aca simplemente se corre el que este habilitado.
        ProyecThor::Shaders::PostProcessorNIS m_NIS;
        bool  m_NISEnabled   = false;
        float m_NISSharpness = 0.5f;

        // "Rellenado" de las barras de letterbox/pillarbox: ver
        // GetBlurredFillTexture(). Vive aca (no en CompositePostChain) por
        // la misma razon que FSR -- necesita el contenido de fondo SIN
        // componer todavia, no el frame final ya con overlays/texto encima.
        ProyecThor::Shaders::BackgroundFillBlur m_FillBlur;
        bool  m_FillBlurEnabled    = false;
        // Cuanto brilla el relleno: 0 = negro (bordes practicamente
        // invisibles, como antes), 1 = brillo real del fondo desenfocado.
        // Pedido explicito: a algunos les gusta el relleno pero mas oscuro
        // -- por eso el default no es 1.0.
        float m_FillBlurBrightness = 0.6f;

        bool  m_StretchToFill = true;

        // ── Ping-pong "bucle falso" (ver Ajustes > Proyeccion > Fondos) ───
        // Solo aplica a Fondos (allowAudio=false) -- nunca a Videos/cola.
        // libVLC no soporta reproduccion en reversa real de forma confiable,
        // asi que la ilusion se logra a pura fuerza de SetPosition(): al
        // terminar el pase hacia adelante (EndReached real, ver SetVideo(),
        // que para este caso carga SIN input-repeat cuando esto esta
        // activo) se pausa el player y se lo hace "retroceder" a pasos,
        // hasta cerca del inicio, donde se reanuda hacia adelante de nuevo.
        // Preferencia persistida (Ajustes); estado en tiempo real de en que
        // fase esta el pase actual, ver Update().
        bool   m_PingPongEnabled    = false;
        bool   m_PingPongReverse    = false;
        double m_PingPongLastStepAt = 0.0;
        static constexpr double kPingPongStepSeconds = 0.15;

        // ── Motor de renderizado alternativo: "libvlc (ventana nativa)" ──
        // Aplica SOLO a contenido de VIDEO real (allowAudio=true — Videos/
        // cola del Monitor), nunca a Fondos/imagenes/color solido: esos
        // siempre necesitan overlays/texto encima y por lo tanto siempre
        // van por el compositor OpenGL de siempre (Active()/Standby()),
        // sin importar este ajuste. m_UseNativeEngine es la preferencia
        // configurada (Ajustes > Proyeccion); m_ActiveIsNative es el
        // estado real de "lo que esta reproduciendose AHORA vino por el
        // camino nativo", que es lo que deciden Update()/Render() y el
        // resto de los getters de audio.
        //
        // NativePlayback = un reproductor + su propia ventana nativa,
        // creados juntos para UN SOLO clip. FIX importante (confirmado en
        // la practica en Windows, con Wine reportando un deadlock real
        // entre dos hilos): reusar el MISMO reproductor+ventana para el
        // SIGUIENTE clip (Stop() + set_media() + play() sobre una ventana
        // ya adjuntada) hace que el modulo de video de VLC se cuelgue —
        // andaba el primer clip, se trababa desde el segundo, sin importar
        // que tan cuidadosamente se serializara Play()/Stop()/Attach/
        // Detach en un solo hilo. La solucion es no reusar nunca ese par:
        // cada clip nuevo arranca 100% de cero (jugador + ventana nuevos),
        // y el par anterior se retira (detach+stop, encolado en
        // m_NativeLoader) y se destruye un rato despues (ver
        // m_RetiringNative/kNativeRetireSeconds en Update()), nunca
        // reutilizado ni vuelto a reproducir.
        struct NativePlayback {
            VLCBasePlayer           player;
            NativeVideoOutputWindow window;
            double                  retiredAt = 0.0; // 0 = todavia activo, no retirado

            explicit NativePlayback(bool forceSilentAudio)
                : player(2, false, forceSilentAudio, /*nativeWindowOutput=*/true) {}
        };

        bool m_UseNativeEngine = false;
        bool m_ActiveIsNative  = false;
        int  m_LastKnownMonitorIndex = -1;
        bool m_ForceSilentAudio = false; // recordado para poder crear NativePlayback mas adelante

        // El par en uso ahora mismo (nullptr si nunca se cargo nada por
        // este motor todavia). Los que ya se retiraron (ver arriba) viven
        // un rato en m_RetiringNative hasta que Update() los destruye.
        std::unique_ptr<NativePlayback>              m_ActiveNative;
        std::vector<std::unique_ptr<NativePlayback>> m_RetiringNative;
        static constexpr double kNativeRetireSeconds = 2.0;

        // ── Revelado diferido de la ventana nueva ────────────────────────
        // FIX (flash blanco durante el cambio de clip): la pintada en negro
        // de NativeVideoOutputWindow (ver su .cpp) cubre el fondo de LA
        // VENTANA, pero no lo que el propio modulo de video de VLC
        // (Direct3D9/11, XVideo) pinte encima una vez adjuntado — eso es
        // otro nivel, fuera de nuestro control, y confirmado en la
        // practica que puede mostrar blanco un instante antes de su
        // primer frame real. La unica forma de garantizar que eso NUNCA
        // se vea es no mostrar la ventana nueva hasta confirmar que el
        // reproductor nuevo ya esta reproduciendo de verdad (GetLoadState
        // == Ready) — mientras tanto, el reproductor+ventana ANTERIOR
        // (m_PendingRetireNative) se deja seguir visible (mudo, ver
        // SetVideo) cubriendo la transicion, y recien se retira cuando el
        // nuevo se revela. kNativeRevealGiveUpSeconds es una red de
        // seguridad: si un archivo roto nunca llega a Ready, se revela
        // igual pasado ese tiempo en vez de quedarse en el clip anterior
        // para siempre.
        std::unique_ptr<NativePlayback> m_PendingRetireNative;
        bool   m_NativeRevealPending = false;
        double m_NativeRevealStart   = 0.0;
        double m_LastNativeSyncTime  = 0.0;
        static constexpr double kNativeRevealGiveUpSeconds = 4.0;

        // Despacha TODA operacion sobre un NativePlayback::player en un
        // hilo aparte (Play/Stop/Attach/DetachNativeWindow) — NUNCA
        // llamarlas directo desde el hilo principal: es el mismo hilo que
        // bombea los mensajes de la ventana nativa, y hacerlo ahi podia
        // colgar toda la app. Ver PreviewLoadWorker.h para el detalle
        // completo (pese al nombre, es generico) — critico tambien: cada
        // pedido combina TODO lo que tiene que pasar en un orden dado en
        // UNA sola accion (nunca dos Request() sueltos para una misma
        // transicion), porque la politica "ultimo pedido gana" podria
        // descartar uno de los dos si se llamaran por separado.
        PreviewLoadWorker m_NativeLoader;

        // Retira un NativePlayback dado: oculta su ventana, encola
        // detach+stop en m_NativeLoader, y lo pasa a m_RetiringNative para
        // que Update() lo destruya mas tarde.
        void RetireNativePlayback(std::unique_ptr<NativePlayback> np);

        // Retira m_ActiveNative Y (si lo hubiera) m_PendingRetireNative,
        // cancelando cualquier revelado pendiente — usar para apagar el
        // motor nativo por completo (vuelta a OpenGL / color solido). No
        // crea nada nuevo.
        void RetireActiveNative();

        // Sondea si el NativePlayback en m_ActiveNative (mientras
        // m_NativeRevealPending sea true) ya esta listo para revelarse —
        // ver comentario largo de m_NativeRevealPending arriba. Llamar una
        // vez por frame desde Update(), sin importar el motor actual.
        void PollNativeReveal();

        // Muestra/adjunta o esconde/desvincula la ventana de m_ActiveNative
        // segun m_IsLiveToPublic && m_ActiveIsNative (llamar despues de
        // cambiar cualquiera de esos dos). No-op si m_ActiveNative es null.
        void SyncNativeWindowVisibility();

        VLCBasePlayer& Active();
        VLCBasePlayer& Standby();
        void PerformSwap();

    public:
        // forceSilentAudio=true construye ambos players internos como
        // permanentemente mudos (ver VLCBasePlayer::m_ForceSilent). Se usa
        // para la instancia de preview de biblioteca, que por requisito
        // de producto NUNCA debe emitir audio, sin importar que boton la
        // toque.
        explicit BackgroundLayer(bool forceSilentAudio = false);
        ~BackgroundLayer() = default;

        void SetFlipVideoY(bool flip) { m_FlipVideoY = flip; }
        bool GetFlipVideoY() const { return m_FlipVideoY; }

        void Update();
        void Render(int outputW, int outputH);

        // Dibuja una imagen (el logo de pantalla de carga, ver
        // PresentationCore::ShouldShowLoadingScreen) letterboxeada dentro
        // del viewport de salida, reusando el mismo blit raw-GL que el
        // fondo normal (BlitTexture) — para la salida REAL al publico,
        // llamado en vez de Render() mientras el logo esta activo.
        void RenderLogo(unsigned int logoTex, int logoW, int logoH, int outputW, int outputH);

        void  SetFSREnabled(bool enabled);
        bool  GetFSREnabled() const;
        void  SetFSRSharpness(float sharpness);
        float GetFSRSharpness() const;

        void  SetNISEnabled(bool enabled);
        bool  GetNISEnabled() const;
        void  SetNISSharpness(float sharpness);
        float GetNISSharpness() const;

        // "Rellenado": en vez de barras negras de letterbox/pillarbox,
        // llena ese espacio con el mismo contenido estirado a pantalla
        // completa y muy desenfocado, detras del contenido nitido -- ver
        // GetBlurredFillTexture() y el uso en UIManager.cpp.
        void SetFillBlurEnabled(bool enabled) { m_FillBlurEnabled = enabled; }
        bool GetFillBlurEnabled() const       { return m_FillBlurEnabled; }
        void  SetFillBlurBrightness(float v)  { m_FillBlurBrightness = std::clamp(v, 0.0f, 1.0f); }
        float GetFillBlurBrightness() const   { return m_FillBlurBrightness; }
        void* GetBlurredFillTexture(int workW, int workH);

        void  SetStretchToFill(bool stretch);
        bool  GetStretchToFill() const;

        // "Bucle falso" de Fondos: en vez de repetir siempre desde el mismo
        // frame 0 (corte visible), reproduce hacia adelante y despues
        // "hacia atras" (scrub por SetPosition, ver comentario del miembro
        // arriba), dando sensacion de bucle continuo. Solo tiene efecto en
        // contenido de Fondos (allowAudio=false); Videos/cola lo ignoran
        // por completo. Cambiarlo mientras un Fondo ya esta cargado no
        // afecta al pase en curso -- se aplica recien en el proximo
        // SetVideo()/CommitPrefetch() (el mismo criterio que ya usan
        // m_UseNativeEngine/m_StretchToFill para ajustes que solo pueden
        // tomarse al abrir el archivo).
        void SetPingPongLoop(bool enabled) { m_PingPongEnabled = enabled; }
        bool GetPingPongLoop() const { return m_PingPongEnabled; }

        // Preferencia de motor para VIDEOS reales (allowAudio=true): false
        // (default) = compuesto OpenGL de siempre; true = libvlc en
        // ventana nativa (ver comentario del miembro m_UseNativeEngine).
        // Fondos/imagenes/color solido SIEMPRE van por OpenGL, ignoran
        // esto por completo. Cambiarlo mientras un video ya esta
        // reproduciendose no tiene efecto instantaneo (asi lo documenta
        // libVLC) — recien se aplica en el proximo SetVideo()/
        // CommitPrefetch() real.
        void SetUseNativeEngine(bool useNative) { m_UseNativeEngine = useNative; }
        bool GetUseNativeEngine() const { return m_UseNativeEngine; }
        void* GetProcessedTexture(int targetW, int targetH);
        void* GetTextureID();

        // Textura cruda de Standby() (sin FSR) + progreso de blend, para que
        // un rendering ImGui (ver UIManager "ProjectorLive") pueda blendear
        // el mismo crossfade que ya hace Render() via BlitTexture, sin
        // duplicar la logica de decidir CUANDO blendear (ver IsSwapPending/
        // HasVideoFrame ya expuestos).
        void*  GetStandbyTextureID();
        float  GetTransitionProgress() const { return m_TransitionProgress; }
        bool   StandbyHasFrame();

        // Reproductor con el contenido REALMENTE activo ahora mismo — el
        // par OpenGL de siempre, o el reproductor de m_ActiveNative si el
        // video actual esta usando el motor libvlc (ver m_ActiveIsNative).
        // Este es el punto que usan la cola del Monitor y los controles de
        // transporte para llegar al reproductor correcto sin importar el
        // motor.
        VLCBasePlayer* GetPlayer();
        void SeekSync(float pos);
        void GetActiveVideoSize(int& width, int& height);

        void SetTransitionProgress(float p) { m_TransitionProgress = std::clamp(p, 0.0f, 1.0f); }

        // Duracion del crossfade de swap (ver m_BlendSeconds arriba).
        void SetBlendSeconds(double s) { m_BlendSeconds = std::max(0.01, s); }

        // allowAudio=false para fondos decorativos (BackgroundsPanel):
        // estructuralmente no podran sonar aunque se este "al aire". Default
        // false: el caller tiene que pedir audio explicitamente (monitor).
        void SetVideo(const std::string& path, bool allowAudio = false);

        // Precarga path en standby, pausado apenas decodifica su primer
        // frame (ver el guard de pausa en Update()) — SIN armar el swap
        // automatico (a diferencia de SetVideo()). Llamar a
        // CommitPrefetch() cuando corresponda mostrarlo: como ya esta
        // listo y quieto en el frame 0, el corte es instantaneo.
        void Prefetch(const std::string& path, bool allowAudio = false);

        // Arma el swap para lo que ya este precargado via Prefetch() (lo
        // despausa en el instante del swap). Si no hay nada precargado (o
        // no coincide, ej. la cola se reordeno), cae a SetVideo(path,...)
        // como carga en frio normal.
        void CommitPrefetch(const std::string& path, bool allowAudio = false);

        bool  IsSwapPending() const { return m_SwapPending; }
        float GetEstimatedLoadSeconds() const;

        void SetSolidColor(float r, float g, float b);

        // Activa/desactiva el gate de audio al publico. Llamado por
        // PresentationCore::SetProjecting(). Al pasar a false, el audio
        // se corta de inmediato en ambos players (activo y standby), sin
        // importar el volumen/mute configurado.
        //
        // monitorIndex solo se usa cuando GetUseNativeEngine() es true:
        // es el monitor donde mostrar/ocultar la ventana nativa de video
        // (ver m_NativeWindow). -1 = no tocar la ventana (compatibilidad
        // con el motor OpenGL, que lo ignora de todos modos).
        void SetPubliclyLive(bool live, int monitorIndex = -1);
        bool IsPubliclyLive() const { return m_IsLiveToPublic; }
        bool GetContentAllowsAudio() const { return m_ContentAllowsAudio.load(std::memory_order_relaxed); }

        void SetLiveVolume(int volume0to200);
        void SetLiveMute(bool mute);

        // Ecualizador de 10 bandas sobre el audio en vivo (ver
        // VLCBasePlayer::SetEqualizer*). Mismo criterio que SetLiveVolume/
        // SetLiveMute: se guarda como "target" y se reaplica en cada swap
        // del doble buffer para sobrevivir al crossfade.
        void SetLiveEqualizerEnabled(bool enabled);
        void SetLiveEqualizerPreamp(float preampDb);
        void SetLiveEqualizerBand(int index, float ampDb);

        // ── Dispositivo de salida de audio ───────────────────────────────
        // Enumera los dispositivos de audio disponibles en el sistema
        // (altavoces, HDMI, interfaces USB, etc.) para mostrarlos en un
        // combo/selector de UI.
        std::vector<VLCBasePlayer::AudioDevice> GetAvailableAudioDevices();

        // Selecciona el dispositivo por el que debe salir el audio del
        // fondo. deviceId vacio o "default" usa el dispositivo
        // predeterminado del sistema. Se aplica de inmediato a ambos
        // players internos (activo y standby), asi que el cambio tiene
        // efecto sin importar que este sonando en este momento.
        void SetAudioOutputDevice(const std::string& deviceId);
        std::string GetAudioOutputDevice() const { return m_AudioDeviceId; }

        void BlockPath(const std::string& path);
        void UnblockPath();
    };

} // namespace ProyecThor::Core