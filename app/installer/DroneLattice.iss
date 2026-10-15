; Drone Lattice installer (Inno Setup 6). Built by app\build.ps1:
;   ISCC /DBuildDir=<artefacts\Release> /DOutDir=<dist> DroneLattice.iss
; Per-user by default (no admin): app in %LOCALAPPDATA%\Programs, VST3 in the
; per-user VST3 folder (%LOCALAPPDATA%\Programs\Common\VST3, VST 3.7+).
; Choosing "all users" in the dialog installs to Program Files / Common Files.

#define AppName "Drone Lattice"
#define AppVer "0.2.0"
#ifndef BuildDir
  #define BuildDir "..\..\build\app\DroneLattice_artefacts\Release"
#endif
#ifndef IconFile
  #define IconFile "..\..\build\app\DroneLattice_artefacts\JuceLibraryCode\icon.ico"
#endif
#ifndef OutDir
  #define OutDir "..\..\dist"
#endif

[Setup]
AppId={{6F3C2A51-9D4E-4B7A-8E21-D20E1A77C0DE}
AppName={#AppName}
AppVersion={#AppVer}
AppPublisher=drone-lattice
DefaultDirName={autopf}\{#AppName}
DefaultGroupName={#AppName}
DisableProgramGroupPage=yes
OutputDir={#OutDir}
OutputBaseFilename=DroneLattice-Setup-{#AppVer}
SetupIconFile={#IconFile}
UninstallDisplayIcon={app}\{#AppName}.exe
Compression=lzma2/max
SolidCompression=yes
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
PrivilegesRequired=lowest
PrivilegesRequiredOverridesAllowed=dialog
WizardStyle=modern

[Languages]
Name: "ru"; MessagesFile: "compiler:Languages\Russian.isl"
Name: "en"; MessagesFile: "compiler:Default.isl"

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"
Name: "vst3"; Description: "VST3 plugin (for DAWs)"; GroupDescription: "Plugins:"

[Files]
Source: "{#BuildDir}\Standalone\{#AppName}.exe"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#BuildDir}\VST3\{#AppName}.vst3\*"; DestDir: "{autocf}\VST3\{#AppName}.vst3"; Flags: ignoreversion recursesubdirs createallsubdirs; Tasks: vst3

[Icons]
Name: "{autoprograms}\{#AppName}"; Filename: "{app}\{#AppName}.exe"
Name: "{autodesktop}\{#AppName}"; Filename: "{app}\{#AppName}.exe"; Tasks: desktopicon

[Run]
Filename: "{app}\{#AppName}.exe"; Description: "{cm:LaunchProgram,{#AppName}}"; Flags: nowait postinstall skipifsilent
