# Itens

Um item é **dado**, não uma classe. Cada item vive em um arquivo
`content/items/<nome>/<nome>.mmoitem` com duas metades:

| Metade | Chaves | Quem lê | Onde vive no código |
| --- | --- | --- | --- |
| Regra de jogo | `id`, `key`, `name`, `slot`, `max_stack`, `weapon.*` | cliente agora, servidor depois | `game/items` (`mmo_game`) |
| Apresentação | `visual.*` | só o cliente | `presentation/item_visuals` (`mmo_presentation`) |

O `id` é a identidade estável do item: inventário, rede e banco guardam esse número.
Nunca reutilize nem altere o `id` de um item já publicado; `key` é o nome legível
usado em ferramentas e logs.

## Formato

```text
MMO_ITEM 1
id=1001
key=wooden_sword
name=Wooden Training Sword
slot=MainHand
weapon.damage=25
weapon.attack_interval=1.5
weapon.range=2.6

visual.model=content/items/wooden_sword/wooden_sword.gltf
# socket bone px py pz qx qy qz qw [sx sy sz]
visual.attach=MainHandGrip hand_r 0 0.075 0 0.70710678 0 0 0.70710678
visual.attack_clip=Sword_Attack
visual.attack_duration=1.2
visual.attack_hit_time=0.55
```

- Primeira linha: formato e versão. Linhas vazias e começadas por `#` são ignoradas.
- `slot` (opcional): `Helmet`, `Shoulders`, `Chest`, `Gloves`, `Pants`, `Boots`, `Cloak`,
  `Bracers`, `Ring1`–`Ring4`, `Earring1`–`Earring2`, `MainHand`, `OffHand`. Sem slot, o
  item não é equipável (por exemplo, materiais e consumíveis).
- `max_stack` (padrão 1). Itens equipáveis não empilham.
- `weapon.*` transforma o item em arma: ele passa a definir dano, intervalo entre golpes
  e alcance do auto-ataque. Arma exige `MainHand` ou `OffHand`.
- `visual.attach` pode se repetir (peças bilaterais). Os bones são do rig UAL.
- `visual.attack_*` define o clip de ataque; `attack_speed`, `attack_blend_in` e
  `attack_blend_out` são opcionais.
- Chave desconhecida é erro, com arquivo e linha. O lado do jogo ignora `visual.*`;
  o lado visual ignora o resto.

## Fluxo

```text
content/items/**/*.mmoitem
   ├─ game::items::ItemCatalog          (regras; o servidor só precisa disto)
   └─ presentation::ItemContent.visuals (modelo, encaixes, clip)

Inventory --equip_from--> Loadout --main_hand_weapon--> CombatSystem::set_weapon (dano/alcance)
                             │
                             └─ LoadoutPresenter::sync --> EquipmentManager --> EquipmentPresenter --> Renderer
                                  (constrói Weapon/BasicEquipment)          (CombatController usa o clip)
```

- `Inventory` e `Loadout` são as regras: espaço, empilhamento, troca ao equipar e
  devolução ao desequipar. Toda operação é tudo-ou-nada.
- `Loadout::revision()` muda a cada alteração; o `LoadoutPresenter` só reconstrói o
  visual quando ela muda.
- `app::PlayerItems` junta tudo no cliente. Quando houver servidor, `equip`/`unequip`
  viram mensagens e a resposta do servidor altera o mesmo `Loadout`.

## Adicionar um item

1. Crie o modelo (`.glb`/`.gltf`) em `content/items/<nome>/`, com origem no ponto
   de pegada e escala em metros.
2. Crie `content/items/<nome>/<nome>.mmoitem` com um `id` novo.
3. Rode `ctest` e confira se `ItemRulesTest` e `WoodenSwordTest` continuam passando;
   arquivos inválidos fazem `load_item_content` falhar com arquivo e linha.
4. Para testar no jogo, dê o item ao jogador em `application.cpp` (`items.give("<key>")`)
   e equipe pela janela de inventário (**I**).

Não é preciso código C++ novo para um item que só muda dados. Comportamento único
(efeito ao acertar, por exemplo) deve entrar como uma regra em `game/` acionada por
uma chave do item, não como subclasse por item.

## Controles no cliente

- **I**: abre o inventário. Passe o mouse sobre um item para ver os atributos.
- **F4**: alterna a arma da mão principal (a primeira arma de `MainHand` da bolsa).
- Com a espada, o auto-ataque usa `Sword_Attack` e o dano dela; sem arma, volta ao
  `Punch_Jab` e aos valores desarmados de `CombatSettings`.

## Limites atuais

Ainda não há IDs por instância, durabilidade, persistência, drop/loot, raridade
nem itens de NPC. O `ItemStack` guarda apenas tipo e quantidade; IDs de instância
entram junto com a persistência.
