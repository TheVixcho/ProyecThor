# Contributing to ProyecThor

Thank you for your interest in contributing to **ProyecThor**! To maintain live-production stability, architectural clarity, and native performance, all contributions follow a structured workflow.

---

## 1. Before You Start

1. **Discuss First on Discord**: Before implementing a major feature, UI change, or refactor, join our development community on [Discord](https://discord.gg/QJ6wGHQjs6) and share your proposal.
2. **Review the Architecture**: Read [`docs/ARCHITECTURE.md`](./docs/ARCHITECTURE.md) to understand the separation between the C++ Core, Media Engine (`libmpv` / OpenGL), and the **Slint** UI (`src/ui_slint`).
3. **Check Third-Party Licenses**: Review [`THIRD-PARTY-NOTICES.md`](./THIRD-PARTY-NOTICES.md) and [`docs/THIRD_PARTY_LICENSES.md`](./docs/THIRD_PARTY_LICENSES.md) if your changes introduce or modify external dependencies.

---

## 2. Pull Request Guidelines (English Required)

To keep the repository accessible to all contributors worldwide:

- **Language**: All Pull Request titles, descriptions, commit messages, and code review discussions **must be written in English**.
- **Scope**: Keep PRs focused on a single feature, fix, or improvement. Avoid unrelated formatting or file changes.
- **Describe Clearly**: Include what changed, why it was needed, how it was tested (OS, GPU, Slint/libmpv behavior), and screenshots/videos for UI changes.
- **No Unreviewed AI Slop**: Read [`docs/AI_POLICY.md`](./docs/AI_POLICY.md). Human-led architecture and real-hardware testing are mandatory.

---

## 3. Local Build & Verification

Ensure the project builds cleanly without errors and runs reliably before opening a Pull Request:

```bash
# Linux (Arch / CachyOS / Ubuntu / Debian)
cmake -B build-linux -G Ninja -DCMAKE_BUILD_TYPE=Release
ninja -C build-linux

# Windows (MSVC / Visual Studio 2022)
cmake -B build-win -G "Visual Studio 17 2022" -A x64
cmake --build build-win --config Release
```

For the Slint UI target (`ProyecThorSlint`), ensure `PROYECTHOR_BUILD_SLINT=ON` (enabled by default) and the Slint C++ SDK is present in `src/vendor/sdk/`.

---

## 4. License

By contributing to ProyecThor, you agree that your contributions will be licensed under the **MIT License** (see [`LICENSE`](./LICENSE)).
