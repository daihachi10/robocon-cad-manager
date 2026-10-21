#define AppName "RoboconCadManager"
#define AppVersion GetEnv("APP_VERSION")
#define AppExe "RoboconCadManager.exe"
#define AppDisplayName "高専ロボコン CAD 管理 (RoboconCadManager)"
#ifndef AppBinary
  #define AppBinary "..\build-release\RoboconCadManager.exe"
#endif
#ifndef GitSetupBinary
  #define GitSetupBinary "..\packaging\dependencies\Git-for-Windows.exe"
#endif

[Setup]
AppId={{F6AFD6D3-A0E6-4A6D-AD1A-4CFE3E5A6F12}
AppName={#AppDisplayName}
AppVersion={#AppVersion}
AppPublisher=高専ロボコン設計班
DefaultDirName={localappdata}\Programs\RoboconCadManager
DefaultGroupName={#AppName}
OutputDir=..\dist
OutputBaseFilename=RoboconCadManagerSetup
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
CloseApplications=force

[Languages]
Name: "japanese"; MessagesFile: "compiler:Languages\Japanese.isl"

[Files]
Source: "{#AppBinary}"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\packaging\git-version.json"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#GitSetupBinary}"; DestName: "Git-for-Windows.exe"; Flags: dontcopy

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
var NeedsGit: Boolean;

function GitLfsWorks(): Boolean;
var Code: Integer;
begin
  Result := False;
  if not FileExists(ExpandConstant('{app}\tools\git\bin\git.exe')) then Exit;
  if not FileExists(ExpandConstant('{app}\tools\git\cmd\git-lfs.exe')) then Exit;
  Result := Exec(ExpandConstant('{app}\tools\git\bin\git.exe'), 'lfs version',
    ExpandConstant('{app}'), SW_HIDE, ewWaitUntilTerminated, Code);
  if Result then Result := Code = 0;
end;

procedure CurStepChanged(CurStep: TSetupStep);
var ResultCode: Integer;
begin
  if CurStep = ssPostInstall then begin
    if NeedsGit then begin
      if not Exec(ExpandConstant('{tmp}\Git-for-Windows.exe'), '/VERYSILENT /NORESTART /SUPPRESSMSGBOXES /DIR="' + ExpandConstant('{app}\tools\git') + '" /COMPONENTS="icons,ext,gitlfs,assoc,assoc_sh"', '', SW_HIDE, ewWaitUntilTerminated, ResultCode) then
        RaiseException('同梱Git・Git LFSを起動できませんでした。');
      if ResultCode <> 0 then
        RaiseException('同梱Git・Git LFSの導入に失敗しました。（終了コード ' + IntToStr(ResultCode) + '）');
    end;
    if not GitLfsWorks() then
      RaiseException('Git LFSの動作確認に失敗しました。インストーラーをもう一度実行してください。');
  end;
end;

function PrepareToInstall(var NeedsRestart: Boolean): String;
begin
  Result := '';
  NeedsGit := not GitLfsWorks();
  if NeedsGit then ExtractTemporaryFile('Git-for-Windows.exe');
end;
