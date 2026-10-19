#define AppName "RoboconCadManager"
#define AppVersion GetEnv("APP_VERSION")
#define AppExe "RoboconCadManager.exe"
#define AppDisplayName "高専ロボコン CAD 管理 (RoboconCadManager)"
#ifndef AppBinary
  #define AppBinary "..\build-release\RoboconCadManager.exe"
#endif

[Setup]
AppId={{F6AFD6D3-A0E6-4A6D-AD1A-4CFE3E5A6F12}
AppName={#AppDisplayName}
AppVersion={#AppVersion}
AppPublisher=高専ロボコン設計班
DefaultDirName={localappdata}\Programs\RoboconCadManager
DefaultGroupName={#AppName}
OutputDir=..\dist
OutputBaseFilename=RoboconCadManagerSetup-{#AppVersion}
PrivilegesRequired=lowest
Uninstallable=yes
Compression=lzma2/ultra64
SolidCompression=yes
ArchitecturesInstallIn64BitMode=x64compatible
WizardStyle=modern
DisableProgramGroupPage=yes
AllowNoIcons=no
SetupIconFile=..\assets\app.ico
UninstallDisplayIcon={app}\RoboconCadManager.exe,0
CloseApplications=yes

[Languages]
Name: "japanese"; MessagesFile: "compiler:Languages\Japanese.isl"

[Files]
Source: "{#AppBinary}"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\packaging\git-version.json"; DestDir: "{app}"; Flags: ignoreversion

[Icons]
Name: "{userprograms}\{#AppDisplayName}"; Filename: "{app}\{#AppExe}"; WorkingDir: "{app}"; IconFilename: "{app}\{#AppExe}"; IconIndex: 0; AppUserModelID: "RoboconCadManager.Desktop"; Comment: "ロボコン設計班のCADファイルを管理"
Name: "{userdesktop}\{#AppDisplayName}"; Filename: "{app}\{#AppExe}"; WorkingDir: "{app}"; IconFilename: "{app}\{#AppExe}"; IconIndex: 0; AppUserModelID: "RoboconCadManager.Desktop"; Tasks: desktopicon

[InstallDelete]
Type: files; Name: "{userprograms}\RoboconCadManager.lnk"
Type: files; Name: "{userdesktop}\RoboconCadManager.lnk"

[Tasks]
Name: "desktopicon"; Description: "デスクトップにショートカットを作成"; Flags: unchecked

[Run]
Filename: "{app}\{#AppExe}"; Description: "{#AppName}を起動"; Flags: nowait postinstall skipifsilent

[UninstallDelete]
Type: filesandordirs; Name: "{app}"

[Code]
var GitSetupPath: String;
const GitUrl = 'https://github.com/git-for-windows/git/releases/download/v2.56.0.windows.2/Git-2.56.0.2-64-bit.exe';
const GitSha256 = '52188f917b378f00c70ec136bcf090005f30d44fbc4eba0bce759cc6592d60f6';

function DownloadGit(): Boolean;
var
  GitSetup: String;
begin
  GitSetup := ExpandConstant('{tmp}\Git-for-Windows.exe');
  GitSetupPath := GitSetup;
  DownloadTemporaryFile(GitUrl, 'Git-for-Windows.exe', GitSha256, nil);
  Result := FileExists(GitSetup);
  if not Result then
    MsgBox('Git for Windowsを取得できませんでした。通信を確認してから再実行してください。', mbError, MB_OK);
end;

procedure CurStepChanged(CurStep: TSetupStep);
var ResultCode: Integer;
begin
  if (CurStep = ssPostInstall) and (GitSetupPath <> '') then begin
    if not Exec(GitSetupPath, '/VERYSILENT /NORESTART /SUPPRESSMSGBOXES /DIR="' + ExpandConstant('{app}\tools\git') + '" /COMPONENTS="icons,ext,gitlfs,assoc,assoc_sh"', '', SW_HIDE, ewWaitUntilTerminated, ResultCode) or (ResultCode <> 0) then
      MsgBox('Git for Windowsをアプリに追加できませんでした。設定からGitの場所を確認してください。', mbError, MB_OK);
  end;
end;

function PrepareToInstall(var NeedsRestart: Boolean): String;
begin
  Result := '';
  if FileExists(ExpandConstant('{app}\tools\git\bin\git.exe')) and
     FileExists(ExpandConstant('{app}\tools\git\cmd\git-lfs.exe')) then Exit;
  if not DownloadGit() then Result := 'Git for Windowsの取得に失敗しました。';
end;
