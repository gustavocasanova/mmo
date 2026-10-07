# Changelog

## Unreleased

- Tab-target combat: `game::combat` module (entities, TargetSystem with click/Tab/Shift+Tab, auto attack with range/cooldown/line of sight, DamageSystem), target ring, ImGui target frame, three enemies with death/despawn/respawn. Press `1` to toggle auto attack; combat facing uses `rotation_speed` without blocking movement.

### Added

- Sistema de animação em camadas: `AnimationMixer` + `BoneMask` (corpo inferior/superior), `MovementState`, `CombatState`, `CombatController`/`CombatSystem` com eventos AttackStart/AttackHit/AttackEnd; ataque (botão esquerdo/1/2/3) simultâneo ao movimento sem clipes combinados; debug F3.

- Marco 1 do World Editor: estado `EngineMode` isolado em `mmo_editor`, alternância F1 entre Game Mode e World Editor Mode e pausa/restauração do controlador do personagem sem recriar o personagem.
- Ferramentas de terreno existentes ficam restritas ao World Editor Mode; edição de entidades fica para marcos posteriores.
- Teste CPU para alternância de modo e comportamento de tecla mantida.
- Marco 2: câmera fly do editor com WASD, Q/E, look com RMB, Shift/Ctrl e ajuste de velocidade na roda. F2 move a alternância das ferramentas de terreno para liberar E para subir.
- Marco 3: seleção por raycast dos colliders demonstrativos, seleção do hit mais próximo, limpar ao clicar no vazio e highlight no renderer. F enquadra o alvo selecionado. Corrigido o sentido do strafe A/D da câmera fly.
- Marco 4 implementado e validado por build/testes CPU: transformações de objetos selecionados com mover/rotacionar/escala, gizmos XYZ, eixos World/Local e snap configurável; os bounds atualizados alimentam seleção, renderização e colisão. Validação visual interativa ainda pendente. Corrigido o sentido vertical da câmera de terceira pessoa para que mover o mouse para cima olhe para cima.
- Marco 5: painéis gráficos ImGui de World Hierarchy e Inspector; objetos podem ser selecionados, renomeados, duplicados, apagados e ter seus transforms editados. As alterações atualizam renderização e colisores do mundo.
- Marco 6: Asset Database para GLB/glTF e manifests de prefab versionados, Asset Browser com busca e criação de prefab, drag & drop para instanciar modelos estáticos selecionáveis e renderer com geometria transformada pela hierarquia de nós. Modelos com skin são rejeitados explicitamente; validação visual interativa continua pendente.

## 0.1.0 — Marco 001 (sources landed, not yet verified on this machine)

### Added

- CMake + vcpkg manifest project for an OpenGL 3.3 Core client
- Vendored GLAD 2-compatible loader (subset of 3.3 Core)
- `mmo_client`: GLFW window, shader compile/link, colored triangle, RAII shutdown
- Architecture, roadmap, and first ADR

### Not done until you report a successful run

- Configure, build, and on-screen triangle on Windows 11 with MSVC
