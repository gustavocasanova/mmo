# MMO Engine

Engine 3D própria em C++20 para um MMORPG persistente de fantasia nórdica estilizada, com Valheim como referência visual de alto nível e assets/identidade originais. Sem Unity, Unreal ou Godot. O renderer é nosso; o primeiro backend usa OpenGL 3.3 Core.

**Etapa atual: Marco 003 — Mundo 3D.** A arquitetura do Marco 002 está implementada: Window, Renderer, Shader, Mesh, Material, Camera e GLM. O protótipo tem terreno, iluminação simples, personagem procedural com equipamentos interativos, câmera em terceira pessoa e movimento WASD; faltam carregamento de modelos e texturas. Veja o [roadmap](docs/roadmap.md) e o [guia dos módulos](docs/modules.md). Ainda não há rede nem simulação autoritativa.

A arquitetura e as dependências planejadas estão em [docs/architecture.md](docs/architecture.md). A regra central é manter a simulação do servidor independente do cliente e da GPU.

## Prerequisites (Windows 11)

Install these before configuring CMake. This machine did not have them on PATH when the sources were written.

| Tool | Why | Install |
| --- | --- | --- |
| Git | Version control; cloning vcpkg | `winget install --id Git.Git -e --source winget` |
| Visual Studio 2022 (MSVC) | C++20 compiler and Windows SDK | Workload **Desktop development with C++** |
| CMake 3.21+ | Build | Comes with VS, or `winget install --id Kitware.CMake -e --source winget` |
| vcpkg | GLFW e GLM | Clone as below |
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
.\build-vs2026-marco001\Debug\mmo_client.exe
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

Uma cena low-poly em terceira pessoa: placeholder procedural sobre terreno verde quadriculado, com câmera suavizada. WASD movimenta; scroll aproxima/afasta e arrastar com o botão direito orbita. Na janela `Character Equipment Test`, `1–8` alternam grupos de equipamento, `9` alterna anéis/brincos, `T` troca a paleta e `0` restaura o conjunto. O corpo visível continua placeholder, não é um GLB real. A câmera respeita o chão, mas ainda não evita paredes ou árvores.

The console prints OpenGL version, vendor, renderer and GLSL version, then `[app] Character Equipment Test running` and the controls.

## Verificação do protótipo atual

Esta lista verifica a cena existente; o escopo dos marcos está no [roadmap](docs/roadmap.md).

- [ ] CMake configure succeeds with MSVC x64 and the vcpkg toolchain
- [ ] `mmo_client` links without errors
- [ ] A window titled `MMO Engine - Character Equipment Test` appears
- [ ] A 3D ground plane and a controllable character are visible
- [ ] WASD moves the character
- [ ] Equipment groups can be toggled and recolored in the test scene
- [ ] A real skinned GLB drives the skeleton (not implemented; requires a model asset and GLTF loader)
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

`domain` verifica movimento, pilhas de itens, câmera, cena, substituição de meshes de equipamento e o componente de animação procedural em blocos. `CharacterEquipmentTest` preserva os testes de modelos/esqueletos sintéticos, crossfade, skin matrices, slots, attachments, substituição e remoção de equipamento.

```powershell
cmake --build --preset vs2026-debug
ctest --test-dir build-vs2026-marco001 -C Debug --output-on-failure
```

Com Visual Studio 2022, use `vs2022-debug` e `ctest --test-dir build -C Debug --output-on-failure`.
Para compilar ambos os testes sem GLFW/OpenGL, configure um diretório separado com
`-DMMO_BUILD_CLIENT=OFF`. GLM continua necessário; use a toolchain vcpkg ou uma
instalação nativa descoberta pelo CMake. A cena procedural não usa ainda um GLB real.

## License of content

Do not copy World of Warcraft assets, code, or data. Original art and code only.
