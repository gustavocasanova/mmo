# Character System

## Responsabilidades

```text
GLB/glTF --GltfModelLoader--> assets::Model (CPU data)
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

`Animator` avalia canais de translation, quaternion rotation e scale, monta matrizes globais e calcula `global * inverseBind`. `AnimationClip::is_valid` confere duração, bone indices, ordenação e tamanho de keyframes. O renderer aceita até 66 matrizes de skinning em um uniform buffer e quatro influências por vertex.

## Animation Controller

`AnimationController` associa estados (`Idle`, `Walk`, `Run`, `Jump`, `Attack`, `Hit`, `Death`) a índices de clips. Clips não existentes não são inventados: estados precisam ser associados por `bind_state`. `play_animation`, `cross_fade`, `set_state` e `update` não conhecem input, combate ou movimento.

## Aparência e câmera

`CharacterAppearance` reserva dados simples para variante/cor do corpo, sem editor de criação. A câmera terceira pessoa continua em `core::CameraController`, separada de Character e CharacterModel; seguir/orbitar/zoom não pertencem ao asset.

## Estado e limitações

`assets::GltfModelLoader` usa fastgltf para carregar meshes, normals, pesos, joints, inverse-bind matrices e canais de animação `LINEAR`/`STEP` de GLB/glTF para `assets::Model`. O cliente carrega `personagem/characterRIGGED.glb`, envia a malha ao renderer e atualiza a paleta de skinning a cada frame. O tamanho do modelo é normalizado para 1,8 unidades de altura e a base é alinhada ao chão.

O GLB runtime usa o personagem masculino de 65 joints e inclui o clip `Slow Run`, retargetado dos ossos Mixamo. Quando o personagem se move, `Walk` usa um clip `Walk` se existir; caso contrário, usa `Run`. Ao parar, volta à pose de bind se não houver clip `Idle`. Texturas ainda não são enviadas à GPU; fatores base de cor são aplicados por vértice.

O script `scripts/build_male_run_animation.py` regenera o GLB runtime a partir do personagem glTF e da animação FBX. `CharacterEquipmentTest` cobre os contratos CPU de equipamento e valida também o asset/animação runtime; as peças ainda não são carregadas/renderizadas como modelos GLB.