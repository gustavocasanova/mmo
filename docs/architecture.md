# Arquitetura técnica

## Princípios

- Construir primeiro um **monólito modular**: módulos bem delimitados, poucos processos e um único deploy de servidor. Microserviços prematuros aumentariam o custo operacional de uma equipe solo.
- O servidor é a autoridade para movimento validado, combate, inventário, economia, quests e persistência. O cliente envia intenção e renderiza resultados; nunca decide resultados persistentes.
- A simulação do mundo não depende de janela, GPU, renderer ou bibliotecas gráficas. `mmo_server` não pode linkar GLFW, GLAD ou OpenGL.
- Renderer próprio significa implementar recursos, materiais, cenas e passes da engine. OpenGL é a API usada pelo backend atual; não espalhar chamadas OpenGL fora de `renderer`.
- Conteúdo, regras e protocolo devem ser versionados e testáveis sem abrir uma janela.
- A inspiração visual é fantasia nórdica estilizada, tomando Valheim como referência de clima e linguagem visual, não como conteúdo a copiar. Não reutilizar código, modelos, texturas, mapas, nomes ou dados de jogos comerciais.

## Visão de componentes

```text
mmo_client
  app -> core -> platform -> renderer
                     |         |
                     |      resources / scene / animation
                     +-> input / audio / UI
  game_client -> shared (regras determinísticas e protocolo)
  network_client <-> gateway -> zone/instance

mmo_server (sem dependência de GPU)
  auth/session -> gateway -> zone workers -> persistence
                                  |               |
                                  +-- shared -----+-> PostgreSQL
```

Começar com `mmo_client` e um executável local. Depois, cliente e servidor passam a ser processos separados, mas continuam no mesmo repositório e compartilham apenas tipos/protocolos explicitamente neutros. O caminho para escala é particionar o mundo em zonas/instâncias e distribuir essas unidades entre processos; não tentar simular um mundo inteiro num único processo para sempre.

## Direção de arte

Meta provisória: fantasia nórdica estilizada em terceira pessoa, com silhuetas simples e legíveis, formas de terreno e arquitetura marcantes, materiais com aspecto pintado, paletas distintas por bioma, vegetação densa e iluminação/fog atmosféricas. Criar modelos, texturas, nomes, mundo e identidade visual originais; Valheim é referência de alto nível, não especificação de assets.

Validar a direção com uma cena pequena: terreno, rochas, árvores, água e um personagem autoral. O renderer deve suportar câmera de terceira pessoa, iluminação direcional, sombras, fog, sky, materiais texturizados e instancing de vegetação, mas cada recurso entra com a cena que o comprova. Não implementar um renderer genérico completo antes desse protótipo visual.

## Limites dos módulos

| Módulo | Responsabilidade | Não deve depender de |
| --- | --- | --- |
| `core` | logging, tempo, configuração, IDs, erros | GLFW, OpenGL, regras de jogo |
| `platform` | janela, eventos, input, filesystem/clock do SO | regras do mundo, recursos OpenGL |
| `renderer` | dispositivo, buffers, shaders, materiais, render passes | gameplay, rede, banco |
| `resources` | localizar, carregar, validar e cachear assets | API gráfica nos formatos de domínio |
| `scene` | câmera, transformações, visibilidade e representação visual | autoridade de gameplay |
| `assets` | dados CPU de modelos, meshes, materiais, texturas e contrato de loader | OpenGL, GLFW, gameplay |
| `animation` | bones, skeleton, clips, pose, controller e matrizes de skinning | renderer, gameplay, rede |
| `character` | corpo visual, modelo do personagem, aparência e dados básicos separados | API OpenGL, inventário |
| `equipment` | slots, itens visuais e attachments resolvidos contra o skeleton | renderer, inventário, banco |
| `game/shared` | tipos e regras determinísticas compartilháveis | renderer, SO, banco |
| `game/client` | input, apresentação, predição e reconciliação | autoridade persistente |
| `game/server` | simulação autoritativa, validações, zonas e sistemas | qualquer biblioteca de cliente |
| `network` | transporte, sessão, serialização e versionamento | tipos OpenGL/GLFW |
| `persistence` | repositórios, transações e migrações | renderer e UI |
| `tools` | importação, validação e inspeção de conteúdo | dependência de runtime do cliente |

Não criar uma interface genérica para vários backends gráficos agora. `renderer` já é a fronteira; implementar um segundo backend só quando existir necessidade concreta e orçamento para manter ambos.

## World Editor

