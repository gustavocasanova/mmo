# Espada de treino de madeira

Asset original do projeto, criado por `scripts/generate-wooden-sword.py`.
Não utiliza modelos ou texturas externos. O jogo carrega os arquivos glTF/bin
versionados; Python só é necessário para regenerá-los.

- 280 triângulos, materiais em tons de madeira, sem textura.
- Coordenadas em metros, lâmina no eixo +Y, origem no centro do cabo.
- Comprimento total: 0,995 m; guarda: 0,29 m de largura.
- Definição: `wooden_sword.mmoitem` — ID 1001, slot `MainHand`, dano 25, um golpe a
  cada 1,5 s, alcance 2,6 m (desarmado: 20 de dano, 2,5 m).
- Encaixe específico do rig UAL: `hand_r`; ajustes ficam em `visual.attach` no mesmo arquivo.

Para regenerar, na raiz do repositório:

```sh
python3 scripts/generate-wooden-sword.py
```

O cliente inicia com a espada equipada. **F4** remove/equipa no modo de jogo e
**I** abre o inventário.
WASD/Shift/Space permitem observar a peça acompanhando as animações; o scroll
aproxima a câmera. Selecione um inimigo com Tab, aproxime-se e use **1** para
ligar o ataque automático: equipada usa `Sword_Attack`, sem espada usa `Punch_Jab`.
Também é possível percorrer os clips com `[` e `]`.

Ainda não há drop nem persistência ([limites do sistema de itens](../../../docs/items.md)). A pegada
usa a pose dos dedos do clip; não há IK ou pose adicional para fechar a mão.
