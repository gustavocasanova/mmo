# Arquitetura técnica

O escopo e a ordem de entrega seguem o [roadmap](roadmap.md): Marco 002 — Arquitetura do renderizador; Marco 003 — Mundo 3D; Marco 004 — Personagens e combate; Marco 005 — Multiplayer; Marco 006 — Persistência; Marco 007 — Mundo persistente. Os componentes futuros abaixo são uma direção técnica, não funcionalidades já implementadas.

## Princípios

- Construir primeiro um **monólito modular**: módulos bem delimitados, poucos processos e um único deploy de servidor. Microserviços prematuros aumentariam o custo operacional de uma equipe solo.
- O servidor é a autoridade para movimento validado, combate, inventário, economia, quests e persistência. O cliente envia intenção e renderiza resultados; nunca decide resultados persistentes.
- A simulação do mundo não depende de janela, GPU, renderer ou bibliotecas gráficas. `mmo_server` não pode linkar GLFW, GLAD ou OpenGL.
- Renderer próprio significa implementar recursos, materiais, cenas e passes da engine. OpenGL é a API usada pelo backend atual; não espalhar chamadas OpenGL fora de `renderer`.
- Conteúdo, regras e protocolo devem ser versionados e testáveis sem abrir uma janela.
- A inspiração visual é fantasia nórdica estilizada, tomando Valheim como referência de clima e linguagem visual, não como conteúdo a copiar. Não reutilizar código, modelos, texturas, mapas, nomes ou dados de jogos comerciais.

## Visão de componentes

A estrutura implementada e suas dependências estão no [guia de módulos](modules.md).

```text
mmo_client / apps/client (composição)
  platform -> janela e input
  game/characters + game/items + game/world -> domínio sem GPU
  game/client -> representação visual -> scene + assets
  renderer -> scene + assets + backend OpenGL

mmo_server (planejado, sem dependência de GPU)
  auth/session -> gateway -> zone workers -> persistence
                                  |               |
                                  +-- domínio ----+-> PostgreSQL
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
| `assets` | localizar, carregar, validar e cachear assets | API gráfica nos formatos de domínio |
| `animation` | skeleton, clips, poses, skin matrices e crossfade CPU | GLFW e OpenGL |
| `character` | corpo, aparência, atributos e composição do modelo de personagem | OpenGL |
| `equipment` | slots e resolução de attachments em bones | OpenGL |
| `scene` | câmera, transformações, visibilidade e representação visual | autoridade de gameplay |
| `game/characters`, `game/items`, `game/world` | estado e regras de domínio compartilháveis | renderer, platform, assets, banco |
| `game/shared` (futuro) | protocolo e tipos de rede | renderer, SO, banco |
| `game/client` | input, apresentação, predição e reconciliação | autoridade persistente |
| `game/server` | simulação autoritativa, validações, zonas e sistemas | qualquer biblioteca de cliente |
| `network` | transporte, sessão, serialização e versionamento | tipos OpenGL/GLFW |
| `persistence` | repositórios, transações e migrações | renderer e UI |
| `tools` | importação, validação e inspeção de conteúdo | dependência de runtime do cliente |

Não criar uma interface genérica para vários backends gráficos agora. `renderer` já é a fronteira; implementar um segundo backend só quando existir necessidade concreta e orçamento para manter ambos.

## Estrutura de pastas planejada

```text
src/
  apps/
    client/main.cpp
    server/main.cpp
  core/                    # logging, clock, config, IDs
  platform/                # window, input, filesystem
  renderer/
    renderer.hpp           # API usada por apps/client; sem tipos GL
    opengl/                # implementação OpenGL e shaders
  assets/                  # dados CPU, catálogo e geometria; loaders futuros
  math/                    # convenções próprias sobre GLM
  scene/                   # câmera, transform e visibilidade
  animation/
  audio/
  network/                 # transporte e codecs
  persistence/             # PostgreSQL e migrations
  game/
    characters/            # estado e movimento
    items/                 # definições e pilhas de itens
    world/                 # limites e regras do mundo
    shared/                # protocolo futuro
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

Criar diretórios e bibliotecas apenas quando houver código para eles. A árvore acima inclui módulos futuros; o guia de módulos distingue o que já foi implementado. `core` não contém a aplicação nem regras de jogo; sua criação fica reservada a utilitários compartilhados concretos.

## Dependências por fase

### Usadas agora