O `mmo_editor` mantém seleção e transformações no lado CPU, sem dependência de OpenGL. `SelectionManager` armazena os bounds locais e o transform de cada objeto selecionável, realiza raycasts em espaço local e fornece bounds mundiais. O cliente encaminha esses transforms ao `renderer` para desenhar os objetos e os gizmos; bounds mundiais também atualizam a colisão aproximada do mundo.

M/R/T selecionam mover/rotacionar/escala, X/Y/Z/U selecionam eixo, G alterna snap e C alterna espaço World/Local. A UI de ImGui fica no cliente; a barra superior também expõe botões para modos de transformação, snap e espaço, além das ferramentas de terreno, pincel, material e raio. O painel lateral reúne objetos e assets em abas; o Inspector permanece à direita e a viewport aceita drag & drop. SelectionManager também oferece as operações CPU de renomear, duplicar, apagar e editar transforms exibidas pelos painéis.

O `AssetDatabase` do editor indexa GLB/glTF nos diretórios de conteúdo configurados e manifests `.mmoprefab` versionados sob `content/prefabs/`. Um prefab referencia um modelo com caminho relativo ao projeto; o Asset Browser permite busca, criação e drag & drop para a viewport. O renderer converte nós de modelos estáticos em geometria centrada para desenhar e selecionar instâncias. Modelos com skin, materiais/texturas visuais, persistência de cenas e undo/redo continuam pendentes; falhas de carregamento ou posicionamento são mostradas no painel.

## Estrutura de pastas planejada

```text
src/
  apps/
    client/main.cpp
    server/main.cpp
  core/                    # logging, clock, config, IDs
  platform/                # window, input, filesystem
  renderer/
    renderer.hpp           # API usada por core/app; sem tipos GL
    opengl/                # implementação OpenGL e shaders
  resources/               # asset catalog, loaders, cache
  math/                    # convenções próprias sobre GLM
  scene/                   # câmera, transform e visibilidade
  animation/               # skeletons, poses and animation clips
  audio/
  network/                 # transporte e codecs
  persistence/             # PostgreSQL e migrations
  game/
    shared/                # protocolo e regras comuns
    client/                # apresentação e predição
    server/                # autoridade e simulação
  tools/                   # importadores/validadores
tests/
  unit/
  integration/
content/
  source/                  # fontes editáveis, por exemplo Blender
  cooked/                  # assets preparados para runtime; fora do Git se grandes
config/
  client/
  server/
ops/
  docker/
  migrations/
docs/
```

Criar diretórios e bibliotecas apenas quando houver código para eles. O primeiro passo usa `src/core`, `src/platform` e `src/renderer`; essa estrutura pequena é deliberada.

## Dependências por fase

### Usadas agora

| Dependência | Uso | Origem |
| --- | --- | --- |
| C++20 / MSVC | linguagem e compilador no Windows | Visual Studio 2022 |
| CMake 3.21+ | configuração e build | instalado no Windows |
| vcpkg | dependências C++ reproduzíveis | instalação do usuário |
| GLFW 3 | janela, contexto e input inicial | port `glfw3` |
| GLM | câmera, projeção e vetores do renderer | port `glm` |
| fastgltf | importar GLB/glTF para dados de modelo CPU | port `fastgltf` |
| GLAD 2 | carregar OpenGL 3.3 Core | código vendorizado em `third_party/glad` |
| OpenGL | API gráfica do renderer atual | driver; `opengl32` no Windows |

### Adicionar quando o marco precisar

| Dependência candidata | Introduzir para | Observação |
| --- | --- | --- |
| Catch2 | testes unitários/integrados | teste de regras sem janela/GPU |
| spdlog | logging estruturado e sinks | antes de multiplicar executáveis |
| nlohmann-json | config e ferramentas | não usar JSON no tráfego frequente de gameplay |
| Dear ImGui | ferramentas e overlay de debug | ferramenta de desenvolvimento, não UI final do jogador |
| ENet ou transporte equivalente | sessões de jogo com mensagens confiáveis e não confiáveis | validar latência, segurança, manutenção e licenciamento antes da escolha |
| PostgreSQL + libpqxx | contas, personagens e persistência | primeiro armazenamento durável; migrations desde o início |
| OpenSSL | TLS/integração segura quando necessária | não inventar criptografia |
| miniaudio | áudio do cliente | só quando houver marco de áudio |

Não instalar a lista futura toda agora. Fixar versões/ports no manifesto quando o código começar a consumir cada biblioteca.

