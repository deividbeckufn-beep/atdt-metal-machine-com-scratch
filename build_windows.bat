@echo off
REM ==========================================================================
REM  ATDT METAL MACHINE - Build automatico para Windows x64
REM  Basta dar dois cliques neste arquivo (ou rodar no "Prompt de Comando").
REM ==========================================================================
setlocal
cd /d "%~dp0"

echo.
echo === ATDT METAL MACHINE - compilando (Windows x64) ===
echo.

where cmake >nul 2>nul
if errorlevel 1 (
    echo [ERRO] CMake nao encontrado. Veja o BUILD_WINDOWS.md, passo 2.
    pause
    exit /b 1
)

where git >nul 2>nul
if errorlevel 1 (
    echo [ERRO] Git nao encontrado. Veja o BUILD_WINDOWS.md, passo 3.
    pause
    exit /b 1
)

echo [1/2] Configurando o projeto (a primeira vez baixa o JUCE, pode demorar)...
cmake -B build -G "Visual Studio 17 2022" -A x64
if errorlevel 1 (
    echo [ERRO] Falha na configuracao. Veja "Solucao de problemas" no BUILD_WINDOWS.md.
    pause
    exit /b 1
)

echo [2/2] Compilando em modo Release...
cmake --build build --config Release --target MetalMachine_VST3 MetalMachine_Standalone
if errorlevel 1 (
    echo [ERRO] Falha na compilacao. Copie a mensagem de erro e me envie.
    pause
    exit /b 1
)

echo.
echo === PRONTO! ===
echo VST3:       build\MetalMachine_artefacts\Release\VST3\ATDT METAL MACHINE.vst3
echo Standalone: build\MetalMachine_artefacts\Release\Standalone\ATDT METAL MACHINE.exe
echo.
echo Para instalar no REAPER, rode "install_vst3.bat" como ADMINISTRADOR.
pause
