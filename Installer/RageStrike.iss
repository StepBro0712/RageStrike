; Установщик RageStrike
#define AppName "RageStrike"
#define AppVersion "1.0"
#define AppExe "RageStrike.exe"

[Setup]
AppId={{8F3A1C20-47B9-4E6D-9A5B-7C1E2D3F4A5B}
AppName={#AppName}
AppVersion={#AppVersion}
AppPublisher=RageStrike
DefaultDirName={localappdata}\Programs\{#AppName}
DefaultGroupName={#AppName}
DisableProgramGroupPage=yes
; ставим в профиль пользователя, права администратора не нужны
PrivilegesRequired=lowest
OutputDir=C:\Dev\RageStrike
OutputBaseFilename=RageStrike_Setup
Compression=lzma2/max
SolidCompression=yes
WizardStyle=modern
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
DiskSpanning=no
UninstallDisplayName={#AppName}
UninstallDisplayIcon={app}\{#AppExe}

[Languages]
Name: "ru"; MessagesFile: "compiler:Languages\Russian.isl"

[Tasks]
Name: "desktopicon"; Description: "Создать ярлык на рабочем столе"; GroupDescription: "Дополнительно:"

[Files]
Source: "C:\Dev\RageStrike\Packaged\Windows\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "C:\Program Files\Epic Games\UE_5.8\Engine\Extras\Redist\en-us\vc_redist.x64.exe"; DestDir: "{tmp}"; Flags: deleteafterinstall
Source: "C:\Dev\RageStrike\Installer\ЧИТАТЬ МЕНЯ.txt"; DestDir: "{app}"; Flags: ignoreversion

[Icons]
Name: "{group}\{#AppName}"; Filename: "{app}\{#AppExe}"
Name: "{group}\Управление и правила"; Filename: "{app}\ЧИТАТЬ МЕНЯ.txt"
Name: "{group}\Удалить {#AppName}"; Filename: "{uninstallexe}"
Name: "{autodesktop}\{#AppName}"; Filename: "{app}\{#AppExe}"; Tasks: desktopicon

[Run]
Filename: "{tmp}\vc_redist.x64.exe"; Parameters: "/install /quiet /norestart"; StatusMsg: "Установка системных библиотек Visual C++..."; Flags: waituntilterminated
Filename: "{app}\{#AppExe}"; Description: "Запустить {#AppName}"; Flags: nowait postinstall skipifsilent

[UninstallDelete]
Type: filesandordirs; Name: "{app}\RageStrike\Saved"