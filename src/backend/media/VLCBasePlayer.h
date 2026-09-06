#pragma once
#include <string>
#include <cstdint>
#include <vector>
#include <atomic>
#include <mutex>

struct libvlc_instance_t;
struct libvlc_media_player_t;
struct libvlc_event_t;

namespace ProyecThor::Core {

    class VLCBasePlayer {
    public:

        struct AudioDevice {
            std::string id;
            std::string description;
        };

        // useHardwareDecode controla si esta instancia usa el decodificador
        // de hardware de la GPU (D3D11VA/DXVA2 en Windows, VAAPI/VDPAU en
        // Linux, autodetectado via "--avcodec-hw=any") o decode por
        // software. Se expone como parametro de construccion para poder
        // diagnosticar contencion de sesiones de decode de hardware.
        //
        // forceSilent: garantia estructural de silencio, fijada UNA sola
        // vez al construir el player y valida durante toda su vida. Un
        // player con forceSilent=true NUNCA puede emitir audio real, sin
        // importar que SetMute/SetVolume/SetAudioActive se llamen con
        // valores "audibles" desde cualquier parte del codigo (boton mal
        // cableado, swap de doble buffer, etc.). Se usa para el player de
        // preview (biblioteca), que por requisito de producto jamas debe
        // sonar: solo el monitor a publico puede tener audio real.
        //
        // nativeWindowOutput: cuando es true, este player NUNCA registra
        // los callbacks vmem (lock/unlock/display) ni crea el buffer de
        // textura — esta pensado para adjuntarse a una ventana nativa via
        // AttachNativeWindow() y dejar que libVLC dibuje el video con su
        // propio renderer acelerado (Direct3D/XVideo), en vez de la copia
        // CPU→textura→GL que usa el modo normal. Ver motor de renderizado
        // "VLC (ventana nativa)" en Ajustes > Proyeccion.
        VLCBasePlayer(int decodeThreads = 0, bool useHardwareDecode = true,
                     bool forceSilent = false, bool nativeWindowOutput = false);
        ~VLCBasePlayer();

        VLCBasePlayer(const VLCBasePlayer&)            = delete;
        VLCBasePlayer& operator=(const VLCBasePlayer&) = delete;

        // NOTA: sincronico. Se ejecuta en el hilo que llama a Play(), sin
        // hilo de fondo propio. Si el archivo tarda en abrir (disco lento,
        // red, o resolucion de YouTube via yt-dlp), el hilo llamante se
        // bloquea durante ese lapso. Es un evento puntual al cambiar de
        // clip, no una carga sostenida por frame.
        void Play(const std::string& path, bool loop = false, bool startMuted = false);
        void Stop();

        void BlockPath(const std::string& path);
        void UnblockPath();

        // En Windows devuelve los picos calculados a partir de los samples
        // interceptados manualmente (WinMM). En Linux, donde el audio lo
        // maneja la salida nativa de libVLC (Pulse/ALSA autodetectado),
        // no hay acceso a los samples crudos, asi que devuelve 0.0f/0.0f.
        void GetAudioLevels(float& left, float& right);

        void SetPause(bool paused);
        bool IsPaused() const { return m_Paused.load(std::memory_order_relaxed); }

        // Si el player es forceSilent, estas tres funciones siguen
        // aceptando el valor pedido (para no romper a quien las llama,
        // ej. sliders de UI), pero el resultado audible real queda
        // siempre en silencio. Ver detalle en VLCBasePlayer.cpp.
        void SetMute(bool mute);
        void SetVolume(int volume);   // 0-200
        void SetSoftwareVolume(float percent);

        // En Windows corta la salida de audio real (HWAVEOUT) de raiz: el
        // callback de audio de VLC retorna de inmediato sin tocar el
        // dispositivo ni hacer busy-wait sobre los buffers. En Linux, sin
        // callback custom de audio, esto se traduce a mute/volumen 0 via
        // libVLC nativo (ver .cpp).
        void SetAudioActive(bool active);
        void EnforceSilenceIfNeeded();
        bool IsForceSilent() const { return m_ForceSilent.load(std::memory_order_relaxed); }

