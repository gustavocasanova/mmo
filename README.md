# MMO Engine

Engine 3D própria em C++20 para um MMORPG persistente de fantasia nórdica estilizada, com Valheim como referência visual de alto nível e assets/identidade originais. Sem Unity, Unreal ou Godot. O renderer é nosso; o primeiro backend usa OpenGL 3.3 Core.

Current step: **Marco 003 — personagem autoral em terceira pessoa**. O cliente carrega o personagem e os clips de animação do pack Universal Animation Library por fastgltf e aplica skinning da rig. WASD movimenta o personagem (A esquerda, D direita) e Espaço executa o pulo; ainda não há rede nem simulação autoritativa.

A arquitetura e as dependências planejadas estão em [docs/architecture.md](docs/architecture.md). A regra central é manter a simulação do servidor independente do cliente e da GPU.

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

## VS Code

Install the recommended extensions when VS Code prompts you. Open this folder, select the configure preset matching the installed Visual Studio (`vs2022-debug` or `vs2026-debug`), configure, and build the `mmo_client` target. The VS2026 preset writes to `build-vs2026-marco001`; VS2022 writes to `build`. To run the installed VS2026 build from PowerShell:

```powershell
.\build-vs2026-marco001\Debug\mmo_client.exe
```

The exact command-line configure/build steps below are useful when diagnosing CMake Tools errors.

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

GLM is used for renderer camera math. Networking, database, GLB/glTF loader, GPU texture upload and audio remain deferred. CPU tests use CTest and synthetic model data; no test framework or asset parser was added.

## Configure, build, run (PowerShell)

From the repository root (`C:\Users\gugug\Desktop\mmo`):

```powershell
$env:VCPKG_ROOT = "$env:USERPROFILE\vcpkg"   # skip if already set
cmake --preset vs2026-debug
cmake --build --preset vs2026-debug
ctest --test-dir build-vs2026-marco001 -C Debug --output-on-failure
\.\build-vs2026-marco001\Debug\mmo_client.exe
```

For a Visual Studio 2022 installation, use `vs2022-debug` instead; that preset uses `build/`.

Equivalent without presets:

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64 -DCMAKE_TOOLCHAIN_FILE="$env:VCPKG_ROOT\scripts\buildsystems\vcpkg.cmake"
cmake --build build --config Debug
.\build\Debug\mmo_client.exe
```

First configure downloads and builds GLFW via vcpkg (needs network).

Close with the window chrome or **Escape**. The process should print `[app] shutdown complete.` and exit 0.

## Expected picture

Uma cena em terceira pessoa com terreno verde quadriculado, o modelo rigged do pack Universal Animation Library e duas paredes de teste. WASD movimenta relativo à câmera; segure Shift para correr; Espaço pula; segure o botão direito e arraste para orbitar; scroll aproxima/afasta. As teclas `[` e `]` reproduzem, uma por vez, todos os clipes carregados. A câmera respeita o chão e recua ao encontrar as paredes; texturas ainda não são renderizadas.

The console prints OpenGL version, vendor, renderer and GLSL version, loaded mesh/bone/clip counts, and controls.

## Marco 003 acceptance

- [ ] CMake configure succeeds with MSVC x64 and the vcpkg toolchain
- [ ] `mmo_client` links without errors
- [ ] A window titled `MMO Engine - Movement Prototype` appears
- [ ] A 3D ground plane and a controllable GLB character are visible
- [ ] WASD moves the character
- [ ] Character movement follows camera yaw and normalizes diagonals
- [ ] The visible test walls block the character and camera
- [ ] Embedded `LINEAR`/`STEP` animation clips play when movement is active
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

## Tests

`CharacterEquipmentTest` validates synthetic model/skeleton data, animation state/crossfade and skin matrices, all equipment slots, attachment resolution, replacement, unequip and failure atomicity without opening a window. Run it with the `ctest` command above. GPU rendering and import of the user-provided GLB are runtime checks.

## License of content

Do not copy World of Warcraft assets, code, or data. Original art and code only.
