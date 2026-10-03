# Como obter o instalador `.exe` do ATDT SYNTH MACHINE

Voce **nao precisa instalar nada** no seu computador (nem Visual Studio, nem CMake).
Um computador Windows gratuito do GitHub compila o plugin, **testa os 33 presets no Windows**
e entrega um instalador pronto. Tempo total: ~10 minutos seus + ~20 minutos esperando.

---

## Passo 1 - Criar uma conta no GitHub (gratis)

1. Acesse **https://github.com/signup**
2. Crie a conta com seu e-mail e confirme o codigo que chegar no e-mail.

## Passo 2 - Criar um repositorio (a "pasta" do projeto na nuvem)

1. Logado no GitHub, clique no **+** (canto superior direito) > **New repository**.
2. **Repository name:** `atdt-metal-machine`
3. Marque **Private** (so voce ve o codigo).
4. **Nao** marque nenhuma outra opcao (nada de README, .gitignore ou license).
5. Clique em **Create repository**.

## Passo 3 - Enviar os arquivos

1. Extraia o `.zip` do projeto no seu computador.
2. Abra a pasta extraida `ATDT-METAL-MACHINE` (a que tem `CMakeLists.txt`, `Source`, `.github`...).
3. No GitHub, na pagina do repositorio vazio, clique no link **uploading an existing file**.
4. Na pasta do Windows, aperte **Ctrl+A** (seleciona tudo) e **arraste** para a area do navegador.
   Use **Chrome** ou **Edge** (eles aceitam arrastar pastas).
5. Espere a lista de arquivos aparecer. **Confira se aparece `.github/workflows/build-windows.yml`**
   (e ele que manda o GitHub compilar). Veja "Problemas" abaixo se nao aparecer.
6. Role ate o fim e clique no botao verde **Commit changes**.

## Passo 4 - Esperar a compilacao

1. Clique na aba **Actions** (no topo do repositorio).
   - Se aparecer um botao pedindo para habilitar os workflows, clique para habilitar.
2. Voce vera **"Build Windows (instalador)"** rodando (bolinha amarela).
3. Espere de **15 a 25 minutos**. Pode fechar a pagina e voltar depois.
4. Quando terminar aparece um **✓ verde**.

> Durante a compilacao ele roda o **teste automatico**: toca todos os presets no Windows e verifica
> se ha audio, sem erros e sem notas presas. Se algo falhar, o check fica **vermelho** e nenhum
> instalador quebrado e gerado.

## Passo 5 - Baixar o instalador

1. Clique na execucao que ficou verde.
2. Role ate o fim da pagina, secao **Artifacts**.
3. Clique em **ATDT-METAL-MACHINE-Windows** (baixa um `.zip`).
4. Extraia. Dentro tem:
   - **`ATDT-Metal-Machine-0.17.0-Windows-x64-Setup.exe`** - o instalador (use este)
   - `ATDT SYNTH MACHINE.exe` - o app standalone avulso
   - `ATDT SYNTH MACHINE.vst3` - o plugin avulso (para quem prefere copiar na mao)

## Passo 6 - Instalar

1. Dois cliques em **`ATDT-Metal-Machine-0.17.0-Windows-x64-Setup.exe`**.
2. O Windows vai mostrar **"O Windows protegeu o computador"** (SmartScreen).
   Isso acontece com todo programa sem certificado digital pago. Clique em
   **Mais informacoes** > **Executar assim mesmo**.
3. Escolha **Completa** (VST3 + app standalone) e avance ate o fim.

O instalador coloca o plugin em `C:\Program Files\Common Files\VST3\` (onde o REAPER procura)
e cria o atalho do app standalone no menu Iniciar.

## Passo 7 - Abrir no REAPER

1. **Options > Preferences > Plug-ins > VST** > **Re-scan** > OK.
2. **Track > Insert virtual instrument on new track...** > **ATDT SYNTH MACHINE** (VST3i).

---

## Atualizacoes futuras

Quando eu te mandar uma versao nova:
1. No repositorio, **Add file > Upload files**, arraste os arquivos novos por cima e **Commit changes**.
2. A compilacao comeca sozinha. Baixe o novo instalador em **Actions** e instale por cima
   (seus presets em `Documentos\ATDT Synth Machine` sao mantidos).

Para compilar de novo sem enviar nada: **Actions > Build Windows (instalador) > Run workflow**.

## Custos

- Repositorio **privado** no plano gratis: 2.000 minutos/mes, e minutos de Windows contam em dobro,
  o que da cerca de **40 compilacoes por mes**. Mais que suficiente.
- Repositorio **publico**: ilimitado (mas o codigo fica visivel para todos).

## Problemas

**A pasta `.github` nao apareceu na lista de upload**
Crie o arquivo na mao: no repositorio, **Add file > Create new file**, digite o nome
`.github/workflows/build-windows.yml` (as barras criam as pastas), cole o conteudo do arquivo
`build-windows.yml` do zip e clique em **Commit changes**.

**A aba Actions nao mostra nada**
Confira se o arquivo `.github/workflows/build-windows.yml` esta no repositorio. Depois va em
**Actions > Build Windows (instalador) > Run workflow**.

**Ficou com X vermelho**
Clique na execucao > clique no passo com X vermelho > copie as ultimas linhas do texto e me envie.

**O Windows apagou/bloqueou o instalador**
Alguns antivirus desconfiam de instaladores novos sem assinatura digital. Libere o arquivo no antivirus.
(Assinatura digital de codigo e um certificado pago: da para adicionar no futuro.)