        // Escape de la garantia "fijada una sola vez" de arriba -- SOLO
        // para el player de Preview de biblioteca, que por pedido explicito
        // puede sonar si el operador lo activa a mano (opt-in, apagado por
        // default para no duplicar audio al escuchar Preview mientras algo
        // ya suena en vivo). Los demas players forceSilent (BackgroundLayer,
        // NativePlayback) siguen sin exponer esto -- nadie los llama.
        void SetForceSilent(bool v) { m_ForceSilent.store(v, std::memory_order_relaxed); }

        // Ecualizador de 10 bandas (libvlc_audio_equalizer_*, ver
        // VLCBasePlayer.cpp). enabled=false quita el filtro por completo
        // (libvlc_media_player_set_equalizer(nullptr)) en vez de dejarlo
        // aplicado con valores en 0. Los cambios de banda/preamp reconstruyen
        // y reaplican el ecualizador completo, mismo criterio que ya usa
        // AudioPanel::RenderEqualizerSection (Audio.cpp).
        static constexpr int kEqualizerBands = 10;
        void SetEqualizerEnabled(bool enabled);
        void SetEqualizerPreamp(float preampDb);
        void SetEqualizerBand(int index, float ampDb);
        bool IsEqualizerEnabled() const { return m_EqEnabled; }

        void SetPosition(float pos);

        int64_t GetTime() const;
        int64_t GetLength() const;

        void* GetTextureID();
        void  GetVideoSize(int& width, int& height);
        // Devuelve true si esta llamada realmente subio un frame NUEVO a
        // GL (false si no habia nada pendiente que subir todavia) — usado
        // por BackgroundLayer para contar frames reales del standby antes
        // de empezar a mostrarlo en el crossfade (ver kSwapSettleFrames).
        bool  UpdateTexture();

        // true si ya se decodifico al menos un frame de video real.
        bool HasVideoFrame() const;

        // Solo tiene efecto en un player construido con nativeWindowOutput
        // = true (ver constructor). Adjunta/desvincula la salida de video
        // de este reproductor a una ventana nativa (HWND en Windows, X11
        // Window en Linux) para que libVLC dibuje ahi directo con su
        // propio renderer. Segun la doc de libVLC, el cambio toma efecto
        // recien cuando arranca la reproduccion — no tiene efecto
        // instantaneo sobre un clip que ya esta reproduciendose.
        void AttachNativeWindow(void* nativeHandle);
        void DetachNativeWindow();

        // Enumera los dispositivos de salida de audio disponibles.
        // - Windows: enumera dispositivos WinMM reales via
        //   waveOutGetNumDevs()/waveOutGetDevCaps(), incluyendo siempre
        //   un primer item "default" (WAVE_MAPPER = dispositivo
        //   predeterminado del sistema). No depende de que haya un media
        //   cargado.
        // - Linux/macOS: delega en libvlc_audio_output_device_enum(),
        //   que si necesita que el media player exista (no necesariamente
        //   reproduciendo).
        std::vector<AudioDevice> GetAvailableAudioDevices();

        // Selecciona el dispositivo de salida de audio para este player.
        // deviceId vacio o "default" selecciona el dispositivo
        // predeterminado del sistema.
        //
        // - Windows: el audio de este player pasa por una salida WinMM
        //   propia (ver vlc_audio_play en el .cpp), asi que aca cerramos
        //   y reabrimos el HWAVEOUT en el dispositivo pedido. Si se llama
        //   antes de la primera reproduccion, el dispositivo se recuerda
        //   y se abre directamente en ese ID cuando arranque el audio.
        // - Linux/macOS: delega en libvlc_audio_output_device_set() sobre
        //   la salida nativa de libVLC. Ademas, el ID se recuerda y se
        //   reaplica automaticamente en cada Play() (LoadAndPlay), porque
        //   libVLC puede resetear el device seleccionado al cargar un
        //   nuevo medio.
        void SetAudioDevice(const std::string& deviceId);
        std::string GetCurrentAudioDeviceId() const { return m_AudioDeviceId; }

