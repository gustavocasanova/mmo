# Asset Pipeline

## Formatos

GLB é o formato de runtime/distribuição preferido; glTF separado também é aceito pelo contrato. Não há parser GLB próprio. `assets::ModelLoader` é a fronteira para uma futura biblioteca madura; `AssetSystem::load_model` verifica extensão e valida a saída do loader.

`assets::Model` mantém dados CPU: nodes, meshes, indices, materiais, referência/dados RGBA de textura, skeleton, inverse-bind matrices e clips. A validação checa referências, hierarquias, pesos de skinning e canais de animação. A leitura de bytes GLB, decodificação de imagem, upload de textura e criação de buffers a partir de `assets::Model` ainda não estão implementados.

## Layout

```text
assets/
  characters/player/body/player_body.glb  # futuro; não criar arquivo vazio
  characters/player/animations/           # clips podem vir no GLB ou como assets separados
  characters/player/body/textures/
  characters/player/body/materials/
  equipment/{helmets,shoulders,chest,gloves,pants,boots,cloaks,bracers,rings,earrings,weapons}/
```

Os diretórios de conteúdo serão criados quando os primeiros assets existirem; nenhum GLB placeholder é mantido no repositório. Dependências atuais permanecem GLFW e GLM; uma biblioteca GLTF será escolhida quando a implementação do loader começar.