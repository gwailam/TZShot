; TZshot Inno Setup script — Windows installer for the pure-screenshot build.
; Packages the windeployqt-staged folder (dist\app) into a single Setup.exe.
; Build with:  "C:\Program Files (x86)\Inno Setup 6\ISCC.exe" packaging\tzshot.iss

#define MyAppName "TZshot"
; 版本号需与 CMakeLists.txt 的 project(VERSION) 保持一致(Inno 无法读取 CMake)。
; 也可在构建脚本中以 ISCC /DMyAppVersion=x.y.z 传入覆盖此默认值。
#ifndef MyAppVersion
#define MyAppVersion "1.1.2"
#endif
#define MyAppPublisher "TZshot"
#define MyAppExeName "TZshot.exe"
#define StageDir "..\dist\app"

[Setup]
AppId={{B6F4B3C2-9E2A-4E1D-9C3A-TZSHOT0001}}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppPublisher={#MyAppPublisher}
DefaultDirName={autopf}\{#MyAppName}
DefaultGroupName={#MyAppName}
DisableProgramGroupPage=yes
OutputDir=..\dist
OutputBaseFilename=TZshot-{#MyAppVersion}-setup
SetupIconFile=..\resource\img\app_icon.ico
Compression=lzma2
SolidCompression=yes
WizardStyle=modern
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
PrivilegesRequired=admin
UninstallDisplayIcon={app}\{#MyAppExeName}

[Languages]
; 简体中文为 Inno 非官方语言包,各机器/CI 的 Inno 安装未必自带,
; 故随仓库附带 packaging\ChineseSimplified.isl 并以相对路径引用,保证可复现。
Name: "chinesesimplified"; MessagesFile: "ChineseSimplified.isl"
Name: "english"; MessagesFile: "compiler:Default.isl"

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"; Flags: unchecked

[Files]
Source: "{#StageDir}\*"; DestDir: "{app}"; Flags: recursesubdirs createallsubdirs ignoreversion

[Icons]
Name: "{group}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"
Name: "{group}\{cm:UninstallProgram,{#MyAppName}}"; Filename: "{uninstallexe}"
Name: "{autodesktop}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"; Tasks: desktopicon

[Run]
Filename: "{app}\{#MyAppExeName}"; Description: "{cm:LaunchProgram,{#MyAppName}}"; Flags: nowait postinstall skipifsilent
