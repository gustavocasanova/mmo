# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

Engine 3D própria em C++20 (OpenGL 3.3 Core, sem Unity/Unreal/Godot) para um MMORPG de fantasia nórdica estilizada. Hoje só existe o cliente local (`mmo_client`); servidor, rede e persistência ainda não existem. Documentação e mensagens de commit são em português.

## Build e testes

Nesta máquina (Fedora) use os presets locais de `CMakeUserPresets.json` (ignorado pelo Git), que apontam GLFW, GLM, fastgltf e ImGui para `~/.local/opt/mmo-deps` e usam `.local-dev/native-toolchain.cmake`:

```sh
cmake --preset local-debug
cmake --build --preset local-debug -j 8
ctest --test-dir build/linux-debug --output-on-failure
ctest --test-dir build/linux-debug -R WoodenSwordTest --output-on-failure   # um teste
./build/linux-debug/mmo_client                                              # requer sessão gráfica
```

No Windows: presets `vs2022-debug` (→ `build/`) ou `vs2026-debug` (→ `build-vs2026-marco001/`), com `VCPKG_ROOT` definido; o `ctest` precisa de `-C Debug`.

- Cada teste é um executável próprio em `tests/` com `main()` e asserts simples (sem framework). Novo teste: `mmo_add_test(<Nome> tests/<arquivo>.cpp <libs>)` no `CMakeLists.txt` (roda na raiz do repo, então pode ler `content/`).
- Os testes rodam sem janela/GPU. Mudanças no renderer só são validadas rodando o cliente.
- `LayeringTest` (`cmake/check_layering.cmake`) falha se um include atravessar camadas proibidas; rode-o depois de mover código.
- Não há linter; o MSVC compila com `/W4 /permissive-` em todos os targets de `MMO_WARNING_TARGETS`.
- `scripts/generate-wooden-sword.py` regenera `content/items/wooden_sword/*.gltf` (os arquivos gerados são versionados).

## Arquitetura

Fonte de verdade: `docs/architecture.md` (princípios e limites de módulos), `docs/modules.md` (o que está ativo) , `docs/equipment-system.md` e `docs/items.md`.

Regras centrais:
- A simulação/regras de jogo não podem depender de janela, GPU, GLFW, GLAD, ImGui ou fastgltf. O futuro `mmo_server` não pode linkar nada gráfico. O cliente envia intenção; o servidor (futuro) decide resultados.
- Chamadas OpenGL só existem em `src/renderer/` (`renderer/opengl/` usa RAII).
- Não serializar structs C++ diretamente para rede/arquivo; formatos são textuais e versionados (ex.: `.mmoterrain`, `.mmoprefab`, `.mmoitem`, lidos por `core::parse_key_value`).

Camadas (detalhes em `docs/modules.md`; uma camada só inclui as de baixo):
- **Engine** — `mmo_core` (formato texto chave=valor), `mmo_world` (terreno, colisão), `mmo_animation`, `mmo_assets` (fastgltf), `mmo_equipment` (slots visuais e encaixe em bones), `mmo_character`, `mmo_scene`, `mmo_editor`; `renderer/` e `platform/` compilam só dentro de `mmo_client`. Nunca inclui `game/`, `presentation/` ou `apps/`.
- **Regras de jogo** — `mmo_game` (`game/combat`, `game/items`): linka só `mmo_core`, `mmo_world` e GLM. É o que o futuro servidor vai linkar; não pode tocar em skeleton, modelo, renderer ou ImGui.
- **Apresentação do cliente** — `mmo_presentation` (`presentation/item_visuals`): converte itens em `equipment::Equipment` e espelha o `Loadout` no `EquipmentManager`. `EquipmentPresenter` (usa o renderer) fica no executável.
- **Composição** — `apps/client/`: `application.cpp` (laço, input, editor), `PlayerItems` (cola itens ↔ combate ↔ visual), `DemoEnemies`, `character_setup`, janelas ImGui.

Fluxos que cruzam módulos:
- **Itens são dados, não classes** (`docs/items.md`): `content/items/**/*.mmoitem` tem as regras (`id`, `slot`, `weapon.*`, lidas por `game::items::ItemCatalog`) e o visual (`visual.*`, lido por `presentation::parse_item_visual`). Item novo = arquivo novo, sem C++. O `id` é a identidade persistente, então não reutilize nem altere.
- **Equipar:** `Inventory` → `Loadout::equip_from` (regra) → `LoadoutPresenter::sync` (constrói `Weapon`/`BasicEquipment`) → `EquipmentManager` → `EquipmentPresenter` → `Renderer`. Em paralelo, `main_hand_weapon(loadout)` → `CombatSystem::set_weapon` define dano, cadência e alcance.
- **Tipos de arma × itens:** `game/items/weapons.hpp` é um catálogo fixo (teclas 4–8) que escolhe classe, `AbilitySystem` (teclas 2/3) e as variações de ataque sem item. O item equipado (`.mmoitem`) define modelo, atributos e, se tiver `visual.attack_clip`, o clip do golpe.
- **Combate:** `game::combat::CombatSystem` decide o golpe; `character::CombatController::request_primary_attack` toca o clip do item equipado ou, sem ele, alterna as ações de `set_actions` (sem IDs nem `dynamic_cast`); o evento `AttackHit` da animação volta para `CombatSystem::on_attack_hit`, que revalida e aplica o dano. Existem duas classes `CombatSystem`: a de `character::` só conta eventos, a de `game::combat::` é a regra.
- **Renderer:** recebe uma lista genérica de `SkinnedInstance` (placement, tint, paleta, attachments rígidos); não sabe o que é jogador ou inimigo.
- **Editor:** F1 alterna Game/World Editor; no editor, `CharacterController::update` é pausado (o personagem não é recriado).

`EquipmentSlot` (engine) e `ItemSlot` (jogo) são enums paralelos e na mesma ordem; os `static_assert`s em `presentation/item_visuals.cpp` precisam continuar valendo.

## Conteúdo e assets

- Modelo/rig do personagem: `Universal Animation Library[Standard]/Unreal-Godot/UAL1_Standard.glb` (66 bones, 43 clips). Attachments de itens assumem nomes de bones do rig UAL (ex.: `hand_r`).
- Conteúdo próprio fica em `content/` (itens `.mmoitem` + modelos, prefabs `.mmoprefab`, `worlds/demo.mmoterrain`). Texturas ainda não são enviadas à GPU.
- Somente arte e código originais: não copiar assets, nomes ou dados de jogos comerciais (WoW, Valheim etc.).
