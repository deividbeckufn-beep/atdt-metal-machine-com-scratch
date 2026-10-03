@echo off
REM ==========================================================================
REM  Copia o VST3 para a pasta padrao do Windows.
REM  Clique com o botao direito -> "Executar como administrador".
REM ==========================================================================
setlocal
cd /d "%~dp0"
set "SRC=build\MetalMachine_artefacts\Release\VST3\ATDT METAL MACHINE.vst3"
set "DST=%CommonProgramFiles%\VST3\ATDT METAL MACHINE.vst3"

if not exist "%SRC%" (
    echo [ERRO] VST3 nao encontrado. Rode primeiro o build_windows.bat.
    pause
    exit /b 1
)

if exist "%DST%" rmdir /s /q "%DST%"
xcopy "%SRC%" "%DST%\" /E /I /Y >nul
if errorlevel 1 (
    echo [ERRO] Nao foi possivel copiar. Voce executou como ADMINISTRADOR?
    pause
    exit /b 1
)

echo Instalado em: %DST%
echo Agora abra o REAPER e faca o "Re-scan" (veja o README).
pause
