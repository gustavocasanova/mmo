# Roadmap

## Marco 001 — Janela e triângulo

OpenGL 3.3 Core, shader, triângulo, resize e shutdown limpo. Critérios no README.

## Marco 002 — Esqueleto modular (agora)

Separar aplicação, janela e renderer; manter a tela atual idêntica. Definir limites de módulo e configurar build/execução pelo CMake Presets no VS Code.

## Marco 003 — Core testável e câmera

Clock, logging, matemática, câmera e input. Testes unitários não dependem de GPU.

## Marco 004 — Protótipo visual autoral

Provar a direção de fantasia nórdica estilizada com uma pequena área explorável, não um continente. Meshes, texturas, materiais, iluminação, fog e vegetação instanciada; assets originais, com Blender -> cook -> runtime.

## Marco 005 — Personagem local

Controlador em terceira pessoa, colisão simples, animação e uma interação. Fechar uma fatia jogável offline antes de criar sistemas MMO.

## Marco 006 — Simulação compartilhada

Entidades, atributos e regras determinísticas, isolados do renderer. Definir IDs, tick fixo, eventos e testes.

## Marco 007 — Servidor autoritativo local

Executável headless, uma zona e dois clientes em localhost. Login de desenvolvimento, movimento, snapshots e reconciliação. Definir modelo de zona, AOI e métricas desde este marco.

## Marco 008 — Persistência e identidade

PostgreSQL, migrations, contas/personagens, salvamento e recuperação após reinício. Segurança de sessão e limites de confiança.

## Marco 009 — Vertical slice online

Uma área pequena com criação de personagem, movimento em grupo, combate, loot, inventário e uma quest persistente. Medir carga e falhas; fixar meta de CCU total e pico por zona antes de declarar o sistema escalável.

## Marco 010 — Conteúdo e operação

Pipeline de assets, ferramentas, observabilidade, backups, deploy Linux, testes de carga, moderação e recuperação operacional.

## Marco 011+ — Crescimento do mundo

Adicionar biomas, zonas, conteúdo e sistemas com metas de capacidade explícitas. Validar handoff entre zonas e testes de concentração. Sharding, serviços separados e otimizações só entram quando medições mostrarem necessidade.

## Critério de avanço

Cada marco produz algo executável/testável e tem critérios de aceite antes de começar. Para equipe solo, priorizar uma fatia vertical pequena e original; “MMORPG completo” é uma meta de longo prazo, não um primeiro milestone.
