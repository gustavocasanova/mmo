# Conteúdo artístico

Esta pasta é destinada aos arquivos de conteúdo; o código de carregamento fica em `src/assets/`.

Quando houver assets externos, organizar:

- `source/models/`: fontes editáveis e modelos de intercâmbio autorais.
- `source/textures/`: imagens e fontes de texturas.
- `cooked/`: derivados produzidos pelo futuro pipeline de importação.

Essas subpastas serão criadas junto com os primeiros arquivos. Hoje terreno e personagem são procedurais, e não há carregador de modelos/texturas ou processo de cook. Não salvar recursos GPU aqui. Definições e regras de itens/personagens pertencem a `src/game/`, independentes dos arquivos de aparência.
