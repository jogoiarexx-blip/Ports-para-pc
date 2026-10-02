# Final Fight X Native v0.3.37

Build validado contra OpenBOR v3.0 Build 3797.

Principais correcoes:
- Jogadores armados preservam o tipo efetivo PLAYER durante o combate.
- candamage controla corretamente os alvos que podem receber dano.
- Removido o bypass que fazia qualquer ataque acertar obstaculos.
- shell, arrow e knife_ respeitam o candamage do PAK e nao acertam obstaculos.
- Defaults legacy de candamage alinhados com Build 3797.
- hostile permanece separado para selecao de alvo da IA.
- followcond usa a elegibilidade de dano, como no motor legado.

Validacao:
- Windows x64 Release: SUCCESS
- v0.3.34: OK
- v0.3.35: OK
- v0.3.36: OK
- v0.3.37: OK
- Fase 1: walls=2 / spawns=22
- Assets do PAK: 1770/1770, sem faltantes, extras ou diferencas de conteudo

Branch de build: build-ffx-v0337
Workflow: Build Final Fight X v0.3.37
