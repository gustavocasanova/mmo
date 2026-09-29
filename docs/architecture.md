# Architecture

Status: Marco 001 (foundation). Only the first graphics executable exists.

## Target shape

The long-term layout is:

- **Core** — time, logging, config, errors
- **Platform** — window and input
- **Renderer** — OpenGL resources and scene drawing
- **Resources / Scene / Animation / Physics / Audio**
- **Client** and **Server** as separate applications
- **Shared** — protocol and rules that do not steal server authority
- **Tools** — importers and editors

None of those modules are separate libraries yet. Marco 002 will split `Application`, `Window`, `Renderer`, and `Shader` out of `src/main.cpp`.

## Marco 001 executable

```
mmo_client
  GlfwRuntime   -> glfwInit / glfwTerminate (RAII)
  Window        -> GLFW window + OpenGL 3.3 Core context
  gladLoadGL    -> OpenGL function pointers
  ShaderProgram -> compile / link GLSL 330
  TriangleMesh  -> VAO + VBO
  loop          -> poll, clear, draw, swap, destroy
```

Simulation and rendering are still in one process and one file. That is acceptable until there is a server. The split to preserve now is: **OpenGL stays behind renderer types**; do not leak GL types into future gameplay or server code.

## Dependency graph (current)

```
mmo_client
  ├── GLFW 3 (vcpkg port glfw3) — window, context, input
  ├── glad (vendored, GLAD 2 API) — load GL 3.3 Core
  └── opengl32 (Windows) — ICD entry
```

There is no GLM, Assimp, audio, or network stack.

## Applications (planned)

| Binary | Role |
| --- | --- |
| `mmo_client` | Presentation, input, interpolation |
| `mmo_server` | Authoritative simulation and persistence |

`mmo_server` must not link GLFW, GLAD, or OpenGL.
