#include "git.h"

namespace rc {
namespace {
bool MainBranch(const std::wstring& branch){return branch==L"main"||branch==L"origin/main"||branch==L"refs/heads/main"||branch==L"refs/remotes/origin/main";}
constexpr auto recovery=L"「その他」→「中断した取り込みを確認・完了」を実行してください。";
std::wstring ConflictMessage(const GitOperationState& state){
 std::wstring text=L"同じファイルへの変更が競合しています。自動では上書きしません。\n";
 for(const auto& path:state.conflicts)text+=L"\n・"+path;
 return text+L"\n\n"+recovery;
}
// Retain the failed first push too, so a recovered failure remains diagnosable.
void AppendResult(ProcessResult& all,const ProcessResult& step){
 if(!all.command.empty())all.command+=L"\r\n";all.command+=step.command;
 all.output+=L"> "+step.command+L"\r\n"+step.output+L"\r\n終了コード: "+std::to_wstring(step.exitCode)+L"\r\n";
 if(!step.error.empty())all.output+=step.error+L"\r\n";
 all.exitCode=step.exitCode;all.error=step.error;
}
}

GitOperationState GitClient::OperationState(const Repository& r)const{
 GitOperationState state;
 auto gitDir=Run(r.folder,{L"rev-parse",L"--absolute-git-dir"});
 if(gitDir.exitCode){state.error=L"作業フォルダーのGit情報を読み取れません。設定を確認してください。";return state;}
 // --absolute-git-dir resolves linked worktrees as well as a regular .git directory.
 auto exists=[&](const wchar_t* name){return std::filesystem::exists(std::filesystem::path(Trim(gitDir.output))/name);};
 for(const auto* name:{L"rebase-merge",L"rebase-apply",L"CHERRY_PICK_HEAD",L"REVERT_HEAD",L"sequencer"}){
  if(exists(name)){state.error=L"別のGitツールで始めた履歴操作が途中です。そのツールで完了または中止してから再実行してください。";return state;}
  if(!state.error.empty())return state;
 }
 state.merging=exists(L"MERGE_HEAD");if(!state.error.empty())return state;
 auto conflicts=Run(r.folder,{L"diff",L"--name-only",L"--diff-filter=U",L"-z",L"--"});
 if(conflicts.exitCode){state.error=L"競合ファイルを確認できません。\n"+conflicts.output;return state;}
 size_t start=0;while(start<conflicts.output.size()){auto end=conflicts.output.find(L'\0',start);if(end==std::wstring::npos)break;state.conflicts.push_back(conflicts.output.substr(start,end-start));start=end+1;}
 if(state.merging){
  auto head=Run(r.folder,{L"rev-parse",L"HEAD"}),other=Run(r.folder,{L"rev-parse",L"MERGE_HEAD"});
  auto staged=Run(r.folder,{L"diff",L"--cached",L"--raw",L"--no-abbrev",L"--no-ext-diff",L"-z",L"--"});
  if(head.exitCode||other.exitCode||staged.exitCode)state.error=L"途中の取り込み情報を確認できません。";
  else{state.head=head.output;state.mergeHead=other.output;state.staged=staged.output;}
 }
 return state;
}

ProcessResult GitClient::RequireReady(const Repository& r)const{
 if(!MainBranch(Branch(r)))return{1,{},L"現在の作業場所がmainまたはorigin/mainではありません。"};
 const auto state=OperationState(r);
 if(!state.error.empty())return{1,{},state.error};
 if(!state.conflicts.empty())return{1,{},ConflictMessage(state)};
 if(state.merging)return{1,{},std::wstring(L"以前の取り込みが、変更記録の確定前に止まっています。\n")+recovery};
 return{};
}

ProcessResult GitClient::Pull(const Repository& r,const GitTransferProgress& progress,const GitStepCallback& steps)const{
 auto ready=RequireReady(r);if(ready.exitCode)return ready;NormalizeGithubRemote(r);
 ProcessResult result;int previous=-1,previousTotal=-1;
 const std::vector<std::wstring> fetch{L"fetch",L"--progress",L"origin",L"main"};
 if(steps)steps(L"最新の変更を取得しています。",ProcessDisplayCommand(exe_,fetch),0,2,nullptr);
 auto fetched=RunProcess(exe_,fetch,r.folder,L"",[&](const std::wstring& output){
  if(!progress)return;const auto value=ParseGitTransferProgress(output);if(!value||(value->first==previous&&value->second==previousTotal))return;
  previous=value->first;previousTotal=value->second;progress(previous,previousTotal);
 });
 AppendResult(result,fetched);if(steps)steps(L"最新の変更の取得結果",fetched.command,1,2,&fetched);if(fetched.exitCode)return result;
 auto upToDate=Run(r.folder,{L"merge-base",L"--is-ancestor",L"FETCH_HEAD",L"HEAD"});
 if(upToDate.exitCode==0){
  auto lfs=LfsPull(r,{},progress,steps);AppendResult(result,lfs);return result;
 }
 if(upToDate.exitCode!=1){AppendResult(result,upToDate);return result;}
 auto fastForward=Run(r.folder,{L"merge-base",L"--is-ancestor",L"HEAD",L"FETCH_HEAD"});
 if(fastForward.exitCode>1){AppendResult(result,fastForward);return result;}
 const bool diverged=fastForward.exitCode==1;
 if(diverged){
  auto dirty=Run(r.folder,{L"status",L"--porcelain=v1",L"-z",L"--untracked-files=all"});
  if(dirty.exitCode){AppendResult(result,dirty);return result;}
  if(!dirty.output.empty()){result.exitCode=1;result.error=L"自分と他の人の変更が両方進んでいます。未保存の変更を保護するため取り込みを止めました。\n送信する変更を選んで「保存して送信」を実行してください。未選択の変更が残る場合は、その変更も保存するか作業フォルダー外へ退避してから「更新」してください。";return result;}
  auto configured=Configure(r);if(configured.exitCode){AppendResult(result,configured);return result;}
 }
 std::vector<std::wstring> merge{L"merge",L"--progress",L"--no-edit",L"--no-autostash",L"--no-overwrite-ignore",diverged?L"--ff":L"--ff-only",L"--commit",L"FETCH_HEAD"};
 if(steps)steps(L"取得した変更を取り込んでいます。",ProcessDisplayCommand(exe_,merge),1,2,nullptr);
 previous=previousTotal=-1;
 auto merged=RunProcess(exe_,merge,r.folder,L"",[&](const std::wstring& output){
  if(!progress)return;const auto value=ParseGitTransferProgress(output);if(!value||(value->first==previous&&value->second==previousTotal))return;
  previous=value->first;previousTotal=value->second;progress(previous,previousTotal);
 });
 if(merged.exitCode){
  const auto state=OperationState(r);
  if(!state.conflicts.empty())merged.error=ConflictMessage(state);
  else if(state.merging)merged.error=std::wstring(L"取り込みの確定が途中で止まりました。\n")+recovery;
  else if(merged.output.find(L"would be overwritten")!=std::wstring::npos)merged.error=L"未保存のファイルと取得する変更が重なっています。変更を保存するか作業フォルダー外へ退避してから、再実行してください。現在の変更は破棄していません。";
 }
 AppendResult(result,merged);if(steps)steps(L"変更の取り込み結果",merged.command,2,2,&merged);if(merged.exitCode)return result;
 auto lfs=LfsPull(r,{},progress,steps);AppendResult(result,lfs);return result;
}

ProcessResult GitClient::Push(const Repository& r,const GitStepCallback& steps)const{
 auto ready=RequireReady(r);if(ready.exitCode)return ready;NormalizeGithubRemote(r);
 const std::vector<std::wstring> args{L"push",L"--porcelain",L"origin",L"HEAD:main"};
 auto send=[&]{if(steps)steps(L"3/3 GitHubへ送信しています。",ProcessDisplayCommand(exe_,args),2,3,nullptr);auto sent=Run(r.folder,args);if(steps)steps(sent.exitCode?L"送信結果を確認しています。":L"3/3 送信が完了しました。",sent.command,sent.exitCode?2:3,3,&sent);return sent;};
 auto first=send();
 // Retry only a concurrent update rejection, never authentication or hook failures.
 if(!first.exitCode||first.output.find(L"[rejected]")==std::wstring::npos||
   (first.output.find(L"fetch first")==std::wstring::npos&&first.output.find(L"non-fast-forward")==std::wstring::npos))return first;
 ProcessResult result;AppendResult(result,first);
 const auto relay=[&](const std::wstring& stage,const std::wstring& command,int,int,const ProcessResult* step){if(steps)steps(L"3/3 他の人の変更を取り込んで再送信します。\n"+stage,command,2,3,step);};
 auto pulled=Pull(r,{},relay);AppendResult(result,pulled);if(pulled.exitCode)return result;
 auto retried=send();AppendResult(result,retried);
 if(retried.exitCode&&retried.output.find(L"[rejected]")!=std::wstring::npos)result.error=L"送信中に他の人の変更が追加されました。自分の変更はこのPCに保存済みです。もう一度「保存して送信」を実行してください。";
 return result;
}

ProcessResult GitClient::FinishMerge(const Repository& r,const GitOperationState& expected)const{
 if(!MainBranch(Branch(r)))return{1,{},L"mainで取り込みを完了してください。"};
 const auto state=OperationState(r);
 if(!state.error.empty())return{1,{},state.error};
 if(!state.conflicts.empty())return{1,{},ConflictMessage(state)};
 if(!state.merging)return{1,{},L"完了待ちの取り込みはありません。"};
 if(!expected.merging||state.head!=expected.head||state.mergeHead!=expected.mergeHead||state.staged!=expected.staged)return{1,{},L"確認中に取り込み内容が変わりました。もう一度、取り込み内容を確認してください。"};
 auto configured=Configure(r);if(configured.exitCode)return configured;
 // Never add all files or commit selected paths during a merge.
 return Run(r.folder,{L"commit",L"--no-edit"});
}

ProcessResult GitClient::MarkResolved(const Repository& r,const std::wstring& path)const{
 const auto state=OperationState(r);
 if(!state.error.empty())return{1,{},state.error};
 if(!state.merging||std::find(state.conflicts.begin(),state.conflicts.end(),path)==state.conflicts.end())return{1,{},L"このファイルは取り込みの競合対象ではありません。"};
 return Run(r.folder,{L"--literal-pathspecs",L"add",L"--",path});
}
}
