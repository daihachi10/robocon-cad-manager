#include "app.h"

namespace rc {
void App::ShowMergeRecovery(){
 const auto* repo=Current();if(!repo)return;
 if(!BeginOperation(L"中断した取り込みを確認",L"取り込みの状態と競合ファイルを確認しています。",L"git diff --diff-filter=U / git diff --cached"))return;
 Background([this,r=*repo]{
  const auto state=git_.OperationState(r);
  auto files=git_.Run(r.folder,{L"-c",L"core.quotepath=false",L"diff",L"--cached",L"--name-status",L"--no-ext-diff",L"--"});
  auto* task=new UiTask{[this,r,state,files]{
   if(!state.error.empty()){FailOperation(L"取り込みの状態を確認できません。",files.command,state.error);return;}
   if(files.exitCode){FailOperation(L"取り込み内容を確認できません。",files.command,files.output+files.error);return;}
   if(!state.conflicts.empty()){
    std::wstring details=L"以下のファイルで変更が重なっています。\r\n";
    for(const auto& path:state.conflicts)details+=L"・"+path+L"\r\n";
    details+=L"\r\n1. この画面を閉じて、表示を「すべてのファイル」にします。\r\n2. 相手と内容を確認し、残すファイルを作業フォルダーへ保存してください。CADでは両者のファイルを別の場所に保管してから選んでください。テキストは競合箇所を修正します。\r\n3. 対象ファイルを右クリック →「このファイルの競合を解決済みにする」を選びます。削除する場合も、その項目で同じ操作をします。\r\n4. すべて解決後に「その他」→「中断した取り込みを確認・完了」を実行します。\r\n\r\nファイルを勝手に選択・上書きしたり、履歴を書き換えたりはしません。";
    FailOperation(L"競合の解決が必要です。下の手順を確認してください。",files.command,details);RefreshLocal();return;
   }
   if(!state.merging){CompleteOperation();MessageBoxW(window_,L"途中で止まっている取り込みはありません。「更新」または「保存して送信」を実行できます。",L"取り込みの確認",MB_OK|MB_ICONINFORMATION);return;}
   CompleteOperation();
   const auto question=L"以前の取り込みが、変更記録を確定する直前で止まっています。\n\n記録される対象（A:追加 / M:変更 / D:削除 など）:\n"+files.output+L"\n現在の取り込み対象をまとめて確定します。チェックボックスの選択は使いません。解決済みとして登録した後に編集した内容や、無関係の未保存ファイルは追加しません。\n\n他のGitツールやエディターで取り込み処理を実行していないことを確認してください。\n取り込みを完了しますか？（送信は別操作です）";
   if(MessageBoxW(window_,question.c_str(),L"中断した取り込みを完了",MB_YESNO|MB_ICONQUESTION|MB_DEFBUTTON2)!=IDYES)return;
   if(!BeginOperation(L"取り込みを完了",L"変更記録を確定しています。",L"git commit --no-edit"))return;
   Background([this,r,state]{
    auto result=git_.FinishMerge(r,state);
    auto* done=new UiTask{[this,result]{
     if(result.exitCode){FailOperation(L"取り込みを完了できませんでした。",result.command,result.output+L"\r\n"+result.error);RefreshLocal();return;}
     CompleteOperation();RefreshLocal();MessageBoxW(window_,L"中断していた取り込みが完了しました。次に「保存して送信」でGitHubへ送信してください。",L"取り込みが完了しました",MB_OK|MB_ICONINFORMATION);
    }};
    if(!PostMessageW(window_,WM_APP_DONE,0,reinterpret_cast<LPARAM>(done)))delete done;
   });
  }};
  if(!PostMessageW(window_,WM_APP_DONE,0,reinterpret_cast<LPARAM>(task)))delete task;
 });
}

void App::ResolveSelectedConflict(){
 const auto* repo=Current();if(!repo)return;
 const int selected=ListView_GetNextItem(list_,-1,LVNI_SELECTED);
 if(selected<0||selected>=static_cast<int>(listPaths_.size())||browserItems_[selected].directory)return;
 const auto path=listPaths_[selected];
 const auto question=L"対象: "+path+L"\n\n相手と内容を確認し、残す内容を保存しましたか？\n現在のファイル内容を、競合を解決した結果として登録します。ファイルを削除済みの場合は削除として登録します。\n\nSolidWorksやエディターで保存・終了してから実行してください。この操作だけでは取り込みの確定・送信は行いません。";
 if(MessageBoxW(window_,question.c_str(),L"このファイルの競合を解決済みにする",MB_YESNO|MB_ICONWARNING|MB_DEFBUTTON2)!=IDYES)return;
 if(!BeginOperation(L"競合の解決",L"保存した内容を解決結果として登録しています。",L"git add -- "+path))return;
 Background([this,r=*repo,path]{
  const auto result=git_.MarkResolved(r,path);
  auto* done=new UiTask{[this,result]{
   if(result.exitCode)FailOperation(L"解決済みとして登録できませんでした。",result.command,result.output+L"\r\n"+result.error);
   else{CompleteOperation();MessageBoxW(window_,L"解決済みとして登録しました。全ファイルの解決後に「その他」→「中断した取り込みを確認・完了」を実行してください。",L"競合の解決",MB_OK|MB_ICONINFORMATION);}
   RefreshLocal();
  }};
  if(!PostMessageW(window_,WM_APP_DONE,0,reinterpret_cast<LPARAM>(done)))delete done;
 });
}
}
