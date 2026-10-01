# Roadmap

## Estado atual

O projeto tem um protótipo 3D local com terreno procedural, personagem feito de cubos, iluminação direcional simples, câmera em terceira pessoa e movimentação WASD. Os componentes do Marco 002 estão implementados, com parte do Marco 003 já disponível.

A próxima etapa é completar o Mundo 3D do Marco 003, principalmente carregamento de modelos e texturas. A numeração exibida na janela não indica que todos os requisitos anteriores foram concluídos.

## Marco 001 — Janela e triângulo

Fundação inicial: janela OpenGL 3.3 Core, shader, triângulo, resize e encerramento com liberação de recursos. O protótipo atual já substituiu o triângulo por uma cena 3D.

## Marco 002 — Arquitetura do renderizador

Criar classes Window, Renderer, Shader, Mesh, Material e Camera. Separar o código gráfico do main.cpp e introduzir GLM.

Estado no código:

- [x] Window e Renderer separados em módulos.
- [x] Código gráfico fora do main.cpp.
- [x] Shader e Mesh separados, com implementação OpenGL e ownership por RAII.
- [x] Material descreve cor e padrão de superfície, separado do renderer.
- [x] Camera concentra visão/projeção; CameraController controla órbita, zoom e suavização.
- [x] GLM integrado para vetores, matrizes, transformações e projeção.

Critério de aceite: o cliente mantém a cena e os controles atuais usando os componentes acima, com build Debug e Release funcionando.

## Marco 003 — Mundo 3D

Carregar modelos e texturas, renderizar terreno, adicionar iluminação, criar uma câmera em terceira pessoa e implementar movimentação básica.

Estado no código:

- [ ] Carregamento de modelos: o personagem atual é gerado com cubos.
- [ ] Carregamento de texturas: o terreno atual usa cores e padrão procedural no shader.
- [x] Terreno básico procedural.
- [x] Iluminação direcional simples.
- [x] Câmera em terceira pessoa com órbita, zoom e suavização.
- [x] Movimentação WASD básica.

Os itens marcados representam funcionalidades presentes no código; a validação visual e dos controles continua necessária.

Critério de aceite: uma cena explorável com modelo e textura carregados de arquivos, terreno, iluminação, câmera em terceira pessoa e movimentação básica.

## Marco 004 — Personagens e combate

Implementar entidades, animações, atributos, criaturas controladas por IA, habilidades, vida, dano e inventário local.

A animação procedural de caminhada atual é apenas um protótipo visual. Character já encapsula movimento; ItemDefinition e ItemStack oferecem uma base de domínio para itens, ainda sem integração na cena ou inventário. Ainda não há os sistemas de entidades, IA e combate deste marco.

Critério de aceite: uma interação de combate offline com criatura controlada por IA, uso de habilidade, alteração de vida/dano e inventário local, com regras testáveis sem renderer.

## Marco 005 — Multiplayer

Desenvolver o servidor dedicado, protocolo de comunicação, sessões de jogadores, sincronização de movimento e validação de combate.

O servidor deve funcionar sem janela ou GPU e ser a autoridade sobre movimento e combate. Separar simulação e apresentação, definir tick fixo, IDs e mensagens versionadas como parte deste marco.

Critério de aceite: dois clientes conectados ao servidor dedicado, com sessões, movimento sincronizado e combate validado pelo servidor.

## Marco 006 — Persistência

Adicionar contas, personagens salvos, equipamentos, missões, banco de dados, recuperação após reinicialização e ferramentas administrativas.

Critério de aceite: contas e estado de personagens, equipamentos e missões sobrevivem ao reinício do servidor; ferramentas administrativas permitem consultar e administrar esse estado com controle de acesso.

## Marco 007 — Mundo persistente

Divisão em zonas, carregamento por proximidade, instâncias, grupos, guildas, economia e otimização para vários jogadores simultâneos.

Critério de aceite: validar os sistemas do marco em uma área pequena, incluindo transferência entre zonas, carregamento por proximidade e instâncias. Definir metas de jogadores simultâneos totais e por zona e medir carga, latência e recuperação antes de afirmar capacidade de escala.

## Critério de avanço

Cada marco deve produzir algo executável e verificável. Os critérios de aceite acima orientam a validação; funcionalidades antecipadas não substituem pendências dos marcos anteriores. Manter a simulação independente da GPU e priorizar uma área pequena jogável antes de expandir o mundo.
