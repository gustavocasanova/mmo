# Changelog

## Unreleased

### Added

- Marco 1 do World Editor: estado `EngineMode` isolado em `mmo_editor`, alternância F1 entre Game Mode e World Editor Mode e pausa/restauração do controlador do personagem sem recriar o personagem.
- Ferramentas de terreno existentes ficam restritas ao World Editor Mode; edição de entidades fica para marcos posteriores.
- Teste CPU para alternância de modo e comportamento de tecla mantida.
- Marco 2: câmera fly do editor com WASD, Q/E, look com RMB, Shift/Ctrl e ajuste de velocidade na roda. F2 move a alternância das ferramentas de terreno para liberar E para subir.
- Marco 3: seleção por raycast dos colliders demonstrativos, seleção do hit mais próximo, limpar ao clicar no vazio e highlight no renderer. F enquadra o alvo selecionado. Corrigido o sentido do strafe A/D da câmera fly.

## 0.1.0 — Marco 001 (sources landed, not yet verified on this machine)

### Added

- CMake + vcpkg manifest project for an OpenGL 3.3 Core client
- Vendored GLAD 2-compatible loader (subset of 3.3 Core)
- `mmo_client`: GLFW window, shader compile/link, colored triangle, RAII shutdown
- Architecture, roadmap, and first ADR

### Not done until you report a successful run

- Configure, build, and on-screen triangle on Windows 11 with MSVC
