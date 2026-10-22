; Inno Setup 6 Installer Script for Amar Bangla Kotha (Windows)
; Builds a one-click .exe installer registering the TSF Text Input Processor

#define MyAppName "Amar Bangla Kotha"
#define MyAppVersion "1.0.0"
#define MyAppPublisher "Amar Bangla Kotha Team"
#define MyAppURL "https://github.com/MehediHasanSumon/amar-bangla-kotha"
#define MyAppExeName "abk_cli.exe"

[Setup]
AppId={{3F5C3B4E-5D8A-4F9A-8417-6B1A4B0C7291}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppPublisher={#MyAppPublisher}
AppPublisherURL={#MyAppURL}
AppSupportURL={#MyAppURL}
AppUpdatesURL={#MyAppURL}
DefaultDirName={autopf}\AmarBanglaKotha
DefaultGroupName={#MyAppName}
AllowNoIcons=yes
OutputDir=..\..\bin
OutputBaseFilename=AmarBanglaKotha-Setup-1.0.0
Compression=lzma2/ultra64
SolidCompression=yes
WizardStyle=modern
ArchitecturesInstallIn64BitMode=x64
PrivilegesRequired=admin

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"; Flags: unchecked

[Files]
; TSF Text Service Provider DLL (Core Engine + Windows TSF)
Source: "..\..\build\windows\abk_tsf.dll"; DestDir: "{app}"; Flags: ignoreversion
; Command Line CLI & Diagnostic Tool
Source: "..\..\bin\abk_cli.exe"; DestDir: "{app}"; Flags: ignoreversion
; Settings GUI & First-Run Tutorial
Source: "..\..\tools\abk_settings.py"; DestDir: "{app}"; Flags: ignoreversion
; Language Corpora & Datasets
Source: "..\..\data\bangla_wordlist.txt"; DestDir: "{app}\data"; Flags: ignoreversion
Source: "..\..\data\bangla_bigrams.tsv"; DestDir: "{app}\data"; Flags: ignoreversion
Source: "..\..\tests\data\golden.tsv"; DestDir: "{app}\data"; Flags: ignoreversion

[Icons]
Name: "{group}\Amar Bangla Kotha Settings"; Filename: "pythonw.exe"; Parameters: """{app}\abk_settings.py"""
Name: "{group}\First-Run Tutorial"; Filename: "pythonw.exe"; Parameters: """{app}\abk_settings.py"" --tutorial"
Name: "{group}\Engine Self-Test Diagnostic"; Filename: "{app}\{#MyAppExeName}"; Parameters: "--selftest"
Name: "{group}\{cm:UninstallProgram,{#MyAppName}}"; Filename: "{uninstallexe}"
Name: "{autodesktop}\Amar Bangla Kotha Settings"; Filename: "pythonw.exe"; Parameters: """{app}\abk_settings.py"""; Tasks: desktopicon

[Run]
; 1. Register TSF COM Component and Category registration
Filename: "regsvr32.exe"; Parameters: "/s ""{app}\abk_tsf.dll"""; StatusMsg: "Registering Bengali Input Method with Windows Text Services Framework..."
; 2. Launch interactive First-Run Tutorial on install finish
Filename: "pythonw.exe"; Parameters: """{app}\abk_settings.py"" --tutorial"; Description: "Launch First-Run Interactive Tutorial (Recommended)"; Flags: postinstall nowait skipifsilent

[UninstallRun]
; Unregister TSF COM Component cleanly on uninstall
Filename: "regsvr32.exe"; Parameters: "/u /s ""{app}\abk_tsf.dll"""; RunOnceId: "UnregisterAbkTsf"