O alvo `mmo_character` contém assets, skeleton/animation, character e equipment sem links para GLFW, GLAD ou OpenGL. `mmo_client` usa essa biblioteca para a apresentação. `assets::GltfModelLoader` usa fastgltf para ler GLB/glTF e validar os dados CPU.

`assets::Model` representa dados CPU. `renderer::Mesh` suporta vertices estáticos e quatro influências por vertex, e o vertex shader possui skinning com paleta de até 66 bones em uniform buffer. O cliente converte as meshes do modelo carregado em buffers e atualiza as matrizes de skinning. Texturas ainda não são enviadas à GPU.

O corpo base é um `CharacterBody` que referencia um model com skeleton. `CharacterModel` combina corpo, `AnimationController`, aparência e `EquipmentManager`. Equipment é dado independente por slot; `EquipmentManager::equip` valida o formato `.glb`/`.gltf`, attachments e bones antes de substituir o slot. Vários attachments por item cobrem peças bilaterais. O manager não carrega modelos nem chama OpenGL. `Character` mantém transform e stats mínimos.

O cliente usa `Universal Animation Library[Standard]/Unreal-Godot/UAL1_Standard.glb` como modelo local, com 43 clips e rig humanoide. As animações `Idle_Loop`, `Walk_Loop`, `Jog_Fwd_Loop` e `Jump_Loop` atendem aos estados de repouso e locomoção; o pulo em Espaço encadeia início, loop e aterrissagem. A/D movem para esquerda/direita. Texturas ainda não são enviadas à GPU. O CTest valida o asset e as poses dos clips.

## World Editor — Marco 1

`mmo_editor` contém `editor::WorldEditor`, `EngineMode` e a câmera CPU-only
`editor::EditorCamera`, sem dependência de GLFW ou renderer. A aplicação cliente é a
camada de composição: consulta F1, pausa integralmente `CharacterController::update`
no modo World Editor e volta a atualizá-lo em Game Mode. O personagem permanece vivo
no mesmo estado; não é destruído nem recriado.

F1 alterna entre Game Mode e World Editor Mode por borda de tecla (segurar F1 não
alterna repetidamente). No editor, a câmera fly começa na pose da câmera de jogo:
WASD voa relativo à orientação, E/Q sobe/desce, botão direito + mouse olha, Shift
acelera, Ctrl permite movimento preciso e a roda ajusta a velocidade configurável.
A câmera orbital do jogo não é substituída e sua pose fica preservada enquanto o
editor está ativo. F2 ativa/desativa as ferramentas de terreno já existentes para
liberar E como movimento vertical; com as ferramentas ativas a roda ajusta o pincel.
`editor::SelectionManager` lança raio contra bounds AABB dos objetos selecionáveis,
seleciona o hit mais próximo com clique esquerdo e limpa a seleção ao clicar no vazio.
Os colliders estáticos da cena de demonstração servem como alvos iniciais; o renderer
destaca o alvo selecionado com uma cor de seleção. Os IDs atuais são locais à cena de
demonstração, não identidades persistentes de entidades. WASD do editor usa o sentido
de strafe visual: D move para a direita da tela e A para a esquerda. F enquadra o centro
do objeto selecionado à distância configurada. Órbita e pan aguardam um marco de câmera
avançada. Gizmos, UI, undo/redo e serialização de entidades também não fazem parte deste
marco.

## Rede, simulação e persistência

- Simulação de servidor com passo fixo; renderização do cliente pode variar independentemente. Medir a frequência e o custo por zona antes de otimizar.
- Para movimento, o cliente prevê localmente e reconcilia com snapshots do servidor. Combate, loot, cooldowns e inventário são validados no servidor.
- Separar mensagens confiáveis (login, inventário, ações) de atualizações transitórias (posição/estado). O transporte deve suportar canais apropriados; não implementar criptografia ou protocolo UDP artesanal.
- Definir IDs estáveis e versões de protocolo. Não serializar structs C++ diretamente: padding, endianness e evolução de schema tornam isso frágil.
- PostgreSQL guarda estado durável; estado quente da zona permanece em memória e persiste em pontos definidos. Começar com um processo autoritativo e uma zona; particionar por zonas/serviços quando medições e metas de capacidade justificarem. Manter auth, gateway e mundo como módulos no começo; separar em processos deployáveis quando escala/isolamento operacional pedir.
- O mundo deve ser orientado a dados e carregado por regiões/biomas. Streaming do cliente reduz memória/gráficos; no servidor, zonas/instâncias delimitam propriedade da simulação e persistência.
- Cada zona usa partição espacial e interest management: o servidor envia a cada cliente apenas entidades/eventos relevantes à sua área de interesse, com limites e prioridades explícitos. AOI reduz tráfego, mas não elimina o custo de simular muitas entidades no mesmo lugar.
- Planejar handoff entre zonas, reconexão e recuperação de processo. Não prometer um mundo contínuo sem loading até existir um protótipo de transferência entre dois workers.
- Segredos e credenciais ficam fora do repositório. O cliente nunca conecta diretamente ao banco.

