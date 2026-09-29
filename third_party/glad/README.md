# Vendored OpenGL loader (GLAD 2 API)

## Purpose

Load OpenGL 3.3 Core function pointers after GLFW creates a context.

Application code uses the GLAD 2 include and init path:

```cpp
#include <glad/gl.h>
gladLoadGL(reinterpret_cast<GLADloadfunc>(glfwGetProcAddress));
```

## Why this is not the vcpkg `glad` port

The vcpkg port `glad` currently exposes the **GLAD 1** API (`glad/glad.h`, `gladLoadGLLoader`). This project standardizes on **GLAD 2** (`glad/gl.h`, `gladLoadGL`) as specified for Marco 001.

## Why it is vendored

Generating a full GLAD 2 dump requires Python and the `glad2` package. Marco 001 vendors a **subset** of 3.3 Core entry points so the first executable does not depend on Python.

## Alternatives considered

| Option | Pros | Cons |
| --- | --- | --- |
| Full GLAD 2 generated sources | Complete 3.3/4.x surface | Needs Python at generation time; large files |
| vcpkg `glad` (GLAD 1) | Easy package | Different API than the one we standardized |
| Manual `wglGetProcAddress` only | No extra files | Windows-specific, easy to get calling conventions wrong |

## Regenerating a full loader later

When Python is available:

```powershell
python -m pip install glad2
python -m glad --api="gl:core=3.3" --out-path="third_party/glad" --reproducible
```

Then keep `gladLoadGL` call sites unchanged. Add new functions only when a later milestone needs them.

## Impact on architecture

The loader is a Platform/Renderer detail. Game and Shared modules must never include `glad` headers. Only renderer (and this first `main.cpp`) talk to OpenGL.
