# Asset Pipeline

## Formatos

GLB é o formato de runtime/distribuição preferido; glTF separado também é aceito. `assets::GltfModelLoader` usa fastgltf e `AssetSystem::load_model` verifica extensão e valida a saída do loader.

`assets::Model` mantém dados CPU: nodes, meshes, indices, materiais, referência/dados RGBA de textura, skeleton, inverse-bind matrices e clips. A validação checa referências, hierarquias, pesos de skinning e canais de animação. O loader extrai geometria triangular, normals, cores base, até quatro pesos por vértice, skin e animações com interpolação `LINEAR` ou `STEP`. Decodificação/upload de texturas e renderização de vários skins no mesmo modelo ainda não estão implementados.

O cliente carrega `Universal Animation Library[Standard]/Unreal-Godot/UAL1_Standard.glb`, que traz a malha, a rig humanoide e os clips de animação juntos. O arquivo `_RM` inclui root motion baked nas animações; o cliente usa a variante sem root motion porque a movimentação do personagem é aplicada pelo próprio cliente.

O asset anterior do personagem masculino ainda pode ser regenerado com o script abaixo. Mantenha os arquivos de origem nas pastas atuais e execute na raiz do repositório:

```powershell
blender --background --python scripts\build_male_run_animation.py
```

O script importa `Superhero_Male_FullBody.gltf` e `Slow Run.fbx`, mapeia os ossos Mixamo para a rig do personagem e exporta um GLB temporário. Ele só substitui o modelo runtime após validar o skin e o clip. O renderer suporta até 66 joints.

O modelo importado requer as texturas referenciadas pelos arquivos glTF para a aparência completa. A renderização de texturas ainda não está implementada; o cliente usa as cores base.

## Layout

```text
assets/
  characters/player/body/player_body.glb  # destino planejado após validação do asset
  characters/player/animations/           # clips podem vir no GLB ou como assets separados
  characters/player/body/textures/
  characters/player/body/materials/
  equipment/{helmets,shoulders,chest,gloves,pants,boots,cloaks,bracers,rings,earrings,weapons}/
```

O asset de trabalho está em `personagem/`; migrá-lo para `assets/` quando o pipeline de conteúdo estiver definido. fastgltf é dependência do cliente para importação GLB/glTF.

## World Editor — Asset Browser

O catálogo do editor percorre somente `assets/`, `content/`, `personagem/` e `Universal Animation Library[Standard]/`; `.git`, diretórios de build e caches fora dessas raízes não são indexados. A extensão `.glb`/`.gltf` é verificada sem diferenciar maiúsculas/minúsculas. Os manifests `.mmoprefab` são encontrados dentro dessas raízes e usam o formato:

```text
MMO_PREFAB 1
model=assets/characters/prop.glb
```

O caminho do modelo é relativo à raiz do projeto e precisa apontar para um modelo indexado. Prefabs armazenados pelo editor ficam em `content/prefabs/`; eles reutilizam a geometria do modelo referenciado e não serializam o mundo. Arrastar modelo ou prefab para a viewport cria uma instância selecionável e centrada no ponto atingido do terreno.

Nesta etapa o renderer aceita apenas modelos estáticos. Modelos com skeleton/pesos de skinning são rejeitados explicitamente; materiais e texturas ainda não são aplicados à instância.