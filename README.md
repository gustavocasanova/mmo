# MMO Engine

Custom 3D engine (C++20, OpenGL 3.3 Core) intended as the long-term base for a fantasy MMORPG. This repository does **not** use Unity, Unreal, or Godot.

Current milestone: **Marco 001 — Fundação**.

The first binary is `mmo_client`. It opens a window, creates an OpenGL 3.3 Core context, compiles shaders, and draws a colored triangle.

## Prerequisites (Windows 11)

Install these before configuring CMake. This machine did not have them on PATH when the sources were written.

| Tool | Why | Install |
| --- | --- | --- |
| Git | Version control; cloning vcpkg | `winget install --id Git.Git -e --source winget` |
| Visual Studio 2022 (MSVC) | C++20 compiler and Windows SDK | Workload **Desktop development with C++** |
| CMake 3.21+ | Build | Comes with VS, or `winget install --id Kitware.CMake -e --source winget` |
| vcpkg | GLFW | Clone as below |
| GPU driver with OpenGL 3.3 | Runtime | Vendor driver |

Optional helper script (installs Git, CMake, VS Community):

```powershell
Set-ExecutionPolicy -Scope Process Bypass
.\scripts\install-prereqs.ps1
```

Then **close and reopen** the terminal.

### Visual Studio (if not using the script)

Install [Visual Studio 2022 Community](https://visualstudio.microsoft.com/) with workload **Desktop development with C++**. Include the MSVC v143 toolset and Windows 10/11 SDK.

### vcpkg

```powershell
cd $env:USERPROFILE
git clone https://github.com/microsoft/vcpkg.git
cd vcpkg
.\bootstrap-vcpkg.bat
[Environment]::SetEnvironmentVariable("VCPKG_ROOT", "$env:USERPROFILE\vcpkg", "User")
$env:VCPKG_ROOT = "$env:USERPROFILE\vcpkg"
```

Confirm:

```powershell
echo $env:VCPKG_ROOT
Test-Path "$env:VCPKG_ROOT\scripts\buildsystems\vcpkg.cmake"
```

The second command must print `True`. Open a **new** PowerShell after setting the User environment variable if `$env:VCPKG_ROOT` is empty.

## Dependencies

### GLFW 3 (`glfw3` on vcpkg)

- **Purpose:** window, OpenGL context, keyboard.
- **Alternatives:** raw Win32+WGL (more code, same result later); SDL2 (broader, unused subsystems).
- **Impact:** Platform layer only. The future dedicated server will not link GLFW.

Version: whatever the vcpkg `glfw3` port resolves to when you bootstrap (typically GLFW 3.4.x). Record the exact version from the configure log after the first successful build.

### GLAD 2 API (vendored)

- **Purpose:** load OpenGL 3.3 Core function pointers.
- **Not using vcpkg `glad`:** that port is GLAD 1 (`glad/glad.h`).
- **Impact:** static library `glad`, renderer-only. See `third_party/glad/README.md`.

No other third-party libraries in Marco 001.

## Configure, build, run (PowerShell)

From the repository root (`C:\Users\gugug\Desktop\mmo`):

```powershell
$env:VCPKG_ROOT = "$env:USERPROFILE\vcpkg"   # skip if already set
cmake --preset vs2022-debug
cmake --build --preset vs2022-debug
.\build\Debug\mmo_client.exe
```

Equivalent without presets:

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64 -DCMAKE_TOOLCHAIN_FILE="$env:VCPKG_ROOT\scripts\buildsystems\vcpkg.cmake"
cmake --build build --config Debug
.\build\Debug\mmo_client.exe
```

First configure downloads and builds GLFW via vcpkg (needs network).

Close with the window chrome or **Escape**. The process should print `[app] shutdown complete.` and exit 0.

## Expected picture

Dark gray-blue background (`RGB ≈ 0.07, 0.08, 0.10`). One large triangle: red-orange lower left, green lower right, blue top.

The console should print OpenGL version, vendor, renderer, and GLSL version, then `[app] Marco 001 running`.

## Marco 001 acceptance

- [ ] CMake configure succeeds with MSVC x64 and the vcpkg toolchain
- [ ] `mmo_client` links without errors
- [ ] A window titled `MMO Engine — Marco 001` appears
- [ ] A colored triangle is visible
- [ ] Resize still fills the framebuffer (viewport callback)
- [ ] Escape or close destroys the context and returns 0
- [ ] Console shows GL version **3.3** or higher in a **Core** context

## Likely failures

| Symptom | What to check |
| --- | --- |
| `git` / `cmake` not recognized | Install tools, new terminal, PATH |
| `CMAKE_TOOLCHAIN_FILE` / `VCPKG_ROOT` | `echo $env:VCPKG_ROOT` |
| `Could not find a package configuration file named glfw3` | vcpkg did not install; read `build/vcpkg-manifest-install.log` |
| `glfwCreateWindow failed` | GPU/driver too old; try `opengl32` ICD update |
| `gladLoadGL failed` | Context is not current, or functions missing |
| Shader compile failed | Driver GLSL; paste the log |
| Window opens then closes | Read `stderr`; an exception ran `fail()` |
| Generator not Visual Studio 17 | Install VS 2022, not only Build Tools without CMake/MSVC |

## Tests (Marco 001)

There is no automated GPU test yet. The acceptance list above is the test. Marco 002 adds unit tests for non-GL code (strings, errors, time).

## Git

After Git is installed:

```powershell
cd C:\Users\gugug\Desktop\mmo
git init
git add .
git commit -m "Marco 001: OpenGL 3.3 window and triangle foundation"
```

## License of content

Do not copy World of Warcraft assets, code, or data. Original art and code only.
