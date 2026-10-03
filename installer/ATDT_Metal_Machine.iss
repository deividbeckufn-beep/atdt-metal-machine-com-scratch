; =============================================================================
;  Instalador Windows do ATDT SYNTH MACHINE (Inno Setup 6)
;  Instala:
;    - VST3  -> C:\Program Files\Common Files\VST3\ATDT SYNTH MACHINE.vst3  (onde o REAPER procura)
;    - App standalone (opcional) -> C:\Program Files\ATDT Synth Machine\
;  Os presets do usuario (Documentos\ATDT Synth Machine) nunca sao apagados.
;  Gerado automaticamente pelo GitHub Actions (.github/workflows/build-windows.yml)
; =============================================================================

#define AppName      "ATDT SYNTH MACHINE"
#define AppVersion   "0.17.0"
#define AppPublisher "ATDT Audio"
#define BuildDir     "..\build\MetalMachine_artefacts\Release"

[Setup]
AppId={{A7D71E2C-5B3F-4C8E-9A61-2F4D8B6C9E10}
AppName={#AppName}
AppVersion={#AppVersion}
AppVerName={#AppName} {#AppVersion}
AppPublisher={#AppPublisher}
DefaultDirName={autopf}\ATDT Synth Machine
DefaultGroupName=ATDT Synth Machine
DisableProgramGroupPage=yes
OutputDir=Output
OutputBaseFilename=ATDT-Metal-Machine-{#AppVersion}-Windows-x64-Setup
ArchitecturesAllowed=x64
ArchitecturesInstallIn64BitMode=x64
PrivilegesRequired=admin
Compression=lzma2
SolidCompression=yes
WizardStyle=modern
CloseApplications=yes
UninstallDisplayName={#AppName}

[Languages]
Name: "brazilianportuguese"; MessagesFile: "compiler:Languages\BrazilianPortuguese.isl"
Name: "english"; MessagesFile: "compiler:Default.isl"

[Types]
Name: "full"; Description: "Completa (VST3 + aplicativo standalone)"
Name: "vst3only"; Description: "Somente o plugin VST3"
Name: "custom"; Description: "Personalizada"; Flags: iscustom

[Components]
Name: "vst3"; Description: "Plugin VST3 (para o REAPER)"; Types: full vst3only custom; Flags: fixed
Name: "standalone"; Description: "Aplicativo standalone (tocar sem DAW)"; Types: full

[Tasks]
Name: "desktopicon"; Description: "Criar atalho do app standalone na area de trabalho"; Components: standalone; Flags: unchecked

[InstallDelete]
; remove uma versao anterior do plugin antes de copiar a nova
Type: filesandordirs; Name: "{commoncf64}\VST3\ATDT SYNTH MACHINE.vst3"
Type: filesandordirs; Name: "{commoncf64}\VST3\ATDT METAL MACHINE.vst3"

[Files]
Source: "{#BuildDir}\VST3\ATDT SYNTH MACHINE.vst3\*"; DestDir: "{commoncf64}\VST3\ATDT SYNTH MACHINE.vst3"; Components: vst3; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "{#BuildDir}\Standalone\ATDT SYNTH MACHINE.exe"; DestDir: "{app}"; Components: standalone; Flags: ignoreversion
Source: "..\README.md"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\LEIA-ME-INSTALACAO.txt"; DestDir: "{app}"; Flags: ignoreversion isreadme

[Icons]
Name: "{autoprograms}\ATDT SYNTH MACHINE"; Filename: "{app}\ATDT SYNTH MACHINE.exe"; Components: standalone
Name: "{autodesktop}\ATDT SYNTH MACHINE"; Filename: "{app}\ATDT SYNTH MACHINE.exe"; Components: standalone; Tasks: desktopicon

[Run]
Filename: "{app}\ATDT SYNTH MACHINE.exe"; Description: "Abrir o ATDT SYNTH MACHINE agora"; Components: standalone; Flags: nowait postinstall skipifsilent unchecked

[UninstallDelete]
Type: filesandordirs; Name: "{commoncf64}\VST3\ATDT SYNTH MACHINE.vst3"
