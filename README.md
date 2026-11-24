# ProyecThor

**Open-Source Professional Multimedia Projection Engine**

ProyecThor is a high-performance, open-source multimedia playback and projection engine built in **C++** and **Slint**. Engineered for instant response, multi-layer compositing, and rock-solid stability under live production pressure in theaters, conferences, churches, and live events.

[Website](https://thevixcho.github.io/ProyecThor/) &nbsp;•&nbsp; [Download Release](https://github.com/TheVixcho/ProyecThor/releases) &nbsp;•&nbsp; [Discord Community](https://discord.gg/FKqgPDZpBF) &nbsp;•&nbsp; [Report a Bug](https://github.com/TheVixcho/ProyecThor/issues) &nbsp;•&nbsp; [Plugins & SDK](./plugins/README.md)

---

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

ProyecThor was created as a free, open-source alternative to expensive closed-source projection suites.

- **100% Open Source (MIT)** — Transparent codebase, free to inspect, modify, compile, and extend.
- **No Subscriptions** — Every feature is available to everyone forever, with no accounts or locked modules.
- **Operator-Centric** — Designed to minimize human error and latency during live productions.
- **Local-First Privacy** — Operates locally on your hardware; your media and libraries stay on your machine.

---

## Key Features

| Feature | Description |
| :--- | :--- |
| **Slint Native UI** | Modern, responsive, GPU-accelerated desktop interface compiled natively with Slint and C++. |
| **Async `libmpv` Engine** | Hardware-accelerated, low-latency video and audio playback with support for HD/4K media. |
| **Multi-Layer Architecture** | Independently manage backgrounds (colors, images, video), overlays, and text layers. |
| **Multi-Screen Workflow** | Dedicated operator control station with clean, borderless outputs for projectors and stage displays. |
| **LAN & Remote Control** | Connect, trigger, and manage presentations across your local network and mobile companion app. |
| **Live Visual Effects** | Real-time shaders and effects (vignette, blur, grain, sharpening, CRT) applied directly to output. |
| **Built-in Media Tools** | Integrated format conversion, PDF rendering (PDFium), and audio playback. |

---

## Platform Support

| Platform | Status | Notes |
| :--- | :--- | :--- |
| **Linux (Arch Linux / CachyOS)** | Fully supported | Optimized memory, CPU, and GPU footprint; Wayland (PipeWire/D-Bus) & X11 support. |
| **Linux (Debian / Ubuntu-based)** | Supported | Build dependencies available via `apt`. |
| **Windows 10 / 11 (64-bit)** | Fully supported | Native MSVC build with hardware-accelerated `libmpv` and NVENC/OpenGL support. |

---

## System Requirements

### Linux

| Component | Minimum | Recommended |
| :--- | :--- | :--- |
| **Distro** | Arch / CachyOS or Debian / Ubuntu (64-bit) | Arch Linux / CachyOS (Wayland or X11) |
| **CPU** | Dual-core 64-bit | Quad-core (i5 / Ryzen 5 or equivalent) |
| **RAM** | 4 GB | 8 GB or more |
| **GPU** | Integrated GPU with OpenGL 3.3+ | Dedicated GPU (Vulkan / OpenGL hardware accel) |
| **Storage** | 500 MB free space + media library | NVMe / SATA SSD |

### Windows

| Component | Minimum | Recommended |
| :--- | :--- | :--- |
| **OS** | Windows 10 (64-bit) | Windows 11 (64-bit) |
| **CPU** | Intel Core i5 (7th gen) or Ryzen equivalent | Recent Intel Core i5/i7 or Ryzen 5/7 |
| **RAM** | 8 GB | 16 GB |
| **GPU** | Integrated GPU (OpenGL 3.3+ / Direct3D) | Dedicated GPU with NVENC / hardware decode |
| **Storage** | 500 MB free space + media library | SSD |

---

## Technical Stack

ProyecThor is built for native performance, low frame latency, and cross-platform reliability:

- **Languages:** C++17 (Core) & C++20 (Slint UI layer)
- **UI Framework:** [Slint](https://slint.dev/) C++ SDK (`ProyecThorSlint`)
- **Media & Video Engine:** `libmpv` (Async hardware-accelerated playback)
- **Graphics & Windowing:** OpenGL, X11 / Wayland (PipeWire & D-Bus)
- **Documents & Metadata:** PDFium, TagLib, `nlohmann/json`, OpenSSL
- **Build System:** CMake + Ninja / MSVC

---

## Contributing

ProyecThor is open source and welcomes community contributions! To keep the codebase clean and reliable for live productions:

1. **Pull Requests in English**: All Pull Request titles, descriptions, commit messages, and code review discussions **must be written in English**.
2. **Coordinate on Discord**: Before starting large features or architectural changes, discuss your idea on our [Discord server](https://discord.gg/FKqgPDZpBF).
3. **Human-Led Architecture & Real Testing**: AI tools may assist with linting or boilerplate, but all PRs must be human-architected and tested on real hardware (see [`docs/AI_POLICY.md`](./docs/AI_POLICY.md)).
4. **Clean Builds**: Verify your changes compile cleanly on Linux (`cmake -B build-linux -G Ninja && ninja -C build-linux`) and/or Windows before opening a PR.

👉 Read [`CONTRIBUTING.md`](./CONTRIBUTING.md) and [`docs/ARCHITECTURE.md`](./docs/ARCHITECTURE.md) before submitting a Pull Request.

---

## Community and Support

- **Discord**: [Join the ProyecThor Discord](https://discord.gg/FKqgPDZpBF) for development discussions, support, and beta testing.
- **Instagram**: [Follow @theproyecthor](https://www.instagram.com/theproyecthor/) for updates and showcases.
- **WhatsApp Channel**: [Join the WhatsApp Channel](https://whatsapp.com/channel/0029Vb7e9tj3WHTdNivIxR19) for release announcements.
- **GitHub Issues**: [Report bugs or request features](https://github.com/TheVixcho/ProyecThor/issues).
- **Donations**: ProyecThor is a free, non-profit open-source project. Voluntary contributions help cover hosting, testing, and development costs.

---

## Licensing

ProyecThor is **Open Source** software licensed under the **MIT License** — see [`LICENSE`](./LICENSE) and [`LICENCIA.md`](./LICENCIA.md).

ProyecThor integrates and links against third-party open-source libraries and tools, each governed by its respective license. Complete attributions and license texts are documented in [`THIRD-PARTY-NOTICES.md`](./THIRD-PARTY-NOTICES.md) and [`docs/THIRD_PARTY_LICENSES.md`](./docs/THIRD_PARTY_LICENSES.md).

| Component | License | Link |
| :--- | :--- | :--- |
| Slint | GPLv3 / Commercial / Royalty-free Desktop | [slint.dev](https://slint.dev) |
| libmpv | GPL v2+ / LGPL v2.1+ | [mpv.io](https://mpv.io) |
| PDFium | BSD 3-Clause | [chromium.googlesource.com](https://chromium.googlesource.com/chromium/src/+/main/third_party/pdfium) |
| TagLib | LGPL 2.1 / MPL 1.1 | [taglib.org](https://taglib.org) |
| GLM | MIT | [github.com/g-truc/glm](https://github.com/g-truc/glm) |
| nlohmann/json | MIT | [github.com/nlohmann/json](https://github.com/nlohmann/json) |
| stb_image | Public Domain / MIT | [github.com/nothings/stb](https://github.com/nothings/stb) |
| FFmpeg (bundled CLI) | GPL v3 | [ffmpeg.org](https://ffmpeg.org) |
| yt-dlp (bundled CLI) | The Unlicense | [github.com/yt-dlp/yt-dlp](https://github.com/yt-dlp/yt-dlp) |

---

<div align="center">
<sub>Open-source multimedia projection engine for live productions.</sub><br>
<sub>&copy; 2026 ProyecThor Project — MIT License.</sub>
</div>
