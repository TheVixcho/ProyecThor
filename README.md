# ProyecThor ⚡🎶
> **Professional multimedia playback software for multi-screen environments.**

[![C++](https://img.shields.io/badge/C++-17-blue.svg)](https://isocpp.org/)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)
[![VLC](https://img.shields.io/badge/Powered%20by-LibVLC-orange.svg)](https://www.videolan.org/vlc/libvlc.html)
[![Platform](https://img.shields.io/badge/Platform-Windows-lightgrey.svg)](#)

**ProyecThor** is a high-performance, open-source solution designed for professional environments that require seamless multimedia projection. Built for speed and reliability, it allows users to manage songs, videos, images, and scriptures across multiple displays via a centralized, intuitive control interface.

---

## 🚀 Key Features

* **Multi-Layer Architecture:** Manage background layers (solid colors, images, or videos) and text overlays independently.
* **Dual-Screen Workflow:** Dedicated operator interface with a clean, borderless projection output for external displays or projectors.
* **Hardware Accelerated:** Powered by the **LibVLC SDK** for low-latency, hardware-accelerated decoding of almost any video format.
* **Modern UI:** A sleek, docking-based workspace powered by **Dear ImGui**, optimized for high customizability and professional aesthetics.
* **Live Preview:** Real-time monitoring of staged content before pushing it to the live output.
* **Integrated Library:** Fast access to multimedia assets, categorized for instant retrieval during live events.

---

## 🛠️ Technical Stack

* **Language:** C++17
* **Graphics API:** OpenGL / GLFW
* **UI Framework:** Dear ImGui (Docking Branch)
* **Video Engine:** LibVLC SDK
* **Build System:** CMake (MinGW/MSYS2 optimized)

---

📁 Project Structure

ProyecThor/
├── .vscode/             # Environment configs (launch, tasks, IntelliSense)
├── src/                 # Main source code
│   ├── core/            # Business logic and presentation engine
│   ├── external/        # Third-party libraries
│   │   ├── imgui-docking/
│   │   └── tools/       # Internal utility tools (OpenURL, etc.)
│   ├── sdk/             # Local dependencies
│   │   ├── glfw/        
│   │   ├── glew/        
│   │   └── vlc/         # LibVLC headers and libs
│   ├── ui/              # User Interface orchestration
│   │   ├── panels/      # Modular UI components (Library, Control, etc.)
│   │   └── UIManager.cpp# UI lifecycle and docking management
│   └── main.cpp         # Application entry point and main loop
├── CMakeLists.txt       # Global build configuration
└── README.md            # Project documentation
