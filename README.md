# ATDT SYNTH MACHINE

Instrumento virtual (VST3) para metal moderno, nu metal, metalcore, industrial,
trap metal e dubstep. Feito em C++ com JUCE. Plataforma principal: Windows x64 + REAPER.

> Versao atual: **0.17.0** - PRODUCER MODE: INSPIRE ME (estilo + clima + KEY + synth + beat + musica num clique).

---

## Jeito mais facil: instalador pronto (.exe)

Siga **[COMO_GERAR_O_INSTALADOR.md](COMO_GERAR_O_INSTALADOR.md)**. O GitHub compila no Windows,
testa todos os presets e entrega um instalador `.exe`. Voce nao precisa instalar ferramentas de programacao.
Compilar no proprio PC (Visual Studio) continua possivel: veja **BUILD_WINDOWS.md**.

---

## O que ja funciona (Fases 1 e 2)

Tudo abaixo esta ligado ao motor de audio real. Nada e decorativo.

**Sintese (aba SYNTH)**
- **3 osciladores**: Sine, Triangle, Saw, Square, Pulse, Noise (anti-aliasing PolyBLEP).
  OCT, SEMI, FINE, PHASE, LEVEL, PAN, com display da forma de onda em tempo real.
- **WAVETABLE** (WAVE = Wavetable): 7 tabelas originais geradas por sintese aditiva - Basic Morph,
  Metal Formant, Digital Grit, Vocal A-O, Growl, PWM Sweep, Harsh Sync. **WT POS** percorre a tabela
  (modulavel pela matriz). Versoes anti-aliasing por oitava: agudos limpos, sem chiado digital.
  O display mostra a forma real e, ao fundo, quadros verdadeiros da tabela (visao 3D).
- **FM** (WAVE = FM): modulacao de fase com **RATIO** (0.5 a 7) e **FM AMT** (modulavel pela matriz).
  Timbres metalicos e digitais para baixos e leads.
- Os controles de WAVETABLE/FM so aparecem quando essa forma de onda esta escolhida.
- **SUB** (Sine/Triangle/Square) e **NOISE** (White/Pink/Digital).
- **FILTER**: LP12, LP24, HP12, HP24, BP, NOTCH. CUTOFF, RESO, DRIVE, KEY TRK, MIX, com curva real.
- **FILTER ENV** e **AMP ENV** com pontos arrastaveis no grafico.
- **VOICE**: Poly / Mono / Legato, 1 a 32 vozes, GLIDE, UNISON (ate 8), DETUNE, SPREAD.
- **MASTER**: ganho (-60 a +18 dB), faixa do pitch bend, **MONO <** (grave mono abaixo da frequencia escolhida).

**Modulacao (aba MOD)**
- **4 LFOs**: Sine, Triangle, Saw, Square, S&H, Random, Reverse Saw. **SMOOTH** arredonda S&H/quadrada. **SYNC** com o BPM do REAPER
  (2/1 a 1/64 e tercinas) travado no compasso durante o play, ou RATE livre em Hz. PHASE, FADE, AMOUNT.
  O display mostra onde o LFO esta de verdade no audio.
- **ENV 3 (MOD ENV)**: envelope livre para modulacao, com grafico arrastavel.
- **Mod Matrix** com 8 slots: SOURCE, DESTINATION, AMOUNT, CURVE (Linear/Exp/Log), MODE (Normal/Invert).
- Fontes: LFO 1-4, Filter Env, Amp Env, Mod Env, Velocity, Mod Wheel, Aftertouch, Note, Random (por nota), Macro 1-4.
- Destinos: Pitch, Pitch de cada OSC, Level de cada OSC, Sub, Noise, Cutoff, Resonance, Drive,
  Filter Mix, Amp, Pan, Unison Detune, Amount de cada LFO, WT Position e FM Amount de cada OSC.
- **Arrastar e soltar**: a fileira **MOD SOURCES** (ao lado das abas) fica sempre visivel.
  Arraste um chip (ex.: LFO1) e solte sobre um knob (ex.: CUTOFF). O slot e criado na matriz
  e o knob ganha um **anel** mostrando a faixa de modulacao.
