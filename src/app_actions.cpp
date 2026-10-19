#include "app.h"
#include "ui.h"

namespace rc {
namespace {
std::wstring FailureMessage(const std::wstring& operation,const ProcessResult& result) {
 if(!result.error.empty())return operation+L"に失敗しました。\n\n"+result.error;
 if(result.output.find(L"Author identity unknown")!=std::wstring::npos||result.output.find(L"tell me who you are")!=std::wstring::npos||result.output.find(L"unable to auto-detect email address")!=std::wstring::npos)return L"変更記録の氏名・メールアドレスをGitが確認できません。「その他」→「設定」で氏名とメールアドレスを入力して保存してください。";
 if(result.output.find(L"nothing to commit")!=std::wstring::npos)return L"保存する変更がありません。";
 if(result.output.find(L"Authentication failed")!=std::wstring::npos||result.output.find(L"could not read Username")!=std::wstring::npos)return L"GitHubに認証できませんでした。Git Credential Managerでサインインしてください。";
 if(result.output.find(L"not a git repository")!=std::wstring::npos)return L"作業フォルダがGitリポジトリではありません。設定を確認してください。";
 if(result.output.find(L"already exists")!=std::wstring::npos)return L"複製先のフォルダが既に存在します。空の保存先を指定してください。";
 if(result.output.find(L"rejected")!=std::wstring::npos)return L"GitHub側に新しい変更があります。先に「最新状態を取得」を実行してください。";
 return operation+L"に失敗しました。（終了コード "+std::to_wstring(result.exitCode)+L"）";
}

}

void App::ShowSetup(){
 SetupFields f;
 if(auto*r=Current()){f.name=r->name;f.remote=r->remote;f.folder=r->folder;f.userName=r->userName;f.userEmail=r->userEmail;}
 if(!ShowSetupDialog(window_,f))return;
 f.userName=Trim(f.userName);f.userEmail=Trim(f.userEmail);
 if(f.name.empty()||f.remote.empty()||f.folder.empty()){MessageBoxW(window_,L"表示名、リポジトリURL、作業フォルダは必須です。",L"設定を保存できません",MB_ICONWARNING);return;}
 if(f.userName.empty()||f.userEmail.empty()){MessageBoxW(window_,L"変更記録に使う氏名とメールアドレスを入力してください。",L"設定を保存できません",MB_ICONWARNING);return;}
 if(f.remote.find(L"://")==std::wstring::npos&&f.remote.find(L"/")!=std::wstring::npos)f.remote=L"https://github.com/"+f.remote+L".git";
 if(f.remote.starts_with(L"http://github.com/"))f.remote.replace(0,5,L"https");
 Repository repository{f.name,f.remote,f.folder,f.userName,f.userEmail};
 config_.Upsert(repository);
 std::wstring error;
 if(!config_.Save(error)){MessageBoxW(window_,error.c_str(),L"設定を保存できません",MB_ICONERROR);return;}
 const auto& repositories=config_.Repositories();
 auto saved=std::find_if(repositories.begin(),repositories.end(),[&](const Repository& value){return value.name==repository.name;});
 selected_=saved==repositories.end()?0:static_cast<int>(std::distance(repositories.begin(),saved));
 StartLocalWatcher();
 SetStatus(L"設定を保存しました。ローカル情報を更新しています。");
 if(std::filesystem::exists(std::filesystem::path(repository.folder)/L".git")){
  Background([this,repository,enableLfsSetup=f.enableLfsSetup]{
   std::wstring setupError;
   ProcessResult result;
   {std::lock_guard guard(lockWorkMutex_);
    result=git_.Configure(repository);
    if(!result.exitCode&&enableLfsSetup){result=git_.SetupLfsLocking(repository,setupError);if(result.error.empty()&&!setupError.empty())result.error=setupError;}
   }
   auto* task=new UiTask{[this,result=std::move(result),enableLfsSetup]() mutable {
    if(result.exitCode||!result.error.empty()){
     const auto error=FailureMessage(L"Gitへの設定反映",result);
     MessageBoxW(window_,error.c_str(),L"Gitに設定を反映できません",MB_ICONERROR);
     SetStatus(error);
    }else{
     if(enableLfsSetup){MessageBoxW(window_,L".gitattributesへLFSロック設定を追加しました。変更を選んで送信してください。",L"LFSロック設定",MB_ICONINFORMATION);Refresh();}
     SetStatus(L"設定を保存し、Gitに変更記録の氏名・メールアドレスを反映しました。");
    }
   }};
   if(!PostMessageW(window_,WM_APP_DONE,0,reinterpret_cast<LPARAM>(task)))delete task;
  });
 }else if(f.enableLfsSetup){
  MessageBoxW(window_,L"設定を保存しました。先に「その他」→「複製」で作業フォルダを取得し、その後にLFSロック設定を実行してください。",L"先にリポジトリを複製してください",MB_ICONINFORMATION);
 }
 Refresh();
}

void App::HandleCommand(WORD id){
 constexpr WORD setup=100,refresh=101,clone=102,pull=104,commit=105,open=106,more=107,refreshLocal=108,lfsSetup=110,locks=111,unlock=112,update=113;
 auto* repository=Current();
 if(id==kBrowseBack||id==kBrowseForward||id==kBrowseUp){
  if(!repository)return;
  const bool moved=id==kBrowseBack?folderNavigation_.Back():(id==kBrowseForward?folderNavigation_.Forward():folderNavigation_.Up());
  if(moved)Populate();return;
 }
 if(id==setup){ShowSetup();return;}
 if(id==more){HMENU menu=CreatePopupMenu();AppendMenuW(menu,MF_STRING,setup,L"設定");AppendMenuW(menu,MF_STRING,lfsSetup,L"LFSロック設定");AppendMenuW(menu,MF_STRING,locks,L"ロック状態を更新");AppendMenuW(menu,MF_STRING,unlock,L"選択ファイルのロック解除");AppendMenuW(menu,MF_STRING,update,L"更新を確認");AppendMenuW(menu,MF_SEPARATOR,0,nullptr);AppendMenuW(menu,MF_STRING,clone,L"複製");AppendMenuW(menu,MF_STRING,pull,L"最新状態だけ取得");AppendMenuW(menu,MF_STRING,refreshLocal,L"一覧だけ更新");AppendMenuW(menu,MF_STRING,open,L"ファイルを開く");POINT point;GetCursorPos(&point);TrackPopupMenu(menu,TPM_LEFTALIGN|TPM_TOPALIGN,point.x,point.y,0,window_,nullptr);DestroyMenu(menu);return;}
 if(id==update){CheckForUpdates(true);return;}
 if(!repository){MessageBoxW(window_,L"先にリポジトリを設定してください。",L"操作できません",MB_ICONINFORMATION);return;}
 auto report=[this](const std::wstring& operation,const std::wstring& success,ProcessResult result,bool refreshAfter=true){
  PostMessageW(window_,WM_APP_DONE,0,reinterpret_cast<LPARAM>(new UiTask{[this,operation,success,result=std::move(result),refreshAfter]{
   const bool ok=result.exitCode==0&&result.error.empty();
   const auto message=ok?success:FailureMessage(operation,result);
   SetStatus(message);
   MessageBoxW(window_,message.c_str(),ok?L"操作が完了しました":L"操作に失敗しました",ok?MB_ICONINFORMATION:MB_ICONERROR);
   if(ok&&operation==L"変更の保存と送信")ReleasePendingLocks();
   if(refreshAfter)Refresh();
  }}));
 };
 if(id==refresh){SetStatus(L"GitHubから最新状態を取得しています。");Background([this,r=*repository,report]{report(L"更新",L"GitHubから最新状態を取得して一覧を更新しました。",git_.Pull(r));});return;}
 if(id==refreshLocal){RefreshLocal();return;}
 if(id==clone){SetStatus(L"リポジトリを複製しています。");Background([this,r=*repository,report]{auto result=git_.Clone(r);if(!result.exitCode)result=git_.Configure(r);report(L"リポジトリの複製",L"リポジトリを複製しました。",std::move(result));});return;}
 if(id==pull){SetStatus(L"GitHubから最新状態を取得しています。");Background([this,r=*repository,report]{report(L"最新状態の取得",L"最新状態を取得しました。",git_.Pull(r),false);});return;}
 if(id==lfsSetup){SetStatus(L"Git LFSロック設定を追加しています。");Background([this,r=*repository,report]{std::wstring error;report(L"LFSロック設定",L".gitattributesへLFSロック設定を追加しました。変更を選んで送信してください。",git_.SetupLfsLocking(r,error));});return;}
 if(id==locks){RefreshLocks();return;}
 if(id==unlock){int selected=ListView_GetNextItem(list_,-1,LVNI_SELECTED);if(selected<0||selected>=(int)listPaths_.size()){MessageBoxW(window_,L"解除するファイルを選択してください。",L"ファイルを選択してください",MB_ICONINFORMATION);return;}const auto path=listPaths_[selected];auto found=locks_.find(path);if(found==locks_.end()||!found->second.ours){MessageBoxW(window_,L"自分が保持しているロックだけを解除できます。",L"ロックを解除できません",MB_ICONINFORMATION);return;}if(opened_.contains(path)||git_.IsDirty(*repository,path)){MessageBoxW(window_,L"開いている、または変更済みのファイルは送信後に自動解除します。",L"ロックを解除できません",MB_ICONINFORMATION);return;}auto lock=found->second;Background([this,r=*repository,path,lock]{std::wstring error;bool ok;{std::lock_guard guard(lockWorkMutex_);ok=git_.Unlock(r,lock,error);}PostMessageW(window_,WM_APP_DONE,0,(LPARAM)new UiTask{[this,path,ok,error]{if(ok){locks_.erase(path);Populate();}else MessageBoxW(window_,error.c_str(),L"ロックを解除できません",MB_ICONERROR);}});});return;}
 if(id==commit){std::vector<std::wstring> paths;for(const auto& change:changes_)if(changeChecks_[change.path])paths.push_back(change.path);if(paths.empty()){SetStatus(L"未送信の変更をGitHubへ送信しています。");Background([this,r=*repository,report]{report(L"変更の保存と送信",L"未送信の変更をGitHubへ送信しました。",git_.Push(r));});return;}auto message=Prompt(window_,L"変更を保存して送信",L"変更内容を日本語で入力してください");if(!message)return;SetStatus(L"選択した変更を保存してGitHubへ送信しています。");Background([this,r=*repository,paths=std::move(paths),message=*message,report]{report(L"変更の保存と送信",L"選択した変更を保存してGitHubへ送信しました。",git_.CommitPush(r,paths,message));});return;}
 if(id==open){int selected=ListView_GetNextItem(list_,-1,LVNI_SELECTED);if(selected<0||selected>=(int)listPaths_.size()){MessageBoxW(window_,L"開くファイルを選択してください。",L"ファイルを選択してください",MB_ICONINFORMATION);return;}if(selected<static_cast<int>(browserItems_.size())&&browserItems_[selected].directory){NavigateFolder(listPaths_[selected]);return;}const auto found=localFiles_.find(listPaths_[selected]);if(found==localFiles_.end()){MessageBoxW(window_,L"削除済みのファイルは開けません。",L"ファイルを開けません",MB_ICONINFORMATION);return;}const bool cad=IEndsWith(listPaths_[selected],L".sldprt")||IEndsWith(listPaths_[selected],L".sldasm")||IEndsWith(listPaths_[selected],L".slddrw");if(cad)LockPath(listPaths_[selected],true);else ShellExecuteW(window_,L"open",found->second.c_str(),nullptr,nullptr,SW_SHOWNORMAL);return;}
}

void App::CheckForUpdates(bool manual){
  if (!opened_.empty()) { if (manual) MessageBoxW(window_, L"SolidWorksでファイルを開いているため、更新を延期しました。ファイルを閉じてから再実行してください。", L"更新を延期しました", MB_ICONINFORMATION); return; }
  if(manual) SetStatus(L"更新を確認しています。");
  Background([this,manual]{
    auto result=rc::CheckForUpdate();
    auto* task=new UiTask{[this,result=std::move(result),manual]() mutable {
      if(!result.ok){if(manual)MessageBoxW(window_,result.error.c_str(),L"更新を確認できません",MB_ICONWARNING);return;}
      if(!result.available){if(manual)MessageBoxW(window_,L"すでに最新版です。",L"更新確認",MB_ICONINFORMATION);return;}
      std::wstring prompt=L"新しいバージョン "+result.info.version+L" が利用できます。\n\n"+result.info.notes+L"\n\n今すぐ更新しますか？";
      if(MessageBoxW(window_,prompt.c_str(),L"アプリの更新",MB_YESNO|MB_ICONINFORMATION)!=IDYES)return;
      SetStatus(L"更新ファイルをダウンロードしています。");
      Background([this,info=result.info]{
        std::filesystem::path installer; std::wstring error;
        if(!rc::DownloadAndVerifyUpdate(info,installer,error)){auto* fail=new UiTask{[this,error]{MessageBoxW(window_,error.c_str(),L"更新できません",MB_ICONERROR);SetStatus(error);}};PostMessageW(window_,WM_APP_DONE,0,reinterpret_cast<LPARAM>(fail));return;}
        auto* done=new UiTask{[this,installer]{SHELLEXECUTEINFOW execute{sizeof(execute)};execute.fMask=SEE_MASK_NOCLOSEPROCESS;execute.lpVerb=L"open";execute.lpFile=installer.c_str();execute.nShow=SW_SHOWNORMAL;if(!ShellExecuteExW(&execute)){auto e=LastError();MessageBoxW(window_,e.c_str(),L"更新できません",MB_ICONERROR);return;}SetStatus(L"更新を開始しました。アプリを終了します。");PostMessageW(window_,WM_CLOSE,0,0);}};PostMessageW(window_,WM_APP_DONE,0,reinterpret_cast<LPARAM>(done));
      });
    }};
    if(!PostMessageW(window_,WM_APP_DONE,0,reinterpret_cast<LPARAM>(task)))delete task;
  });
}
}
