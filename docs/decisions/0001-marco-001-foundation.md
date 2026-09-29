# ADR 0001 — Marco 001 foundation

Date: 2026-09-29

## Context

Build a custom engine on Windows 11, C++20, MSVC, CMake, vcpkg, OpenGL 3.3 Core. The first milestone must open a window and draw a triangle without dragging in later systems.

## Decisions

1. **One executable, one translation unit** (`src/main.cpp`). File split is Marco 002.
2. **GLFW 3 via vcpkg** for window and context. Raw Win32+WGL would delay the first green frame. SDL2 would add a larger I/O surface we do not need yet.
3. **GLAD 2 API, vendored subset** (`glad/gl.h`, `gladLoadGL`). The vcpkg `glad` port is GLAD 1. Full GLAD 2 generation needs Python; a curated 3.3 Core subset is enough for this milestone.
4. **Shaders as string literals** in `main.cpp`. Avoids working-directory bugs before a resource system exists.
5. **C++ exceptions** for fatal startup failures (init, compile, link). The process has no recovery path yet. Logging will be formalized in Marco 002.
6. **No GLM, Assimp, tests, or server** in this milestone.

## Consequences

- `mmo_client` is a graphics smoke test, not an engine library.
- Replacing the vendored loader with a full GLAD 2 dump must keep `gladLoadGL` call sites.
- Gameplay and future `mmo_server` must not include GLFW or glad.