- **4 Macros** (AGGRESSION, MOVEMENT, DARKNESS, SPACE): duplo clique no nome para renomear.
  **EDIT MACRO** mostra na matriz so o que as macros controlam.

**Presets**
- **33 presets de fabrica** com volumes equilibrados: Bass (11), Trap (1), Dubstep (2), Keys (7),
  Pads (3), Leads (4), Plucks (2), Arps (1), FX (2).
- **Browser**: clique no nome do preset no topo. Categorias, busca, favoritos (estrela), autor.
  Clique carrega na hora; setas cima/baixo navegam; Enter/duplo clique fecha; Esc fecha.
- Setas **< >** no topo trocam de preset (respeitando a categoria/busca do browser).
- **SAVE** / **SAVE AS** / **DELETE** (preset do usuario vai para a lixeira).
- Presets do usuario: `Documentos\ATDT Synth Machine\Presets\<CATEGORIA>\`.
  Sao arquivos `.atdtpreset` (XML): da para copiar e compartilhar.
- Os presets de fabrica tambem aparecem no **menu de presets do proprio REAPER** (programs do VST3).

**Biblioteca de fabrica (v0.8)**
- **~220 presets**: 52 feitos a mao + 169 gerados por receitas de sound design (`tools/gen_library.py`).
- Categorias: BASS (sub, metal, FM, wavetable, reese, neuro), DUBSTEP (growls), TRAP (808), BREAKDOWN,
  LEADS, PADS, ATMOSPHERE, KEYS, PLUCKS, STABS, ARPS, SEQUENCES, DRUMS, FX.
- Cada preset tem funcao musical definida e usa o motor inteiro: wavetable/FM, envelopes, LFO sincronizado,
  matriz de modulacao, macros (AGGRESSION / MOVEMENT / DARKNESS / SPACE) e efeitos.
- **Controle de qualidade automatico** (`tools/qc_library.py`), medido no audio de cada preset:
  volume nivelado (RMS -20 dBFS com pico maximo de -6 dBFS), sem clipping, sem silencio, sem notas presas,
  grave mono abaixo de 120 Hz nos baixos. Sons curtos (plucks/stabs) ficam limitados pelo pico de -6 dBFS
  de proposito: nivelar pelo RMS deixaria o ataque alto demais.
- Os presets de ARPS e SEQUENCES usam o sequencer real (segure uma tecla ou acorde).

**PRODUCER MODE - INSPIRE ME (aba PRODUCER, v0.17)**
- Um clique escolhe uma combinacao coerente de estilo + clima + KEY (9 receitas: Modern Metalcore/F# menor,
  Electronicore/D menor, Trap Metalcore/C# menor, Industrial/E frigio, Darkcore/G menor harmonica, Cinematic/A menor,
  Modern Trap Rock/B menor, Cyber Metal/D# menor, Emotional Metalcore/C dorico), carrega um synth da categoria que
  combina, gera o beat (aba DRUMS) e a musica completa (aba PRODUCER) e mostra o resumo com o BPM sugerido.

**SCRATCH MACHINE (aba SCRATCH, v0.16)**
- Toca-discos virtual: o audio e lido na posicao do prato, entao o pitch sai da VELOCIDADE do disco (como num vinil
  real): rapido = agudo, parado = silencio, para tras = reverso.
- **SYNTH SCRATCH**: 10 fontes sintetizadas originais (vozes AHH/OHH/EHH, stab, varredura de ruido, 808, chirp digital,
  golpe metalico, voz robotica, textura de vinil). **SAMPLE SCRATCH**: o seu WAV/AIFF/FLAC/MP3 (ate 30 s) com
  START, END, CENTER, FADE IN, FADE OUT. O sample volta com o projeto.
- **SCRATCH PAD**: disco + forma de onda. O disco gira com a velocidade real do audio; arraste o mouse no disco
  (circular) ou na forma de onda (horizontal) para arranhar. Playhead e zoom (rodinha).
- **19 padroes**: Baby, Forward, Backward, Chirp, Transformer, Tear, Flare, Orbit, Crab, Stab, Scribble, Slicer, Stutter,
  Reverse, Brake, Vinyl Stop, Rhythmic, Glitch, Random. RATE 1/1 a 1/32 + tercinas; SWING, REPEAT, VARIATION.
- **ENGINE**: SPEED, DEPTH, DIRECTION, CURVE, ATTACK, RELEASE, GATE, RESONANCE, TONE, NOISE, VINYL, PITCH, WIDTH,
  MOMENTUM (inercia do prato), ACCELERATION, DECELERATION, RANDOMNESS (nenhum golpe identico).
- **Disparo**: TRIGGER (tecla C4 = padrao selecionado; C#4-G#4 = 8 QUICK), SYNC (grade do REAPER), FREE;
  botoes TRIGGER, HOLD, RELEASE, LOOP. As outras notas continuam no synth.
- **SCRATCH FX**: filter, drive, saturation, bitcrush, delay (1/8 pontuada), reverb, stereo, vinyl noise, FX MIX, MASTER,
  com protecao de saida suave.
- **QUICK SCRATCH** (8 botoes + INTENSITY) e **RANDOM SCRATCH** com travas (PATTERN, MOTION, TONE, FX).
- **70 presets** (categoria SCRATCH): Classic, Hip Hop, Modern, Metal, Metalcore, Experimental, Performance.
  Gerados por receita (tools/gen_scratch.py) com verificacao de comportamento distinto; volume nivelado no audio
  (pico ~ -7 dBFS). Presets "(Play)" tocam com o play do REAPER.

**ATDT SYNTH MACHINE - novo layout (v0.14)**
- Novo nome: **ATDT SYNTH MACHINE** (antes ATDT METAL MACHINE). O codigo interno do plugin foi mantido: projetos do
  REAPER salvos com versoes anteriores continuam abrindo. O instalador remove a versao com o nome antigo.
- Pasta do usuario: `Documentos\ATDT Synth Machine` (a pasta antiga e copiada automaticamente na primeira abertura).
- Navegacao superior com icones: SYNTH, MOD, FX, SEQUENCER, DRUMS, PRODUCER (gerador de musica), REFERENCE.
- Status em tempo real: CPU, VOICE (ativas/maximo), BPM, KEY + LEDs MIDI, SYNC (play do host) e OUTPUT.
- Barra lateral de presets: busca (ENTER), biblioteca por categoria (com contagem; as setas < > passam a navegar na
  categoria escolhida), TAGS (busca por palavra), Factory / User / Reference / Favorites e card do preset atual.
- Barra de preset: < > nome, estrela de favorito, categoria, SAVE, SAVE AS, INIT, RANDOM (preset aleatorio da mesma
  categoria), MENU e osciloscopio da saida.
- Janela padrao 1520 x 940 (MENU > escala da interface para telas menores).

**REFERENCE LAB (aba REF LAB, v0.13) - FULL MIX ANALYSIS**
- Arraste a musica (WAV, AIFF, FLAC, MP3) ou use IMPORT. Analise 100% local, numa thread propria (o audio do REAPER
  nunca e afetado), com barra de progresso. Nada e enviado para servidores.
- Mede, sempre com **% de CONFIANCA**: BPM (autocorrelacao do envelope de ataques), KEY + escala (cromagrama harmonico
  com separacao harmonico/percussiva e peso do grave na tonica; mostra a 2a hipotese), espectro por faixas, perfil de
  notas, dinamica (RMS, pico, crest), estereo (largura, correlacao), linha do tempo de energia com eventos
  (IMPACT, secoes HIGH/LOW).
- **BEAT DNA**: kick, hats, half-time, sincope, trap, eletronico. **SOUND DNA**: sub, brilho, distorcao, estereo,
  densidade, escuridao, agressividade, impacto.
- Gera material ORIGINAL (nunca copia notas ou ritmos da referencia):
  **APPLY KEY**, **GENERATE ORIGINAL GROOVE** (Beat Engine), **GENERATE ORIGINAL SONG** (Song Generator),
  **CREATE PRESET** (Bass/Lead/Pad aproximado, salvo na categoria **REFERENCE** do navegador).
- **Cache** em `Documentos\ATDT Synth Machine\ReferenceCache` (reanalisar a mesma faixa e instantaneo; o cache se
  invalida sozinho quando o algoritmo melhora). A analise fica salva no projeto.
- Validado com faixas de gabarito: 140 BPM -> 139,9 | 160 -> 160,4 | 150 -> 150,2; A menor, D menor, F# menor corretos.
- **Ainda nao disponivel** (proximas etapas): separacao/restauracao de stems (Deep Analysis, exige modelo de IA
  embarcado), reconstrucao de synth com Auto Sound Matching iterativo, extracao de MIDI, analise de regiao.

**SONG GENERATOR (aba SONG, v0.12)**
- **GENERATE EVERYTHING**: cria uma ideia musical completa a partir do SONG DNA (Heavy, Electronic, Trap, Dark,
  Melodic, Modern, Texture, FX), STYLE (16 estilos), MOOD (Dark, Emotional, Epic, Aggressive, Melancholic, Tense,
  Cinematic), KEY/SCALE (aba SEQ) e BPM do projeto.
- **Estrutura**: Short / Standard (intro, verso, pre, refrao, verso 2, pre, refrao, breakdown, bridge, final, outro) /
  Extended (com pre-drop e drop). **Curva de energia** por secao decide as camadas.
- Pensa como produtor - os elementos se relacionam:
  HARMONIA por clima -> **MOTIF** de 3-6 notas que evolui (intro = pad lento, verso = pluck esparso, pre = sobe,
  refrao = lead, breakdown = riff grave, final = oitavas empilhadas) -> **RHYTHMIC LOCK** (kick nos acentos do motif)
  -> **808 = tonica de cada acorde** -> **SYNTH RIFF** preenche os espacos entre os kicks (call & response) ->
  ARP moderno (pausas, saltos de oitava, repeticoes) -> PAD com **sidechain (CC11)** no kick ->
  **FX nas transicoes** (riser 2 compassos antes, impacto, sub drop, stutter, downlifter, reverse, textura) ->
  **automacao** de filtro (CC74) no pre-chorus.
- **VOCAL SPACE** (verso com espaco para a voz), **QUICK IDEA** (so bateria, 808, riff e melodia).
- **SEED**: REGENERATE (mesma seed, depois de mudar o DNA) / NEW SEED. **LOCK** por elemento (DRUMS, 808, BASS,
  RIFF, MELODY, ARP, PAD, FX): elementos travados ficam iguais.
- **Clique numa secao da linha do tempo = PREVIEW**: bateria + 808 + synth riff carregados no motor; de PLAY no REAPER.
- **EXPORT SONG MIDI / DRAG SONG MIDI**: MIDI de 12 trilhas (Drums, Kick, Snare, Hats, Perc, 808, Bass, Synth Riff,
  Melody, Arp, Pad, FX) com marcadores das secoes. FX: C3 riser, D3 impact, E3 sub drop, F3 downlifter,
  G3 stutter, A3 reverse, B3 textura.
- Limite atual: o preview interno toca bateria, 808 e riff; pad, melodia e arp sao ouvidos no REAPER com o
  instrumento que voce escolher para cada trilha.

**BEAT ENGINE 2.0 (v0.11)**
- Algoritmo novo: espaco + groove + velocity + sincope + 808 + micro-fills + transicoes + contraste.
- Estilos novos: Modern Trap, Dark Trap, Modern Electronicore, Modern Trap Rock, Industrial Metalcore, Trapcore.
  Secoes novas: **Pre-Drop** (hats aceleram 1/16 -> 1/64 e silencio no ultimo 1/8) e **Drop** (impacto, crash, 808 cheio).
- Controles novos: **MODERNITY** (tradicional -> hibrido), **SYNCOPATION**, **SPACE** (menos golpes, mas o 1 e o
  backbeat ficam), **COMPLEXITY** (ghost notes, kick duplo de trap, micro-fills 1/32).
- **Rolls inteligentes**: so antes da caixa ou no fim da frase; 1/32, tercina ou 1/64; pitch subindo/descendo.
  Hats com dinamica FORTE/fraco/medio/fraco e microvariacao de pan.
- **808**: 8 tipos (Clean, Sub, Distorted, Digital, Metal, Reese, Short, Long), DRIVE e GLIDE; slide por passo.
- **MAKE IT**: MODERN, HEAVIER, MORE TRAP, ELECTRONIC, DARKER, BIGGER, TIGHTER, GROOVIER - transformam o beat
  atual sem destruir o groove e respeitam os cadeados.

**BEAT ENGINE (aba DRUMS, v0.10)**
- **GENERATE BEAT**: cria um beat novo por REGRAS de estilo (nao e sorteio): Modern Metal, Metalcore, Electronic Metal,
  Industrial, Nu Metal, Trap Metal, Electronicore, Dubstep Metal, Half-Time, Dark Electronic, Cinematic Metal, Hybrid.
- **SECTION**: Intro, Verse, Pre-Chorus, Chorus (ride + crash), Breakdown (half-time, espaco, impacto), Bridge, Outro.
- **A / B / C / D**: variacoes do MESMO beat (B redistribui kick e conducao, C mais denso, D com fill).
- **ENERGY, DENSITY, AGGRESSION, ELECTRONIC, TRAP, GLITCH**: mudam as regras (limiares, camadas, acentos).
- **HALF-TIME** (Auto / Full / Half) e **DOUBLE KICK** (Off, Light = ultimo tempo, Medium = tempos 3-4,
  Heavy = compasso inteiro, Extreme = com rajadas de 1/32).
- **Trap**: rolls de hat por RATCHET (2-4 golpes no passo: 1/32 e 1/64) e **808 na escala da KEY** (tonica, 5o, 7o,
  oitava) com slide entre notas diferentes. O 808 e um canal real do motor (afinado: KEY = E toca E1 = 41,2 Hz).
- **MAKE A FILL** (ultimo tempo: toms, roll de caixa, kick) e **RANDOMIZE** Subtle / Medium / Extreme
  (preserva estilo, secao, BPM e KEY).
- **Cadeado por canal**: clique no nome do instrumento; o gerador nao altera canais travados.
- Grade: **ALT+clique** = roll (ratchet 1-4); a linha 808 mostra a nota de cada passo.
- **EXPORT MIDI**: arquivo .mid (4 compassos, BPM do projeto) em `Documentos\ATDT Synth Machine\MIDI`.
  **MULTI-TRACK**: uma trilha por instrumento (o REAPER cria trilhas separadas ao importar).
  **DRAG MIDI**: arraste o botao para uma trilha do REAPER.

**Bateria (aba DRUMS)**
- 10 instrumentos **100% sintetizados** (nenhum sample): KICK, SNARE, CLAP, HAT, OPEN HAT, PERC, TOM, CRASH, RIDE, FX.
  O HAT fechado abafa o OPEN HAT, como numa bateria real.
- Grade de ate **64 passos** em 1/16: **clique** liga/desliga, **SHIFT+clique** = acento, **clique direito** = ghost note.
  Arraste para desenhar. A coluna destacada mostra o passo tocando.
- Por canal: **LVL, PAN, PITCH, DECAY** (mini-knobs; duplo clique volta ao padrao).
- **PLAY**: Transport (toca com o play do REAPER, sincronizado ao compasso) ou Always (toca sempre - util para ensaiar).
- **SWING**, **VOLUME** e o **TIME FEEL** da aba SEQ (half/double) valem para a bateria; o HUMANIZE tambem.
- **DRUM KEYS**: o teclado MIDI toca a bateria no mapa GM (36 kick, 38 snare, 39 clap, 42 hat, 46 open hat,
  37 perc, 41-50 tom, 49 crash, 51 ride, 52 fx). As outras notas continuam tocando o synth.
- **17 padroes originais**: METAL (Basic Rock, Double Kick, Gallop, Blast Beat), METALCORE (Heavy/Dark Half Time,
  Breakdown Chug, Half-Time Destroy), TRAP (Trap 01, Dark Trap, Metal Trap, Trap Metal), DUBSTEP, INDUSTRIAL,
  ELECTRONIC e HYBRID.
- **10 presets de kit** (categoria DRUMS): carregue e toque a bateria no teclado.
- O padrao de bateria faz parte do preset e fica salvo no projeto do REAPER.
- A bateria entra depois dos efeitos do synth (nao passa pela distorcao do baixo) e antes do MASTER/LIMITER.

**Visual ATDT AUDIO TOOLS (v0.9)**
- Identidade de hardware: chassis de aco escuro escovado, placas chanfradas com parafusos, knobs metalicos com
  escala gravada, chaves fisicas com LED, telas embutidas e mini-displays de valor.
- Vermelho neon so e funcional: controle ativo, sinal, LED, modulo ligado. Modulos em bypass ficam "apagados"
  e a faixa da cadeia de sinal acende so os efeitos ligados.

**Efeitos novos (v0.9)**
- **SATURATION**: Warm, Tape, Tube, Digital; DRIVE, MIX.
- **BITCRUSH**: BITS (2-16), DOWN (reducao de sample rate 1-64x), MIX.
- **PHASER**: RATE, DEPTH, FEEDBACK, MIX.  **FLANGER**: RATE, DEPTH, FEEDBACK, MIX (estereo).
- **GLITCH** sincronizado ao compasso do REAPER (1/2 a 1/64): **Stutter** (repete a 1a fatia do grupo),
  **Gate** (corte ritmico), **Reverse** (fatia de tras para frente), **Tape Stop** (desacelera ate parar); MIX.
- Ordem: SYNTH > DIST > SAT > CRUSH > EQ > CHORUS > PHASER > FLANGER > GLITCH > DELAY > REVERB > COMP > MASTER > LIMITER.

**Efeitos (aba FX)**
- Ordem do sinal: SYNTH > **DISTORTION** > **EQ** > **CHORUS** > **DELAY** > **REVERB** > **COMPRESSOR** > MASTER > **LIMITER**.
- Cada modulo tem **ON** na barra de titulo (liga/desliga com crossfade, sem clicks). Desligado nao gasta CPU.
- **DISTORTION**: Soft, Hard, Fuzz, Rectify; DRIVE, TONE, MIX (paralelo). Processada com oversampling 2x.
- **EQ**: LOW (120 Hz), MID (1 kHz), HIGH (8 kHz), +/-12 dB.
- **CHORUS**: RATE, DEPTH, MIX.
- **DELAY**: tempo sincronizado ao BPM (1/4, 1/8, 1/8 pontuada, 1/16, tercinas, 1/2), FEEDBACK, MIX, PING-PONG.
  As repeticoes vao escurecendo naturalmente.
- **REVERB**: SIZE, DAMP, WIDTH, MIX.
- **COMPRESSOR**: THRESH, RATIO, ATTACK, RELEASE, MAKEUP.
- **LIMITER** (ligado por padrao): teto de -0.3 dBFS - protege contra clipagem mesmo com o GAIN em +18 dB.
- O plugin informa ao REAPER uma cauda de 8 s: o render offline nao corta reverb nem delay.

**Sequencer (aba SEQ)**
- **MODE**: Off / Sequence / Arp. **TRIGGER**: Key (toca enquanto segura a tecla), Latch (continua apos soltar),
  Transport (toca junto com o play do REAPER, sem precisar de tecla).
- **RATE** 1/4 a 1/32 e tercinas; **TIME FEEL** Half / Normal / Double (tambem vale para os LFOs em SYNC).
- Ate **64 passos**: NOTE (grau da escala), VELOCITY, GATE, ACCENT, SLIDE, ON. Desenhe arrastando na grade;
  a coluna destacada mostra o passo tocando. Com LENGTH acima de 32 aparecem as paginas **1-32 / 33-64**.
- **SWING** (0-100%): atrasa os contratempos; 100% = sensacao de tercina. Vale para sequencia e arpejo.
- **TRANSP** (-24 a +24 semitons): transpoe a sequencia/arpejo; com SCALE LOCK o resultado continua na escala.
- Sincronizado ao BPM do REAPER com precisao de amostra (verificado no teste automatico a 120/150/180 BPM).
- **KEY / SCALE**: ROOT, Minor, Harmonic Minor, Phrygian, Phrygian Dominant, Dorian, Locrian, Major, Chromatic,
  OCTAVE e **SCALE LOCK** (arpejo e sequencia ficam sempre no tom).
- **ARPEGGIATOR**: Up, Down, Up/Down, Random, Order, Chord, 1 a 4 oitavas (usa o ritmo da grade de passos).
- **HUMANIZE**: variacao de velocity e de timing.
- **Biblioteca de sequencias** por secao: INTRO, VERSE, BUILDUP, CHORUS, BREAKDOWN, BRIDGE, OUTRO.
  As sequencias sao gravadas em tempos musicais: acompanham qualquer BPM.
- **SMART RANDOM**: gera sequencia nova sempre dentro da escala, com peso na tonica e nos tempos fortes.
- **VEL RANDOM**: sorteia so as velocities (tempos fortes mais altos), mantendo notas e ritmo.
- **MIDI OUT**: envia as notas geradas para o REAPER. Para gravar a sequencia como MIDI editavel:
  crie outra trilha, ponha a entrada dela como a saida MIDI da trilha do plugin e grave.
- **TEMPO**: Host (segue o REAPER) ou Internal (BPM proprio, com TEMPO PRESETS de 120 a 180).

**Geral**
- MIDI: notas, velocity, sustain (CC64), pitch bend, mod wheel (CC1), aftertouch.
- **MIDI Learn** no clique direito de qualquer knob. Salvo no projeto.
- Automacao de todos os parametros no REAPER; estado completo salvo no projeto.
- INIT, escala da interface (80-150%), temas METAL e CYBER (menu **MENU**).

A aba **PERFORM** mostra apenas o que vira numa fase futura (sem controles falsos).

## Como usar os knobs

| Acao | Resultado |
|---|---|
| Clicar e arrastar | ajusta |
| SHIFT + arrastar | ajuste fino |
| Duplo clique | volta ao valor padrao |
| Roda do mouse | ajusta |
| Clique direito | MIDI Learn / remover CC / remover modulacoes / valor padrao |
| Soltar um chip de MOD SOURCES | cria modulacao nesse knob |

---

## Requisitos

- Windows 10 ou 11, 64 bits
- Visual Studio 2022 Community (gratis), com "Desenvolvimento para desktop com C++"
- CMake 3.22 ou superior
- Git
- REAPER (64 bits)

O passo a passo completo, do zero, esta em **[BUILD_WINDOWS.md](BUILD_WINDOWS.md)**.

## Compilacao rapida

1. Dois cliques em `build_windows.bat`
2. Clique direito em `install_vst3.bat` -> **Executar como administrador**
3. No REAPER: *Options > Preferences > Plug-ins > VST > Re-scan*

## Instalacao no REAPER

1. O arquivo `ATDT SYNTH MACHINE.vst3` deve estar em:
   `C:\Program Files\Common Files\VST3\`
2. Abra o REAPER.
3. Va em **Options > Preferences > Plug-ins > VST**.
4. Confira se `C:\Program Files\Common Files\VST3` esta na lista de caminhos.
5. Clique em **Re-scan** (ou *Clear cache/re-scan*).
6. Feche as preferencias.
7. Menu **Track > Insert virtual instrument on new track...**
8. Procure por **ATDT SYNTH MACHINE** (em *VST3i*) e confirme.

## Configurando o teclado MIDI no REAPER

1. Conecte o teclado **antes** de abrir o REAPER.
2. **Options > Preferences > Audio > MIDI Inputs**: clique no seu teclado e marque
   **Enable input**. Clique em OK.
3. Na trilha do plugin, clique no botao vermelho **Record arm**.
4. Clique com o botao direito no botao Record arm -> **Input: MIDI > All MIDI inputs > All channels**.
5. Ative o **Record monitoring** (icone de alto-falante ao lado do Record arm).
6. Toque. O LED **MIDI** no topo do plugin acende a cada nota.

## Solucao de problemas

**O plugin nao aparece no REAPER**
- Confira se o arquivo esta mesmo em `C:\Program Files\Common Files\VST3\`.
- Faca *Clear cache/re-scan* nas preferencias de VST.
- Veja se o REAPER e 64 bits (o plugin e 64 bits).

**O plugin aparece mas nao faz som**
- A trilha precisa estar com **Record arm** + **Record monitoring** ligados e com a entrada MIDI certa.
- Clique no teclado virtual do plugin: se sair som ali, o problema esta na configuracao do MIDI.
- Veja se o **GAIN** do MASTER nao esta em -inf.

**Chiados ou falhas no audio**
- Em *Options > Preferences > Audio > Device*, use o driver **ASIO** da sua placa (ou ASIO4ALL)
  e aumente o buffer (256 ou 512 amostras).

**Mudei de preset e o volume ficou diferente do esperado**
- Os presets de fabrica foram nivelados entre -10 e -12 dBFS (nota unica para baixos, acorde para teclas/pads).
  O botao GAIN do MASTER faz parte do preset.

**O preset que salvei nao aparece**
- MENU > "Recarregar lista de presets". Confira a pasta em MENU > "Abrir pasta de presets do usuario".

**MIDI Learn nao responde**
- Mexa o knob/fader fisico **depois** de escolher "MIDI Learn" (o knob pisca enquanto espera).
- O pedal de sustain (CC64) nao pode ser mapeado: ele sempre controla o sustain.

---

## Estrutura do projeto

```
ATDT-METAL-MACHINE/
├── CMakeLists.txt          configuracao de build (baixa o JUCE sozinho)
├── build_windows.bat       compila no Windows
├── install_vst3.bat        instala o VST3 (executar como administrador)
├── COMO_GERAR_O_INSTALADOR.md   instalador .exe pelo GitHub (sem instalar ferramentas)
├── .github/workflows/      compilacao automatica no Windows + testes + instalador
├── installer/              script do instalador (Inno Setup)
├── README.md               este arquivo
├── BUILD_WINDOWS.md        passo a passo de compilacao
├── Docs/ARCHITECTURE.md    arquitetura, fluxo de sinal e plano das fases
├── Tests/PluginTest.cpp    teste automatico: toca todos os presets e verifica o audio
├── Samples/                samples WAV licenciados (vazio por enquanto)
└── Source/
    ├── PluginProcessor.*   ponto central: MIDI -> synth -> saida, estado
    ├── PluginEditor.*      janela do plugin (escala e tema)
    ├── Parameters/         todos os parametros automatizaveis
    ├── DSP/                oscilador, noise, filtro
    ├── Synth/              motor de vozes (SynthEngine, SynthVoice)
    ├── MIDI/               atividade MIDI, MIDI Learn
    ├── Sequencer/          sincronia de tempo com o REAPER (sequencer: Fase 3)
    ├── UI/                 tema, knobs, displays, paginas
    ├── Drums/              Fase 4
    ├── Presets/            PresetManager + 33 presets de fabrica
    ├── Effects/            Fase 6
    └── Utilities/
```

## Teste automatico (opcional)

```
cmake -B build -DMM_BUILD_TESTS=ON
cmake --build build --config Release --target PluginTest
build\PluginTest_artefacts\Release\PluginTest.exe "build\MetalMachine_artefacts\Release\VST3\ATDT SYNTH MACHINE.vst3"
```
Carrega o VST3 como um host, toca todos os presets e confere: gera audio, sem NaN/Inf,
nenhuma nota presa depois do release, estado salvo/restaurado.

## Dependencias

- **JUCE 8.0.9** (baixado automaticamente pelo CMake). Licenca: AGPLv3 ou licenca comercial JUCE.
  Para vender o plugin com codigo fechado, e preciso uma licenca comercial do JUCE.
- **VST3 SDK**: ja incluso no JUCE.
- Nenhum sample, preset ou codigo de terceiros proprietario esta incluido.
