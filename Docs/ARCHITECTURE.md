# ATDT METAL MACHINE - Arquitetura

## 1. Arquitetura escolhida

C++20 + JUCE 8 + CMake. Formatos: VST3 (principal) e Standalone. CLAP e macOS entram
sem reescrever o codigo (JUCE/CMake ja suportam; basta adicionar ao `FORMATS`).

Principios:
- **Modular**: cada modulo em sua pasta, sem dependencias cruzadas desnecessarias.
- **Audio thread seguro**: sem alocacao, sem locks, sem leitura de arquivo em `processBlock`.
  Parametros lidos via `std::atomic<float>*` (sem busca por texto em tempo real).
- **Estado unico**: `AudioProcessorValueTreeState` guarda todos os parametros e e o que o
  REAPER salva no projeto. Presets (Fase 2) usam o mesmo formato.
- **Interface reflete o DSP**: displays leem os mesmos parametros que o motor de audio.
  Nada de controle falso: modulo nao implementado aparece como "fase X".

## 2. Componentes

| Componente | Arquivo | Status |
|---|---|---|
| PluginProcessor | `Source/PluginProcessor.*` | Fase 1 |
| PluginEditor / MainView | `Source/PluginEditor.*`, `Source/UI/MainView.*` | Fase 1 |
| Parametros | `Source/Parameters/` | Fase 1 |
| SynthEngine (vozes) | `Source/Synth/SynthEngine.h` | Fase 1 |
| SynthVoice | `Source/Synth/SynthVoice.*` | Fase 1 |
| Oscillator / Noise / Filter | `Source/DSP/` | Fase 1 |
| TempoSync | `Source/Sequencer/TempoSync.h` | Fase 1 (leitura) |
| MidiActivity / MidiLearn | `Source/MIDI/` | Fase 1 |
| Theme / LookAndFeel / Knobs / Displays | `Source/UI/` | Fase 1 |
| PresetManager + presets de fabrica | `Source/Presets/` | Fase 2 |
| LfoEngine, ModulationDefs (matriz), Macros | `Source/Synth/` | Fase 2 |
| ModPage, PresetBrowser | `Source/UI/` | Fase 2 |
| StepSequencer, Arpeggiator | `Source/Sequencer/` | Fase 3 |
| DrumEngine | `Source/Drums/` | Fase 4 |
| LoopEngine | `Source/Sequencer/` | Fase 5 |
| EffectsEngine | `Source/Effects/` | Fase 6 |

## 3. Fluxo MIDI -> Synth -> Audio

```
REAPER (MIDI + BPM + transporte)
   │
   ▼
processBlock()
   ├─ TempoSync.update()           le BPM/PPQ/play do REAPER
   ├─ keyboardState                junta teclado da tela + MIDI externo
   ├─ MidiActivity.scan()          LED de MIDI, ultima nota
   ├─ MidiLearn.process()          CC mapeado -> parametro (notifica o REAPER)
   ├─ LfoEngine.process()          4 LFOs globais (travados no PPQ do REAPER quando SYNC)
   └─ SynthEngine.renderNextBlock()   divide o bloco no instante exato de cada evento
          │  Note On/Off, velocity, CC64 sustain, pitch bend, CC1, aftertouch
          ▼
      SynthVoice x16 (polifonia + voice stealing)
          Mod Matrix (8 slots) avaliada por voz: LFOs, envelopes, velocity, MW, AT, note, macros
          OSC1 + OSC2 + OSC3 (unison ate 8) + SUB + NOISE  -> pan
          -> Filtro SVF estereo (cutoff + env + keytrack + mod wheel, drive, mix)
          -> AMP ENV x velocity
   ▼
Bass Mono (M/S) -> Master gain (suavizado, sem clicks) -> medidores -> saida estereo
```

## 4. Plano das fases

1. **Fase 1 (feito)**: VST3 funcional, 3 OSC + SUB + NOISE, filtro, envelopes, MIDI completo,
   MIDI Learn, estado salvo, interface principal com displays reais.
2. **Fase 2 (feito)**: presets e browser (categorias, busca, favoritos), 33 presets de fabrica,
   LFO x4, matriz de modulacao com arrastar-e-soltar, macros, mono/legato/glide, unison, bass mono.
   Pendente para fases futuras: oscilador FM e wavetable (presets "FM" entram junto com eles).
3. **Fase 3**: step sequencer 16/32 passos (nota, velocity, gate, slide, accent, mute),
   arp, sincronia com o BPM, KEY/SCALE.
4. **Fase 4**: drum engine com sintese + samples WAV licenciados, padroes iniciais.
5. **Fase 5**: loop engine procedural (1/2/4/8 compassos).
6. **Fase 6**: efeitos em 6 slots + sidechain/pump.
7. **Fase 7**: performance view, randomize controlado, refinamentos visuais.
8. **Fase 8**: otimizacao e testes no REAPER (render offline, automacao, carga de CPU).
9. **Fase 9**: build final Windows x64.
