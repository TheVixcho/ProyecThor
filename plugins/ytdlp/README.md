# Plugin: Gestor YouTube & Streaming (yt-dlp)

Este plugin desacopla el uso de `yt-dlp` del núcleo de ProyecThor para que los usuarios puedan mantenerlo, actualizarlo o reemplazarlo independientemente.

## Características

- **Resolución de Stream**: Extrae el enlace de streaming de video para reproducción directa en VLC.
- **Importador de Letras**: Descarga subtítulos en español o inglés y los formatea como estrofas limpias para canciones.
- **Actualización Independiente**: Permite ejecutar `yt-dlp -U` o actualizar mediante pip desde la interfaz de usuario sin recompilar la aplicación.

## ¿Cómo instalar yt-dlp?

Tienes tres opciones sencillas:
1. **Colocar el ejecutable aquí**:
   - En Windows: descarga `yt-dlp.exe` desde [GitHub](https://github.com/yt-dlp/yt-dlp/releases) y colócalo en esta carpeta (`plugins/ytdlp/yt-dlp.exe`).
   - En Linux: descarga el binario `yt-dlp`, dale permisos (`chmod +x yt-dlp`) y colócalo en esta carpeta (`plugins/ytdlp/yt-dlp`).
2. **Instalar en el sistema**:
   - Vía pip: `pip install -U yt-dlp`
   - Vía gestor de paquetes de tu distribución (ej. `sudo apt install yt-dlp` o `sudo pacman -S yt-dlp`).
3. **Desde ProyecThor**:
   - Abre el panel de **Plugins & Extensiones** en ProyecThor, selecciona este plugin y pulsa **Actualizar yt-dlp**.