        // Ruta que esta activa o cargando en este momento en esta
        // instancia (vacio si esta detenida). Usado por BackgroundLayer
        // para detectar pedidos redundantes de reproducir lo que ya se
        // esta mostrando (ver SetVideo()).
        const std::string& GetCurrentPath() const { return m_CurrentPath; }

        // Estado de carga real, derivado del evento libvlc_MediaPlayerPlaying
        // (hilo interno de libVLC) + HasVideoFrame() (primer frame de video
        // ya decodificado). Antes IsLoading() era un stub que devolvia
        // false siempre — BackgroundLayer::Update() lo consultaba creyendo
        // que reflejaba el estado real, asi que el gate de "esta listo el
        // standby" corria solo a medias (ver HasVideoFrame() mas abajo).
        enum class LoadState { Idle, Opening, Buffering, Ready, Error };
        LoadState GetLoadState() const;
        bool IsLoading() const;

        bool ConsumeEndReached();

        // true si el ultimo ConsumeEndReached() vino de un error real
        // (libvlc_MediaPlayerEncounteredError: codec no soportado, archivo
        // corrupto, etc.) y no de un fin de clip normal. Se consume (se
        // resetea a false) al leerlo, igual que ConsumeEndReached().
        bool ConsumeHadError();

    private:

        int  m_DecodeThreads    = 0;
        bool m_UseHardwareDecode = true;
        bool m_NativeWindowOutput = false;

        libvlc_instance_t*       m_Instance    = nullptr;
        libvlc_media_player_t*   m_MediaPlayer = nullptr;
        void* m_VideoCtx = nullptr;
        void* m_AudioCtx = nullptr;

        mutable std::mutex m_MediaSwapMutex;

        std::atomic<float> m_VolumeMultiplier{1.0f};
        std::atomic<bool>  m_Muted{false};
        std::atomic<bool>  m_EndReached{false};
        std::atomic<bool>  m_HadError{false};
        std::atomic<bool>  m_Paused{false};
        std::atomic<bool>  m_AudioActive{true};
        std::atomic<bool>  m_ForceSilent{false};

        // Ecualizador — ver SetEqualizerEnabled/Preamp/Band.
        bool  m_EqEnabled = false;
        float m_EqPreamp  = 0.0f;
        float m_EqBands[kEqualizerBands] = { 0.0f };
        void  ApplyEqualizer();

        // Backing de LoadState/IsLoading (ver GetLoadState() en el .cpp):
        // m_VlcIsPlaying refleja el evento libvlc_MediaPlayerPlaying del
        // load EN CURSO (se resetea a false en cada LoadAndPlay), separado
        // de m_HadError (que ConsumeHadError() consume para EndReached)
        // para no pisar esa semantica existente.
        std::atomic<bool>  m_HasEverPlayed{false};
        std::atomic<bool>  m_VlcIsPlaying{false};
        std::atomic<bool>  m_LoadHasError{false};
        unsigned int m_TextureID = 0;
        int          m_VideoW    = 0;
        int          m_VideoH    = 0;

        // Dispositivo de salida de audio actualmente seleccionado (vacio =
        // predeterminado del sistema). Se recuerda aca (y no solo en el
        // ctx nativo) para poder reaplicarlo tras cada Play()/reload.
        std::string m_AudioDeviceId;

        bool                    m_PathBlocked       = false;
        std::string             m_BlockedPath;

        // Ruta actualmente activa o cargando en ESTA instancia. Vacio si el
        // player esta detenido (Stop()) o nunca reprodujo nada. Ver guard de
        // reentrancia en Play().
        std::string m_CurrentPath;

        std::atomic<uint64_t> m_LoadGeneration{0};
        int m_InstanceId = -1;
        void InitVLC();
        void DestroyVLC();
        void EnsureTexture(int w, int h);
        void CreatePersistentPlayer();

        void LoadAndPlay(const std::string& path, bool loop, bool startMuted, uint64_t myGeneration);

        static void OnVlcEvent(const libvlc_event_t* evt, void* userData);
    };

} // namespace ProyecThor::Core