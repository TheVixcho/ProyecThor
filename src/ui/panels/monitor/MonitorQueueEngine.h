#pragma once
#include <string>
#include <vector>

namespace ProyecThor::UI {

enum class QueueState { Stopped, Playing };

// Motor de la cola. Regla unica: al reproducirse, recorre TODOS los items
// en orden, uno por uno, sin loops y sin repetir ninguno. El avance es
// estrictamente por evento (VLC reporta fin de clip), nunca por tiempo.
// Arranque directo, sin preflight (el primer item se carga como cualquier
// otro). Mientras un item reproduce, se precarga el siguiente en segundo
// plano (ver Update()/PreloadNextIfNeeded) para que la transicion entre
// clips sea un corte instantaneo.
class MonitorQueueEngine {
public:
    void Load();
    void Save() const;

    void Add(const std::string& fullPath);
    void AddURL(const std::string& url);

    void Move(int from, int to);
    void Remove(int index);
    void Clear();

    // Arranca en "index" y, si la entrada es invalida, salta sola hacia
    // adelante hasta encontrar una valida (o detiene si no queda ninguna).
    void PlayIndex(int index);

    // Boton unico de la cola: si esta detenida, arranca SIEMPRE desde el
    // item 0 (garantiza recorrer la lista completa). Si esta reproduciendo,
    // detiene.
    void TogglePlayStop();

    void Stop();

    // Llamar una vez por frame. Es el UNICO lugar del programa que debe
    // llamar a ConsumeEndReached() sobre el reproductor de fondo mientras
    // la cola esta activa.
    void Update();

    void  SetVolume(float vol0to2) { m_Volume = vol0to2; ApplyAV(); }
    void  SetMuted(bool muted)     { m_Muted  = muted;   ApplyAV(); }
    float GetVolume() const { return m_Volume; }
    bool  IsMuted()   const { return m_Muted; }

    const std::vector<std::string>& Items() const { return m_Items; }
    int  CurrentIndex()  const { return m_CurrentIndex; }
    int  SelectedIndex() const { return m_SelectedIndex; }
    void SetSelectedIndex(int i) { m_SelectedIndex = i; }
    QueueState State()  const { return m_State; }
    bool IsActive()     const { return m_State == QueueState::Playing; }

private:
    void ApplyAV();

    std::vector<std::string> m_Items;
    int        m_CurrentIndex  = -1;
    int        m_SelectedIndex = -1;
    QueueState m_State         = QueueState::Stopped;
    float      m_Volume        = 0.8f;
    bool       m_Muted         = false;

    // Cuenta errores reales (codec/archivo corrupto) seguidos que hizo
    // avanzar la cola sin que hubiera un fin de clip normal de por medio.
    // Si toda la cola esta rota, evita girar en silencio para siempre.
    int        m_ConsecutiveErrors = 0;

    // true una vez que ya se intento precargar el SIGUIENTE item para el
    // m_CurrentIndex actual (ver Update()/PreloadNextIfNeeded) — evita
    // reintentar el prefetch en cada frame despues del primer intento
    // exitoso. Se resetea a false cada vez que PlayIndex() cambia de item.
    bool       m_PrefetchedForCurrent = false;

    // Busca, desde fromIndexInclusive en adelante, la ruta del primer item
    // valido (mismo criterio "sin ruta = se omite" que PlayIndex) sin
    // mutar ningun estado — usado para saber que precargar. "" si no
    // queda ningun item valido en lo que resta de la cola.
    std::string FindNextValidPath(int fromIndexInclusive) const;
    void        PreloadNextIfNeeded();
};

} // namespace ProyecThor::UI
