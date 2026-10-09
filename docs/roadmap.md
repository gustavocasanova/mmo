# Roadmap

## Estado atual

O projeto tem um protótipo 3D local com terreno procedural, personagem procedural com equipamentos interativos, iluminação direcional simples, câmera em terceira pessoa e movimentação WASD. Os componentes do Marco 002 estão implementados, com parte do Marco 003 já disponível.

A próxima etapa é completar o Mundo 3D do Marco 003, principalmente carregamento de modelos e texturas. A ferramenta de terreno/editor agora também oferece malhas em chunks, documento versionado do mundo e histórico de undo/redo. Isso não substitui o carregamento de assets nem a validação visual dos marcos anteriores. A numeração exibida na janela não indica que todos os requisitos anteriores foram concluídos.

## World Editor incremental

- [x] **Marco 1 — Editor Mode:** F1 alterna Game Mode/World Editor Mode e pausa/restaura o controlador do personagem sem destruir seu estado. Inputs da ferramenta de terreno só são aceitos no modo editor.
- [x] **Marco 2 — Editor Camera:** câmera fly dedicada, WASD, Q/E, mouse look (RMB), modificadores Shift/Ctrl e ajuste de velocidade na roda.
- [ ] Órbita e pan da câmera de edição ainda pendentes.
- [x] **Marco 3 — Selection:** raycast da câmera, selecionar/limpar com clique esquerdo, escolha do objeto mais próximo e highlight visual dos objetos selecionáveis atuais; F enquadra o alvo selecionado.
- [ ] **Marco 4 — Transform (implementação e testes CPU concluídos; validação visual pendente):** mover, rotacionar e redimensionar os objetos selecionáveis; gizmos nos eixos XYZ, seleção de eixo, espaço world/local e snap de grid/rotação/escala. Arraste com o botão esquerdo aplica a transformação no eixo ativo; M/R/T escolhem mover/rotacionar/redimensionar, X/Y/Z/U escolhem o eixo, G alterna snap e C alterna espaço.
- [ ] **Marco 5 — World Hierarchy (implementação e testes CPU concluídos; validação visual pendente):** painel gráfico Hierarchy/Inspector com seleção, renomear, editar transform, duplicar e apagar objetos.
- [ ] **Marco 6 — Asset Browser (implementação e testes CPU concluídos; validação visual pendente):** catálogo de GLB/glTF e prefabs versionados, busca, criação de prefab e arrastar assets para a viewport para instanciar modelos estáticos selecionáveis.
- [x] **Marco 7 — Terrain e persistência do editor:** malha do heightfield em chunks 16×16, salvar/carregar documento de mundo versionado com terreno e objetos, undo/redo com coalescência de pinceladas e arrastes, e barra visual com ações de persistência/histórico.
- [x] **Marco 8 — Streaming de meshes de terreno:** mundo de 384×384 células (24×24 chunks), meshes OpenGL carregadas sob demanda em torno da câmera e descarregadas ao sair da janela ativa. Arquivos legados de 96×96 células são migrados para o centro do mundo maior.
- [x] **Marco 9 — Streaming de dados CPU por região:** amostras divididas em regiões persistidas em sidecar, cache LRU limitada a 128 regiões, gravação de páginas sujas e carregamento sob demanda para amostragem, escultura e geração de mesh.
- [x] **Marco 10 — Pintura de materiais de terreno:** pesos de material por vértice, pincel radial com falloff e mistura suave, quatro camadas de paleta procedural (grama/terra/rocha/areia), persistência no documento e nas regiões sidecar e atualização seletiva de meshes.
- [x] **Polimento da UI do editor:** barra compacta com ações frequentes e modos, painel Scene com abas Objects/Assets, viewport central ampliada e controles visuais para ferramentas de terreno, pincel, material e raio; atalhos existentes preservados.
- [ ] Próximos marcos: materiais com texturas importadas e validação visual dos painéis/interações.

Validação CPU cobre modo, câmera, seleção, transformações, CRUD do Hierarchy/Inspector, catálogo de assets, manifests de prefab, conversão de modelos estáticos, terreno, streaming GPU/CPU por região, pintura de materiais, documento de mundo e undo/redo. Build do cliente e testes `TerrainTest`/`WorldEditorModeTest` passaram no Marco 10. Ainda falta confirmar visualmente na janela os painéis, o streaming durante deslocamento, a pintura de materiais e o drag & drop/instanciação. A paleta procedural não substitui o carregamento de texturas artísticas. Movimento e pulo do personagem permanecem exclusivos do Game Mode. Órbita/pan da câmera de edição ainda não estão disponíveis.

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

- [ ] Carregamento de modelos: o personagem atual é procedural; existe um contrato ModelLoader, mas nenhum parser GLB/glTF integrado.
- [ ] Carregamento de texturas: o terreno atual usa cores e padrão procedural no shader.
- [x] Terreno básico procedural.
- [x] Primeiro fluxo de edição do terreno: elevar, baixar, nivelar e salvar/carregar.
- [x] Iluminação direcional simples.
- [x] Câmera em terceira pessoa com órbita, zoom e suavização.
- [x] Movimentação WASD básica.
- [x] Cena interativa de equipamentos (1–9/T/0), preservada da main.

Os itens marcados representam funcionalidades presentes no código; a validação visual e dos controles continua necessária.

Critério de aceite: uma cena explorável com modelo e textura carregados de arquivos, terreno, iluminação, câmera em terceira pessoa e movimentação básica.

## Marco 004 — Personagens e combate

Implementar entidades, animações, atributos, criaturas controladas por IA, habilidades, vida, dano e inventário local.

A main adicionou Character/CharacterModel/CharacterBody, atributos básicos, skeleton, avaliação de animação/crossfade e EquipmentManager, testados com modelos sintéticos. A cena equipada ainda é procedural e estática; o componente anterior de animação em blocos continua separado. O movimento local e ItemDefinition/ItemStack permanecem nos módulos game. Ainda não há loader GLB real, inventário, IA ou combate integrado.

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
