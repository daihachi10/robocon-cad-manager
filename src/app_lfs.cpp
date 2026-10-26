#include "app.h"

namespace rc {
void App::DownloadLfsFiles(const std::vector<std::wstring>& paths,bool openAfter){
 auto* repo=Current();if(!repo)return;
 const auto target=paths.empty()?std::wstring(L"すべての大容量ファイル"):paths.front();
 const auto command=paths.empty()?L"git lfs pull --include= --exclude=":L"git lfs pull --include=<選択ファイル> --exclude=";
 const auto title=paths.empty()?L"大容量ファイルを取得":L"大容量ファイルを取得して開く";
 if(!BeginOperation(title,L"大容量ファイルの本体を取得しています。",command))return;
 SetStatus(L"大容量ファイルの本体を取得しています。");
 Background([this,r=*repo,paths,openAfter,target]{
  auto progress=[this](int complete,int total){PostOperationStep(L"大容量ファイルを取得中（"+std::to_wstring(complete)+L"/"+std::to_wstring(total)+L"ファイル）",L"git lfs pull",complete,total);};
  auto steps=[this](const std::wstring& stage,const std::wstring& command,int complete,int total,const ProcessResult* result){PostOperationStep(stage,command,complete,total,result?L"> "+result->command+L"\r\n終了コード: "+std::to_wstring(result->exitCode)+L"\r\n"+result->output:L"");};
  const auto result=git_.LfsPull(r,paths,progress,steps);
  auto* done=new UiTask{[this,result,openAfter,target]{
   if(result.exitCode||!result.error.empty()){
    const auto details=L"Git LFSの本体を取得できませんでした。\r\n\r\n"+result.error+L"\r\n\r\n"+result.output;
    SetStatus(L"大容量ファイルを取得できませんでした。");FailOperation(L"大容量ファイルを取得できませんでした。",result.command,details);RefreshLocal();return;
   }
   CompleteOperation();SetStatus(L"大容量ファイルの本体を取得しました。");RefreshLocal();
   if(openAfter)LockPath(target,true);
   else MessageBoxW(window_,L"大容量ファイルの本体を取得しました。",L"取得が完了しました",MB_OK|MB_ICONINFORMATION);
  }};
  if(!PostMessageW(window_,WM_APP_DONE,0,reinterpret_cast<LPARAM>(done)))delete done;
 });
}
}
