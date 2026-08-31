; Z Video Editor Windows installer.
; Build from the repository root after staging the distributable in release_pkg:
;   iscc /DMyAppVersion=1.1.2 installer\Z.iss

#ifndef MyAppVersion
  #define MyAppVersion "1.1.2"
#endif

#define MyAppName "Z Video Editor"
#define MyAppPublisher "C0DE-Z"
#define MyAppURL "https://github.com/C0DE-Z/Z"
#define MyAppExeName "z.exe"

[Setup]
AppId={{B9A320C5-3C2C-446F-A079-A02FAAD65B7E}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppPublisher={#MyAppPublisher}
AppPublisherURL={#MyAppURL}
AppSupportURL={#MyAppURL}/issues
AppUpdatesURL={#MyAppURL}/releases/latest
DefaultDirName={autopf}\Z Video Editor
DefaultGroupName={#MyAppName}
DisableProgramGroupPage=yes
LicenseFile=..\README.md
OutputDir=..
OutputBaseFilename=Z-VideoEditor-Setup-Windows-x64
Compression=lzma2/ultra64
SolidCompression=yes
WizardStyle=modern
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
UninstallDisplayIcon={app}\{#MyAppExeName}

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

[Files]
Source: "..\release_pkg\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs

[Icons]
; {autoprograms} registers Z in the Start menu, making it discoverable from Apps search.
Name: "{autoprograms}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"
; Always create the requested desktop shortcut instead of leaving it behind an unchecked task.
Name: "{autodesktop}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"

[Run]
Filename: "{app}\{#MyAppExeName}"; Description: "Launch {#MyAppName}"; Flags: nowait postinstall skipifsilent
