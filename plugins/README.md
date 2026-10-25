# Carpeta de Plugins de ProyecThor

Esta carpeta contiene los plugins y extensiones instalados en ProyecThor.

## ¿Cómo añadir un plugin?

1. **Arrastra y suelta** cualquier archivo o carpeta directamente en esta carpeta (`plugins/`) o sobre la ventana de ProyecThor.
2. Formatos soportados:
   - **Carpetas con `plugin.json`**: Plugins completos con manifiesto, metadatos, acciones personalizadas y ejecutable.
   - **Scripts individuales (`.py`, `.sh`, `.bat`)**: ProyecThor los detecta automáticamente y genera una interfaz de ejecución en el panel de Plugins.
   - **Binarios ejecutables**: Herramientas nativas ejecutables con interfaz de terminal en vivo.

## Plugins incluidos por defecto

- `ytdlp/`: Gestor de YouTube y transmisiones mediante `yt-dlp`. Permite resolver enlaces de video directos para reproducir en VLC y extraer subtítulos como letras de canciones.
