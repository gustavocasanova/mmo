# Character System

## Responsabilidades

```text
GLB/glTF --future ModelLoader--> assets::Model (CPU data)
                                      |
                                      v
Character -> CharacterModel -> CharacterBody -> model meshes + skeleton
                    |                 |
                    |                 +-> AnimationController -> pose/skin matrices
                    +-> EquipmentManager -> resolved bone/socket attachments
                                      |
                                      v
                                  Renderer
```

- `assets::Model` guarda meshes, nodes, materials, texture descriptors, skeleton e clips. Não depende da GPU.
- `CharacterBody` referencia o model base e o skeleton compartilhado. O corpo é a fonte da rig; equipamentos não substituem a rig.
- `CharacterModel` compõe corpo, aparência, controller de animação e equipamentos visuais.
- `Character` é gameplay neutro: transform e stats básicos. Não inclui OpenGL, inventário, rede ou persistência.
- `renderer::Mesh` é recurso GPU e aceita vertices estáticos ou quatro índices/pesos de bone.

## Skeleton e Skinning

`animation::Bone` registra nome, índice, pai, transform local de bind e inverse-bind matrix. `Skeleton::add_bone` atribui índices; não há lista humanoide fixa. `find_bone`, `root_bone_index` e `is_valid` permitem consultar e validar nomes únicos, pais, uma raiz e ciclos. Skeleton vazio é permitido para models estáticos.

`Animator` avalia canais de translation, quaternion rotation e scale, monta matrizes globais e calcula `global * inverseBind`. `AnimationClip::is_valid` confere duração, bone indices, ordenação e tamanho de keyframes. O renderer atual aceita no máximo 48 matrizes na paleta GLSL e quatro influências por vertex.

## Animation Controller

`AnimationController` associa estados (`Idle`, `Walk`, `Run`, `Jump`, `Attack`, `Hit`, `Death`) a índices de clips. Clips não existentes não são inventados: estados precisam ser associados por `bind_state`. `play_animation`, `cross_fade`, `set_state` e `update` não conhecem input, combate ou movimento.

## Aparência e câmera

`CharacterAppearance` reserva dados simples para variante/cor do corpo, sem editor de criação. A câmera terceira pessoa continua em `core::CameraController`, separada de Character e CharacterModel; seguir/orbitar/zoom não pertencem ao asset.

## Estado e limitações

Não existe loader GLB/glTF conectado, upload de `assets::Model` para GPU, upload de textura ou personagem skinned real. O personagem visível ainda é um placeholder procedural do renderer. `CharacterEquipmentTest` valida contratos com dados sintéticos e permite alternar visualmente esse placeholder.

O caminho planejado do corpo é `assets/characters/player/body/player_body.glb`. Esse arquivo não foi criado. Quando o asset autoral existir, implementar/adicionar um `ModelLoader` maduro, mapear os dados para `assets::Model` e integrar buffers e materiais ao renderer.