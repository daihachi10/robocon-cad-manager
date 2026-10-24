#include "git.h"
#include <iostream>
#include <stdexcept>

namespace {
void Require(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
void Write(const std::filesystem::path& path,const std::string& data){std::ofstream f(path,std::ios::binary);f<<data;Require(bool(f),"write fixture failed");}
std::string Read(const std::filesystem::path& path){std::ifstream f(path,std::ios::binary);return std::string(std::istreambuf_iterator<char>(f),{});}
}
int wmain(int argc,wchar_t** argv){
 if(argc!=3)return 1;
 try{
  const std::filesystem::path root=argv[2];
  Require(!std::filesystem::exists(root),"fixture directory must be new");
  const auto a=root/L"作業 A",b=root/L"作業 B",remote=root/L"remote.git";
  std::filesystem::create_directories(a);std::filesystem::create_directories(remote);
  SetEnvironmentVariableW(L"GIT_CONFIG_GLOBAL",(root/L"empty-global").c_str());SetEnvironmentVariableW(L"GIT_CONFIG_NOSYSTEM",L"1");
  for(const auto name:{L"GIT_AUTHOR_NAME",L"GIT_AUTHOR_EMAIL",L"GIT_COMMITTER_NAME",L"GIT_COMMITTER_EMAIL"})SetEnvironmentVariableW(name,nullptr);
  SetEnvironmentVariableW(L"GIT_EDITOR",L"must-not-start-editor");SetEnvironmentVariableW(L"GIT_MERGE_AUTOEDIT",L"yes");
  rc::GitClient git(argv[1]);
  auto run=[&](const std::filesystem::path& path,const std::vector<std::wstring>& args){auto result=git.Run(path,args);if(result.exitCode){std::wcerr<<result.command<<L"\n"<<result.output<<L"\n"<<result.error;throw std::runtime_error("fixture Git failed");}return rc::Trim(result.output);};
  auto ok=[&](const rc::ProcessResult& result,const char* message){if(result.exitCode)std::wcerr<<result.command<<L"\n"<<result.output<<L"\n"<<result.error;Require(!result.exitCode,message);};
  run(remote,{L"init",L"--bare",L"-b",L"main"});run(a,{L"init",L"-b",L"main"});run(a,{L"remote",L"add",L"origin",remote.wstring()});
  rc::Repository ra{L"A",remote.wstring(),a.wstring(),L"設計 A",L"a@example.invalid"},rb{L"B",remote.wstring(),b.wstring(),L"設計 B",L"b@example.invalid"};
  Write(a/L"共通.txt","base\n");Write(a/L"部品.SLDPRT",std::string("base\0cad",8));
  ok(git.CommitPush(ra,{L"共通.txt",L"部品.SLDPRT"},L"base"),"initial push");ok(git.Clone(rb),"clone B");ok(git.Configure(rb),"configure B");
  run(a,{L"config",L"core.editor",L"must-not-start-editor"});
  run(a,{L"config",L"pull.rebase",L"true"});run(a,{L"config",L"merge.ff",L"only"});
  Require(run(a,{L"var",L"GIT_EDITOR"})==L"true","editor override missing");
  wchar_t editor[100]{};GetEnvironmentVariableW(L"GIT_EDITOR",editor,100);Require(std::wstring(editor)==L"must-not-start-editor","parent environment was changed");

  Write(b/L"相手.txt","B\n");ok(git.CommitPush(rb,{L"相手.txt"},L"B update"),"B push");
  Write(a/L"自分.txt","A\n");int stepResults=0;
  auto steps=[&](const std::wstring&,const std::wstring&,int,int,const rc::ProcessResult* value){if(value)++stepResults;};
  auto concurrent=git.CommitPush(ra,{L"自分.txt"},L"A update",steps);ok(concurrent,"concurrent disjoint push was not recovered");
  Require(stepResults>=6&&concurrent.output.find(L"[rejected]")!=std::wstring::npos,"retry trace missing");
  Require(Read(a/L"相手.txt")=="B\n"&&Read(a/L"自分.txt")=="A\n","merge lost contents");
  Require(run(a,{L"rev-list",L"--parents",L"-1",L"HEAD"}).size()==122,"not a two-parent merge");
  ok(git.Pull(rb),"fast forward B");

  // Diverged pull must not stash or discard uncommitted work.
  Write(a/L"local.txt","committed\n");run(a,{L"add",L"local.txt"});run(a,{L"commit",L"-m",L"local"});
  Write(a/L"未保存.txt","keep me");const auto beforeDirty=run(a,{L"rev-parse",L"HEAD"});
  Write(b/L"remote.txt","remote\n");ok(git.CommitPush(rb,{L"remote.txt"},L"remote"),"remote update");
  Require(git.Pull(ra).exitCode!=0,"dirty divergent merge was allowed");Require(run(a,{L"rev-parse",L"HEAD"})==beforeDirty&&Read(a/L"未保存.txt")=="keep me","dirty data changed");
  std::filesystem::rename(a/L"未保存.txt",root/L"退避.txt");ok(git.Pull(ra),"clean divergent pull");ok(git.Push(ra),"publish divergence");ok(git.Pull(rb),"sync B");

  // Reproduce MERGE_HEAD left behind by an interrupted editor, with a swap file.
  Write(b/L"b3.txt","B3");ok(git.CommitPush(rb,{L"b3.txt"},L"B3"),"B3 push");
  Write(a/L"a3.txt","A3");run(a,{L"add",L"a3.txt"});run(a,{L"commit",L"-m",L"A3"});run(a,{L"fetch",L"origin",L"main"});run(a,{L"merge",L"--no-commit",L"--no-ff",L"FETCH_HEAD"});
  const auto swap=a/L".git"/L".MERGE_MSG.swp";Write(swap,"unsaved editor data must survive");
  const auto pending=git.OperationState(ra);Require(pending.merging&&pending.conflicts.empty()&&pending.error.empty(),"pending merge not detected");
  Require(git.Pull(ra).exitCode&&git.Push(ra).exitCode&&git.CommitPush(ra,{L"b3.txt"},L"partial").exitCode,"unfinished merge not blocked");
  Write(a/L"別の作業.txt","not part of merge");run(a,{L"add",L"別の作業.txt"});Require(git.FinishMerge(ra,pending).exitCode!=0,"stale confirmation accepted");run(a,{L"reset",L"--",L"別の作業.txt"});
  ok(git.FinishMerge(ra,git.OperationState(ra)),"pending merge recovery");
  Require(Read(swap)=="unsaved editor data must survive","swap was deleted or changed");
  Require(git.Run(a,{L"cat-file",L"-e",L"HEAD:別の作業.txt"}).exitCode!=0&&Read(a/L"別の作業.txt")=="not part of merge","unselected file included");
  std::filesystem::rename(a/L"別の作業.txt",root/L"別の作業.txt");ok(git.Push(ra),"push recovered merge");ok(git.Pull(rb),"B sync recovered merge");

  // Binary conflict: preserve both index stages, refuse automatic completion.
  const auto ours=std::string("ours\0cad",8),theirs=std::string("theirs\0cad",10);
  Write(b/L"部品.SLDPRT",theirs);ok(git.CommitPush(rb,{L"部品.SLDPRT"},L"B CAD"),"B CAD push");
  Write(a/L"部品.SLDPRT",ours);const auto conflict=git.CommitPush(ra,{L"部品.SLDPRT"},L"A CAD");
  Require(conflict.exitCode!=0&&conflict.error.find(L"競合")!=std::wstring::npos,"binary conflict not explained");
  auto conflicted=git.OperationState(ra);Require(conflicted.merging&&conflicted.conflicts==std::vector<std::wstring>{L"部品.SLDPRT"},"wrong conflicts");
  Require(Read(a/L"部品.SLDPRT")==ours&&git.FinishMerge(ra,conflicted).exitCode!=0,"binary conflict automatically resolved");
  const auto oursBackup=root/L"ours.bin",theirsBackup=root/L"theirs.bin";
  ok(rc::RunProcessToFile(argv[1],{L"show",L":2:部品.SLDPRT"},a,oursBackup),"ours stage");ok(rc::RunProcessToFile(argv[1],{L"show",L":3:部品.SLDPRT"},a,theirsBackup),"theirs stage");
  Require(Read(oursBackup)==ours&&Read(theirsBackup)==theirs,"conflict versions not retained");
  ok(git.MarkResolved(ra,L"部品.SLDPRT"),"mark chosen content resolved");ok(git.FinishMerge(ra,git.OperationState(ra)),"finish resolved conflict");ok(git.Push(ra),"push resolution");

  // A failed network/path push keeps the commit for a later send-only retry.
  run(a,{L"remote",L"set-url",L"origin",(root/L"offline.git").wstring()});Write(a/L"再送.txt","retry");
  Require(git.CommitPush(ra,{L"再送.txt"},L"offline").exitCode!=0,"offline fixture unexpectedly pushed");const auto saved=run(a,{L"rev-parse",L"HEAD"});
  run(a,{L"remote",L"set-url",L"origin",remote.wstring()});ok(git.CommitPush(ra,{},L""),"send-only retry");Require(run(remote,{L"rev-parse",L"main"})==saved,"saved commit not retried");
  std::filesystem::create_directories(a/L".git"/L"rebase-merge");Require(git.Pull(ra).exitCode&&git.Push(ra).exitCode,"rebase state ignored");
  std::cout<<"PASS: no editor, concurrent send recovery, clean divergence, dirty preservation, pending merge + swap preservation, stale confirmation, binary conflict + explicit resolution, offline retry, rebase guard.\n";
  return 0;
 }catch(const std::exception& ex){std::cerr<<ex.what()<<"\n";return 1;}
}
