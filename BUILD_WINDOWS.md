# Como compilar o ATDT SYNTH MACHINE no Windows (passo a passo)

Este guia parte do zero. Voce nao precisa saber programar.
Tempo total na primeira vez: cerca de 30 a 60 minutos (a maior parte e download).

---

## Passo 1 - Instalar o Visual Studio 2022 Community (gratis)

1. Acesse https://visualstudio.microsoft.com/pt-br/downloads/
2. Baixe o **Visual Studio 2022 Community**.
3. Abra o instalador. Na tela "Cargas de trabalho", marque:
   **[x] Desenvolvimento para desktop com C++**
4. Clique em **Instalar** e aguarde (sao varios GB).
5. Reinicie o computador ao final.

> Voce nao vai precisar abrir o Visual Studio. Ele so fornece o compilador.

## Passo 2 - Instalar o CMake

1. Acesse https://cmake.org/download/
2. Baixe o **Windows x64 Installer** (arquivo `.msi`).
3. Durante a instalacao, marque:
   **(o) Add CMake to the system PATH for all users**
4. Conclua a instalacao.

## Passo 3 - Instalar o Git

1. Acesse https://git-scm.com/download/win
2. Baixe e instale o **64-bit Git for Windows Setup**.
3. Pode aceitar todas as opcoes padrao (clique em *Next* ate o fim).

## Passo 4 - Conferir se deu tudo certo

1. Aperte a tecla **Windows**, digite `cmd` e abra o **Prompt de Comando**.
2. Digite e aperte Enter:
   ```
   cmake --version
   ```
   Deve aparecer algo como `cmake version 3.xx`.
3. Digite:
   ```
   git --version
   ```
   Deve aparecer `git version 2.xx`.

Se algum disser "nao e reconhecido como um comando", reinicie o computador e tente de novo.
Se continuar, reinstale marcando a opcao de PATH.

## Passo 5 - Colocar o projeto numa pasta simples

1. Extraia o `.zip` do projeto.
2. Mova a pasta para um caminho **curto e sem acentos**, por exemplo:
   `C:\Dev\ATDT-METAL-MACHINE`

> Caminhos com acentos ou muito longos (ex.: dentro de OneDrive/Documentos) causam erros.

## Passo 6 - Compilar

1. Abra a pasta `C:\Dev\ATDT-METAL-MACHINE`.
2. De **dois cliques** em `build_windows.bat`.
3. Uma janela preta vai abrir. Na primeira vez ele baixa o JUCE (5 a 15 minutos).
4. Espere aparecer **=== PRONTO! ===**.

O plugin fica em:
```
build\MetalMachine_artefacts\Release\VST3\ATDT SYNTH MACHINE.vst3
```
E uma versao independente (sem DAW), util para testes, fica em:
```
build\MetalMachine_artefacts\Release\Standalone\ATDT SYNTH MACHINE.exe
```

## Passo 7 - Instalar o VST3

1. Clique com o **botao direito** em `install_vst3.bat`.
2. Escolha **Executar como administrador**.
3. Ele copia o plugin para `C:\Program Files\Common Files\VST3\`.

Se preferir fazer manualmente: copie a pasta `ATDT SYNTH MACHINE.vst3` inteira para
`C:\Program Files\Common Files\VST3\`.

## Passo 8 - Abrir no REAPER

Siga a secao **Instalacao no REAPER** do `README.md`.

---

## Recompilar depois de uma atualizacao

Quando voce receber arquivos novos do projeto, basta rodar de novo:
1. `build_windows.bat`
2. `install_vst3.bat` (como administrador)
3. **Feche o REAPER antes do passo 2**, senao o Windows bloqueia a copia.

---

## Solucao de problemas

**"Visual Studio 17 2022 could not find any instance of Visual Studio"**
O Visual Studio nao foi instalado com o C++. Abra o *Visual Studio Installer*,
clique em *Modificar* e marque **Desenvolvimento para desktop com C++**.

**"cmake nao e reconhecido como um comando interno"**
O CMake nao esta no PATH. Reinstale marcando *Add CMake to the system PATH*.

**Erro ao baixar o JUCE (FetchContent / git clone)**
Verifique sua internet. Alternativa: baixe o JUCE 8.0.9 em
https://github.com/juce-framework/JUCE/releases , extraia e renomeie a pasta para `JUCE`,
colocando-a dentro da pasta do projeto (ao lado do `CMakeLists.txt`). O build usa essa pasta
automaticamente.

**Erros estranhos depois de mudar algo**
Apague a pasta `build` inteira e rode `build_windows.bat` de novo.

**"Access denied" ao instalar**
Feche o REAPER e execute o `install_vst3.bat` como administrador.

**O Windows Defender/SmartScreen bloqueou o .bat**
Clique em *Mais informacoes* > *Executar assim mesmo*. Os scripts apenas chamam o CMake e copiam arquivos.

Se aparecer qualquer outro erro, copie o texto da janela preta e me envie.
