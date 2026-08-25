; Instalador de PNGTuber Desktop.
; Se compila desde la CI con:
;   ISCC.exe /DSourceDir="...\dist\PngtuberDesktop" installer\pngtuber.iss
; SourceDir es la carpeta que ha dejado windeployqt, con el .exe y las DLL de Qt.

#ifndef SourceDir
  #define SourceDir "..\dist\PngtuberDesktop"
#endif

#define MyAppName "PNGTuber Desktop"
#define MyAppVersion "1.0.0"
#define MyAppExeName "pngtuber.exe"

[Setup]
AppId={{A7C41E62-3F8B-4D19-9A6E-5B2C8D0F4E77}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppVerName={#MyAppName} {#MyAppVersion}
DefaultDirName={autopf}\{#MyAppName}
DefaultGroupName={#MyAppName}
UninstallDisplayIcon={app}\{#MyAppExeName}
OutputBaseFilename=PngtuberDesktop-Setup
Compression=lzma2/max
SolidCompression=yes
WizardStyle=modern
; Sin privilegios de administrador: instala en la carpeta del usuario y así
; Windows no pide elevación ni asusta con el aviso de control de cuentas.
PrivilegesRequired=lowest
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible

[Languages]
Name: "spanish"; MessagesFile: "compiler:Languages\Spanish.isl"
Name: "english"; MessagesFile: "compiler:Default.isl"

[Tasks]
Name: "desktopicon"; Description: "Crear un acceso directo en el escritorio"; GroupDescription: "Accesos directos:"
Name: "startupicon"; Description: "Arrancar automáticamente al iniciar Windows"; GroupDescription: "Inicio:"; Flags: unchecked

[Files]
Source: "{#SourceDir}\*"; DestDir: "{app}"; Flags: recursesubdirs createallsubdirs ignoreversion

[Icons]
Name: "{group}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"
Name: "{group}\Desinstalar {#MyAppName}"; Filename: "{uninstallexe}"
Name: "{autodesktop}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"; Tasks: desktopicon
Name: "{userstartup}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"; Tasks: startupicon

[Run]
Filename: "{app}\{#MyAppExeName}"; Description: "Abrir {#MyAppName} ahora"; Flags: nowait postinstall skipifsilent
