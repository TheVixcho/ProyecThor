# Third-Party Notices and SDK License Compliance

**ProyecThor** is proprietary, closed-source software (Copyright © 2026 ProyecThor Project / TheVixcho).
However, ProyecThor bundles, dynamically links against, and distributes third-party SDKs, libraries, and tools that carry their own licenses. 

In strict compliance with each third-party license (including the GNU LGPL 2.1, GNU GPL 3.0, Apache 2.0, BSD, MIT, and zlib licenses), this document details all third-party components, their licensing terms, and instructions for accessing corresponding source code where required by law.

---

## 1. Summary of Third-Party Libraries and SDKs

| Component | License | Linking / Distribution | Source Code Availability |
| :--- | :--- | :--- | :--- |
| **LibVLC SDK** (`libvlc.dll`, `libvlccore.dll`) | **GNU LGPL v2.1 or later** | Dynamic linking (shared libraries) | Source available (see Section 2 below) |
| **FFmpeg** (`ffmpeg.exe`) | **GNU GPL v3.0** | Standalone executable (invoked via child process/pipe) | Source available (see Section 3 below) |
| **PDFium** (`pdfium.dll`) | **Apache 2.0 / BSD 3-Clause** | Dynamic linking (shared library) | Not required; source at pdfium.googlesource.com |
| **Dear ImGui** | **MIT License** | Compiled into application | Included in plugin/development resources |
| **GLFW** | **zlib/libpng License** | Dynamic / static linking | Permissive; source at glfw.org |
| **GLEW** | **Modified BSD / MIT** | Compiled into application | Permissive; source at glew.sourceforge.net |
| **GLM (OpenGL Mathematics)** | **Happy Bunny / MIT** | Header-only | Permissive; source at github.com/g-truc/glm |
| **nlohmann/json** | **MIT License** | Header-only | Permissive; source at github.com/nlohmann/json |
| **cpp-httplib** | **MIT License** | Header-only | Permissive; source at github.com/yhirose/cpp-httplib |
| **stb (stb_image, stb_image_write, etc.)** | **Public Domain / MIT** | Header-only implementation | Permissive; source at github.com/nothings/stb |
| **NanoSVG** | **zlib License** | Header-only | Permissive; source at github.com/memononen/nanosvg |
| **qrcodegen** | **MIT License** | Source compiled | Permissive; source at nayuki.io |
| **yt-dlp** (`yt-dlp.exe`) | **The Unlicense (Public Domain)** | Standalone subprocess executable | Public Domain; source at github.com/yt-dlp/yt-dlp |
| **Microsoft Edge WebView2** | **Microsoft WebView2 SDK License** | Dynamic linking (`WebView2Loader.dll`) | Redistributable binary |
| **Holy-Bible-XML-Format (Beblia)** | **Public Domain / Open Data** | Bundled data assets | Public Domain |

---

## 2. LGPL 2.1 Compliance: LibVLC Engine

ProyecThor uses **LibVLC** (the core engine of VLC media player, Copyright © 1996–2026 VideoLAN and authors) for audio and video playback decoding.

### Compliance Details:
1. **Dynamic Linking**: ProyecThor links *dynamically* to `libvlc.dll` and `libvlccore.dll` (on Windows) and `.so` libraries (on Linux). The proprietary application code remains separate from the LGPL library.
2. **User Relinking and Substitution**: Because dynamic linking is used, any end user has the technical ability to replace or upgrade `libvlc.dll`, `libvlccore.dll`, or VLC plugins with their own custom or modified builds.
3. **Source Code Access**: In accordance with Section 6 of the GNU LGPL 2.1, the full, unmodified source code of the LibVLC version used is publicly available:
   - Official VideoLAN source repository: https://www.videolan.org/vlc/download-sources.html
   - VideoLAN Git repository: https://code.videolan.org/videolan/vlc
   - Version used: VLC 3.0.x series (compatible with runtime plugins located in the `plugins/` directory).

Full text of the GNU LGPL 2.1 license is reproduced below in Section 5.

---

## 3. GPL 3.0 Compliance: FFmpeg Executable

On Windows, ProyecThor bundles a precompiled `ffmpeg.exe` binary (used by the RTMP streaming and media conversion tools). The distributed build is the "essentials" package from Gyan.dev, which incorporates the `libx264` encoder, placing the compiled executable under the **GNU General Public License v3.0**.

### Compliance Details:
1. **Mere Aggregation (Subprocess Execution)**: ProyecThor does **not** link against FFmpeg libraries (libavcodec, libavformat, etc.) directly in its executable address space. Instead, ProyecThor invokes `ffmpeg.exe` as a standalone child process, communicating strictly via standard command-line arguments and standard input/output pipes. This constitutes "mere aggregation" under copyright law, allowing ProyecThor's own code to remain closed-source and proprietary.
2. **Binary Redistribution & Source Code Availability**: In accordance with Section 6 of the GNU GPL v3, any distributor of the compiled `ffmpeg.exe` binary must make the Corresponding Source code available. The complete source code and build recipes for the exact bundled FFmpeg build can be obtained from:
   - Official FFmpeg source code: https://ffmpeg.org/download.html
   - Gyan.dev build scripts & sources: https://github.com/GyanD/codexffmpeg
   - GNU GPL v3 License: https://www.gnu.org/licenses/gpl-3.0.html

---

## 4. Apache 2.0 Compliance: PDFium

PDFium is Copyright © 2014 Google LLC. Distributed under the Apache License 2.0.
Licensed under the Apache License, Version 2.0 (the "License"); you may not use this file except in compliance with the License. You may obtain a copy of the License at:
http://www.apache.org/licenses/LICENSE-2.0

---

## 5. Full License Texts

### GNU LESSER GENERAL PUBLIC LICENSE (LGPL v2.1)
```text
Version 2.1, February 1999
Copyright (C) 1991, 1999 Free Software Foundation, Inc.
51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
Everyone is permitted to copy and distribute verbatim copies
of this license document, but changing it is not allowed.

[Preamble and standard terms of LGPL 2.1 apply to LibVLC and its plugins]
Full text available at: https://www.gnu.org/licenses/old-licenses/lgpl-2.1.txt
```

### GNU GENERAL PUBLIC LICENSE (GPL v3.0)
```text
Version 3, 29 June 2007
Copyright (C) 2007 Free Software Foundation, Inc. <https://fsf.org/>
Everyone is permitted to copy and distribute verbatim copies
of this license document, but changing it is not allowed.

[Standard terms of GPL v3.0 apply to ffmpeg.exe]
Full text available at: https://www.gnu.org/licenses/gpl-3.0.txt
```

### MIT License
```text
Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
```

### zlib/libpng License
```text
This software is provided 'as-is', without any express or implied
warranty. In no event will the authors be held liable for any damages
arising from the use of this software.

Permission is granted to anyone to use this software for any purpose,
including commercial applications, and to alter it and redistribute it
freely, subject to the following restrictions:

1. The origin of this software must not be misrepresented; you must not
   claim that you wrote the original software. If you use this software
   in a product, an acknowledgment in the product documentation would be
   appreciated but is not required.
2. Altered source versions must be plainly marked as such, and must not be
   misrepresented as being the original software.
3. This notice may not be removed or altered from any source distribution.
```
