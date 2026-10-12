# Equipment System

## Separação

```text
Inventory data (future)
        |
        v
Equipment value -> EquipmentManager -> resolved attachments -> CharacterModel -> Renderer
```

Inventário futuro não terá dependência gráfica. `equipment::Equipment` é um dado visual independente: ID, nome, slot, caminho `.glb`/`.gltf` e uma ou mais attachments. `EquipmentManager` não carrega assets nem usa OpenGL.

## Slots e attachments

Slots: Helmet, Shoulders, Chest, Gloves, Pants, Boots, Cloak, Bracers, Ring1–Ring4, Earring1–Earring2, MainHand e OffHand. Um item pode declarar mais de uma `EquipmentAttachment`, cada uma com nome de socket, bone e transform local (offset, rotação, escala).

Exemplos de rig esperados, não nomes obrigatórios do skeleton:

| Slot | Attachments sugeridas |
|---|---|
| Helmet | Head |
| Shoulders | Shoulder_L, Shoulder_R |
| Gloves | Hand_L, Hand_R |
| Boots | Foot_L, Foot_R |
| Cloak | Back |
| Bracers | Forearm_L, Forearm_R |
| Rings | bones de dedos independentes |
| Earrings | Ear_L, Ear_R |
| MainHand / OffHand | Hand_R / Hand_L |

O `EquipmentManager` valida o slot, ID/nome, extensão, skeleton, bone names e transforms antes de substituir o item atual. `unequip`, `get_equipped`, `has_equipped`, `clear_slot`, `clear` e `resolved_attachments` permitem testar e consultar mudanças sem recriar o personagem.

## Limitações

O manager resolve bone indices, mas não compõe ainda as matrizes world-space de cada attachment nem solicita ao renderer o carregamento/desenho dos modelos. Cloak é apenas um attachment; física e animação própria ficam para depois. Não existe schema JSON ou catálogo de itens nesta etapa.

## Armas disponíveis no protótipo

O catálogo do cliente oferece Espada grande (4), Arco de arqueiro (5), Cajado de mago (6), Espada e escudo (7) e Machado (8). Cada arma seleciona suas habilidades e, quando há animações compatíveis, percorre as variações de ataque uma a uma. O arqueiro usa as animações de locomoção do pacote Longbow, mas não recebe um ataque automático animado: os arquivos disponíveis não incluem disparo de arco.

Os FBXs escolhidos dos pacotes ZIP ficam em `assets/animations/source/` localmente. Para convertê-los e retargetá-los ao esqueleto UAL, execute:

```text
node scripts/retarget_melee_animations.js <caminho-para-FBX2glTF.exe> --weapon-packs
```

O comando gera `assets/animations/generated/weapon_animations_ual.glb`, carregado pelo cliente se estiver presente. O pacote contém variações de espada grande, espada e escudo, machado e magia, locomoção de arco, além de `Jump_Running` e `Jump_Running_Landing`; as transições de salto e aterrissagem usam cross-fade para voltar à corrida equipada. Os FBXs de origem e os GLBs gerados são locais e ignorados pelo Git. Sem o GLB gerado, o cliente mantém as animações UAL embutidas e informa no console quais recursos opcionais não estão disponíveis.
