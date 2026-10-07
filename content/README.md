# Conteúdo artístico

Esta pasta é destinada aos arquivos de conteúdo; o código de carregamento fica em `src/assets/`.

Quando houver assets externos, organizar:

- `source/models/`: fontes editáveis e modelos de intercâmbio autorais.
- `source/textures/`: imagens e fontes de texturas.
- `cooked/`: derivados produzidos pelo futuro pipeline de importação.

Essas subpastas serão criadas junto com os primeiros arquivos. Hoje terreno e personagem são procedurais, e não há carregador de modelos/texturas ou processo de cook. Não salvar recursos GPU aqui. Definições e regras de itens/personagens pertencem a `src/game/`, independentes dos arquivos de aparência.

Os dados do heightfield editável são mundos de desenvolvimento, não assets artísticos:
o cliente cria `worlds/demo.mmoterrain` ao salvar pela primeira vez. O formato textual
versionado contém as alturas do terreno e pode ser carregado pelo modo de edição.
