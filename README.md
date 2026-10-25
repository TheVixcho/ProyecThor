# ProyecThor

**Professional Projection Engine**

High-performance proprietary projection engine designed for optimal speed and reliability in high-pressure environments. Built for churches, theaters, and events where fluidity and instant reaction are everything.

[Website](https://proyecthor.web.app) &nbsp;•&nbsp; [Download Release](https://github.com/TheVixcho/ProyecThor/releases) &nbsp;•&nbsp; [Report a Bug](https://github.com/TheVixcho/ProyecThor/issues) &nbsp;•&nbsp; [Plugins & SDK](./plugins/README.md)

---
## Used AI to generate comprehensive comments, document function logic for team collaboration, and structure distinct code sections.

## Table of Contents

- [Project Philosophy](#project-philosophy)
- [Key Features](#key-features)
- [Platform Support](#platform-support)
- [System Requirements](#system-requirements)
- [Technical Stack](#technical-stack)
- [Contributing](#contributing)
- [Community and Support](#community-and-support)
- [Licensing](#licensing)

---

## Project Philosophy

ProyecThor was born from the need for a high-quality, free alternative to expensive proprietary projection software.

- **No Subscriptions** — All features are available to everyone, forever.
- **Operator-Centric** — Interface designed to minimize human error under pressure.
- **Total Privacy** — Operates 100% locally. Your files and data never leave your device.

---

## Key Features

| Feature | Description |
| :--- | :--- |
| Multi-Layer Architecture | Independently manage backgrounds (colors, images, videos) and text overlays. |
| Dual-Screen Workflow | Dedicated operator interface with a clean, borderless output for projectors. |
| Hardware Acceleration | LibVLC-powered engine for low-latency decoding of almost any format. |
| Modern UI | Docking-based workspace powered by Dear ImGui for high customizability. |
| Live Preview | Real-time monitoring of content before pushing it to the main screen. |

---

## Platform Support

| Platform | Status | Notes |
| :--- | :--- | :--- |
| Windows 10 / 11 (64-bit) | Fully supported | Primary development and testing platform. |
| Linux (Arch Linux / CachyOS) | Fully supported | Compatibility added recently; tested on Arch-based distributions. |
| Linux (Debian / Ubuntu-based) | Should work | Build dependencies available via `apt`; less extensively tested than Arch. |

On Linux, ProyecThor is noticeably lighter on resources than on Windows, using less RAM, CPU, and GPU on comparable hardware.

---

## System Requirements

### Windows

| | Minimum | Recommended |
| :--- | :--- | :--- |
| OS | Windows 10 (64-bit) | Windows 11 (64-bit) |
| CPU | Dual-core, modern generation | Intel i5 (recent generation) or i7 |
| RAM | 8 GB | 16 GB |
| GPU | Integrated graphics | Dedicated GPU with hardware acceleration |
| Storage | 500 MB free space (plus media library) | SSD recommended |

Because ProyecThor links against several SDKs (LibVLC, PDFium, ImGui/OpenGL rendering, etc.), a modern i5-class CPU and hardware-accelerated GPU are strongly recommended for smooth playback of video-heavy setups.

### Linux

| | Minimum | Recommended |
| :--- | :--- | :--- |
| Distro | Arch Linux / CachyOS (or Debian/Ubuntu-based) | Arch Linux / CachyOS |
| CPU | Dual-core | Quad-core (i5-class or equivalent) |
| RAM | 4 GB | 8 GB |
| GPU | Integrated graphics with OpenGL support | Dedicated GPU (optional, not required) |
| Storage | 500 MB free space (plus media library) | SSD recommended |

The Linux build has a smaller footprint overall, so lower-spec hardware is viable for lighter setups (fewer simultaneous layers, standard-definition media).

---

## Technical Stack

The ProyecThor core is optimized for maximum graphical performance and stability:

- **Language:** C++17
- **Graphics:** OpenGL / GLFW
- **UI Framework:** Dear ImGui (Docking branch)
- **Video Engine:** LibVLC SDK
- **Build System:** CMake

---

## Contributing & AI Policy

ProyecThor is open source and welcomes contributions. Because ProyecThor is built for high-stakes live productions, all contributions must uphold our architectural and stability standards:

1. **Human-led, Human-architected**: The core idea, architecture, and logic must be designed by human developers.
2. **AI as a Tool, Not the Author**: Artificial Intelligence is permitted strictly as an assistive tool (like an advanced linter, debugger, or comment generator). Zero tolerance for unverified "AI slop" or careless dumping of machine-generated code. If you wish to use AI without human oversight, you are free to create a separate fork under the MIT license.
3. **Exhaustive Real-World Testing**: Code must pass strict performance, frame rate, and stability checks on actual hardware.

👉 Read our full **[AI Contribution Policy & Manifesto (`docs/AI_POLICY.md`)](./docs/AI_POLICY.md)** and the **[Contribution Guidelines (`CONTRIBUTING.md`)](./CONTRIBUTING.md)** before opening a Pull Request.

---

ProyecThor is a non-profit project. We believe in the power of open-source collaboration to improve professional tools.

Want to support us? While the application is free, we accept voluntary donations to cover maintenance costs, update hosting, and development time to keep the software secure and up to date.

---

## Licensing

ProyecThor is proprietary software (All rights reserved) — see [`LICENSE`](./LICENSE) and [`LICENCIA.md`](./LICENCIA.md).

ProyecThor links against and integrates several third-party libraries and SDKs, each under its own respective license (including LGPL 2.1, GPL 3.0, Apache 2.0, MIT, and zlib). Complete legal notices, license texts, and source code availability compliance details can be found in [`THIRD-PARTY-NOTICES.md`](./THIRD-PARTY-NOTICES.md).

| Library | License | Link |
| :--- | :--- | :--- |
| Dear ImGui | MIT | [github.com/ocornut/imgui](https://github.com/ocornut/imgui) |
| GLFW | zlib/libpng | [glfw.org](https://www.glfw.org) |
| GLEW | MIT / BSD | [glew.sourceforge.net](http://glew.sourceforge.net) |
| GLM | MIT | [github.com/g-truc/glm](https://github.com/g-truc/glm) |
| LibVLC SDK | LGPL 2.1 | [videolan.org](https://www.videolan.org) |
| TagLib | LGPL 2.1 / MPL 1.1 | [taglib.github.io](https://taglib.github.io) |
| PDFium | BSD 3-Clause | [chromium.googlesource.com](https://chromium.googlesource.com/chromium/src/+/main/third_party/pdfium) |
| nlohmann/json | MIT | [github.com/nlohmann/json](https://github.com/nlohmann/json) |
| stb_image | Public Domain / MIT | [github.com/nothings/stb](https://github.com/nothings/stb) |
| Holy-Bible-XML-Format | Public Domain / Open Data | [github.com/Beblia/Holy-Bible-XML-Format](https://github.com/Beblia/Holy-Bible-XML-Format) |

ProyecThor also bundles two command-line tools, invoked as separate child processes (not linked into the app) for a couple of features:

| Tool | License | Link | Used for |
| :--- | :--- | :--- | :--- |
| FFmpeg (`ffmpeg.exe`, Windows builds) | GPL v3 (essentials build, includes libx264) | [ffmpeg.org](https://ffmpeg.org) | Streaming/RTMP encoding and Biblioteca > Render (format conversion). |
| yt-dlp (`yt-dlp.exe`, Windows builds) | The Unlicense (public domain) | [github.com/yt-dlp/yt-dlp](https://github.com/yt-dlp/yt-dlp) | Resolving playable stream URLs and Archivo > Importar > Importar desde URL (subtitle-as-lyrics import). |

Some of these dependencies (notably LibVLC and TagLib, both under LGPL 2.1) carry additional obligations beyond the permissive licenses above, especially regarding static linking. TagLib is linked statically in ProyecThor; its source (v1.13.1) is available at [github.com/taglib/taglib/releases/tag/v1.13.1](https://github.com/taglib/taglib/releases/tag/v1.13.1).

**Anyone modifying, building, or redistributing ProyecThor should read [`THIRD-PARTY-NOTICES.md`](./THIRD-PARTY-NOTICES.md) before making changes that affect these SDKs.**

### Acknowledgments

- **Beblia (Holy-Bible-XML-Format)**: The bundled Bible versions and XML schema structure are provided by the [Holy-Bible-XML-Format](https://github.com/Beblia/Holy-Bible-XML-Format) project by Beblia.
- **SudoMeke / subtitle-grabber**: The "Importar desde URL" subtitle-as-lyrics feature (Archivo > Importar) follows the same yt-dlp track-selection approach (manual captions preferred over auto-generated, `tlang=` auto-translations filtered out) as [SudoMeke/subtitle-grabber](https://github.com/SudoMeke/subtitle-grabber), a Python CLI tool for downloading YouTube subtitles. ProyecThor's implementation is an independent C++ port, not a copy of its code.

---

<div align="center">
<sub>Developed for the live production community.</sub><br>
<sub>&copy; 2026 ProyecThor Project.</sub>
</div>