## Definição de “massivo”

“Massivo” precisa virar números antes de dimensionar hardware, transporte, tick rate ou banco: jogadores simultâneos totais, pico por zona, tamanho do grupo/raid, quantidade de entidades ativas, latência regional e orçamento mensal de infraestrutura. Até esses valores serem definidos, não há uma alegação honesta de capacidade final.

Plano de validação incremental: clientes reais para a experiência e bots determinísticos para carga; medir uma zona, depois várias zonas, incluindo o pior caso de concentração de jogadores. Registrar CPU por tick, memória por entidade, bytes/s por cliente, latência p95/p99 e tempo de recuperação. A arquitetura começa com uma zona, mas os limites entre gateway, zona, persistência e protocolo deixam espaço para escalar horizontalmente.

## Ambiente de desenvolvimento e distribuição

Desenvolver o cliente no Windows 11/VS 2022 x64. Planejar o servidor para Linux desde a separação do primeiro executável, com build/teste Linux em CI antes de produção. Conteúdo artístico passa por pipeline Blender -> formato de intercâmbio -> validação/cook -> runtime; manter fontes e derivados separados.

## Estado executável atual

`mmo_client` abre uma janela OpenGL 3.3 Core e renderiza terreno quadriculado e o modelo rigged local com câmera orbital suavizada. `CameraController` concentra sensibilidade, zoom, foco, elevação e suavização; `Application` controla movimento local, carregamento do personagem e atualização das animações; `Window` encapsula GLFW, captura do mouse e perda de foco. A lógica CPU segue coberta por dados sintéticos. Texturas ainda não são enviadas à GPU. A câmera evita atravessar o plano do chão. Ainda não há geometria de cenário, colliders ou raycast para bloquear paredes/árvores; a simulação autoritativa, rede e servidor também não existem.

### Ferramenta inicial de terreno

`game::world::Terrain` guarda um heightfield CPU finito de 384 × 384 células, com
amostragem, escultura radial, interseção de raio e leitura/escrita versionada. A malha
de runtime é dividida em 24 × 24 chunks de 16 × 16 células, mantendo coordenadas,
alturas e normais contínuas nas bordas. O renderer mantém meshes OpenGL apenas na
janela de chunks ao redor do foco da câmera e as cria/remove quando essa janela muda;
os dados CPU também são páginas de região com LRU limitado a 128 regiões; páginas
sujas são persistidas em `demo.mmoworld.regions/region_<x>_<z>.mmohm` e carregadas
ao consultar uma área que saiu da cache. O controlador consulta a altura ao caminhar e
aterrissar, com limites de movimento iguais aos limites do terreno.

No cliente, `F1` alterna o World Editor e `F2` ativa as ferramentas de terreno. Os
botões 1/2/3 selecionam elevar, baixar e nivelar; 4 alterna para o pincel de material,
onde 1/2/3/5 pintam grama, terra, rocha e areia. O botão esquerdo aplica o pincel e a
roda altera seu raio. O material usa pesos por vértice com interpolação suave e cores
procedurais no shader; ainda não há importação nem pintura de arquivos de textura. `Ctrl+S`
e a barra do editor salvam o documento completo em
`content/worlds/demo.mmoworld`; `Ctrl+L` carrega terreno e objetos. O documento de
texto versionado inclui a heightmap e referências/transformações dos objetos. Heightmaps
legados de 96 × 96 células são centralizados no mundo ampliado; os formatos antigos
sem dados de material assumem grama. `*.mmoterrain` ainda pode ser carregado quando
não existe um documento de mundo. O documento principal
mantém a heightmap completa para portabilidade; a pasta de regiões é o backing store
usado em runtime e reconstruído ao carregar o documento.
Undo/redo mantém até 128 estados e agrupa pinceladas e arrastes em operações únicas.
Carregamentos inválidos falham explicitamente e não substituem os dados atuais.