| Dependência | Uso | Origem |
| --- | --- | --- |
| C++20 / MSVC ou GCC | linguagem e compilador | Visual Studio 2022 / GCC no Linux |
| CMake 3.21+ | configuração e build | Windows e Linux |
| vcpkg | dependências no Windows | manifesto; baseline ainda não fixada |
| GLFW 3 | janela, contexto e input inicial | vcpkg ou Fedora |
| GLM | matemática CPU, transformações e câmera | vcpkg ou Fedora |
| GLAD 2 | carregar OpenGL 3.3 Core | código vendorizado em `third_party/glad` |
| OpenGL | API gráfica do renderer atual | driver; `opengl32` no Windows |

### Adicionar quando o marco precisar

| Dependência candidata | Introduzir para | Observação |
| --- | --- | --- |
| Catch2 | testes unitários/integrados | teste de regras sem janela/GPU |
| spdlog | logging estruturado e sinks | antes de multiplicar executáveis |
| nlohmann-json | config e ferramentas | não usar JSON no tráfego frequente de gameplay |
| Dear ImGui | ferramentas e overlay de debug | ferramenta de desenvolvimento, não UI final do jogador |
| fastgltf | importar glTF 2.0 | Blender como fonte; pipeline próprio de validação/cook |
| ENet ou transporte equivalente | sessões de jogo com mensagens confiáveis e não confiáveis | validar latência, segurança, manutenção e licenciamento antes da escolha |
| PostgreSQL + libpqxx | contas, personagens e persistência | primeiro armazenamento durável; migrations desde o início |
| OpenSSL | TLS/integração segura quando necessária | não inventar criptografia |
| miniaudio | áudio do cliente | só quando houver marco de áudio |

Não instalar a lista futura toda agora. Fixar versões/ports no manifesto quando o código começar a consumir cada biblioteca.

Os alvos `mmo_character`, `mmo_assets` e `mmo_animation` preservam os módulos de modelos, skeleton/animation, character e equipment da main sem links para GLFW, GLAD ou OpenGL. A cena procedural continua independente de CharacterModel. O contrato `assets::ModelLoader` recebe caminhos `.glb`/`.gltf`, mas ainda não há implementação GLTF; nenhum parser próprio foi criado e nenhuma dependência de importação foi adicionada.

`assets::Model` representa dados CPU. `renderer::Mesh` suporta vertices estáticos e quatro influências por vertex, e o vertex shader possui skinning com paleta de até 48 bones. O renderer ainda não converte automaticamente `assets::Model` em buffers, não carrega imagens e não faz upload/amostragem de texturas.

O corpo base é um `CharacterBody` que referencia um model com skeleton. `CharacterModel` combina corpo, `AnimationController`, aparência e `EquipmentManager`. Equipment é dado independente por slot; `EquipmentManager::equip` valida o formato `.glb`/`.gltf`, attachments e bones antes de substituir o slot. Vários attachments por item cobrem peças bilaterais. O manager não carrega modelos nem chama OpenGL. `Character` mantém transform e stats mínimos.

O personagem procedural existente é somente placeholder da cena interativa `Character Equipment Test`; ele não representa um asset final nem consome ainda os dados do `CharacterModel`. O CTest usa mesh, skeleton e animações sintéticos, sem inventar um arquivo GLB.

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

O fluxo compartilhado de desenvolvimento do cliente usa Windows 11/VS 2022 ou VS 2026 x64. O CMake aceita compiladores e dependências nativas compatíveis; presets pessoais ficam em CMakeUserPresets.json, fora do Git. Planejar o servidor para Linux desde a separação do primeiro executável, com build/teste Linux em CI antes de produção. Conteúdo artístico passa por pipeline Blender -> formato de intercâmbio -> validação/cook -> runtime; manter fontes e derivados separados.

## Estado executável atual

Os componentes do Marco 002 estão implementados. Shader e Mesh cuidam dos recursos GPU; Material e Camera descrevem a cena; GLM fornece a matemática. Assets têm dados CPU e catálogo próprios. Character encapsula o movimento, e DemoScene monta o personagem equipado com a geometria procedural trazida pela main. CharacterVisual continua disponível como componente de animação em blocos, mas não é a cena ativa. ItemDefinition e ItemStack estabelecem o domínio de itens, ainda sem inventário ou itens visíveis.

`mmo_client` abre uma janela OpenGL 3.3 Core e renderiza o protótipo de terreno quadriculado e personagem em terceira pessoa. CameraController controla órbita e suavização; a aplicação conecta input, personagem, câmera e cena; Window encapsula GLFW; Renderer recebe DrawItems e não conhece jogadores ou itens.

Ainda não há carregamento de modelos/texturas, colisores de cenário, combate, rede, servidor ou persistência. A proteção de câmera continua limitada ao chão plano. Os controles 1–9/T/0 da cena de equipamentos e a correção de direção horizontal do mouse da main foram preservados. Build headless e testes CPU verificam os módulos sem GLFW/GLAD/OpenGL; isso ainda não constitui um servidor dedicado.
