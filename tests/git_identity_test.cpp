#include "git.h"
#include <iostream>
#include <stdexcept>

int wmain(int argc,wchar_t** argv){
 if(argc!=3)return 1;
 try{
  const auto transferProgress=rc::ParseGitTransferProgress(L"remote: Receiving objects: 42% (21/50), 1.00 MiB");
  if(!transferProgress||transferProgress->first!=21||transferProgress->second!=50)throw std::runtime_error("git transfer progress was not parsed");
  const auto lfsProgress=rc::ParseGitTransferProgress(L"Receiving objects: 100% (50/50)\rDownloading LFS objects: 25% (1/4)");
  if(!lfsProgress||lfsProgress->first!=1||lfsProgress->second!=4)throw std::runtime_error("latest LFS progress was not selected");
  const auto root=std::filesystem::path(argv[2])/L"identity-regression";
  const auto work=root/L"既存 フォルダ",remote=root/L"remote.git",global=root/L"isolated-global.config";
  std::filesystem::create_directories(work);std::filesystem::create_directories(remote);
  SetEnvironmentVariableW(L"GIT_CONFIG_GLOBAL",global.c_str());
  SetEnvironmentVariableW(L"GIT_CONFIG_NOSYSTEM",L"1");
  for(const auto variable:{L"GIT_AUTHOR_NAME",L"GIT_AUTHOR_EMAIL",L"GIT_COMMITTER_NAME",L"GIT_COMMITTER_EMAIL",L"EMAIL"})SetEnvironmentVariableW(variable,nullptr);
  rc::GitClient git(argv[1]);
  auto require=[](bool ok,const char* message){if(!ok)throw std::runtime_error(message);};
  auto run=[&](const std::filesystem::path& folder,const std::vector<std::wstring>& args){
   auto result=git.Run(folder,args);if(result.exitCode){std::wcerr<<result.error<<L"\n"<<result.output;throw std::runtime_error("git operation failed");}return rc::Trim(result.output);
  };
  run(remote,{L"init",L"--bare",L"--quiet"});
  run(work,{L"init",L"-b",L"main",L"--quiet"});
  run(work,{L"config",L"--local",L"user.useConfigOnly",L"true"});
  run(work,{L"remote",L"add",L"origin",remote.wstring()});
  std::ofstream(work/L"初回.txt")<<"first";
  run(work,{L"add",L"--",L"初回.txt"});
  require(git.Run(work,{L"commit",L"-m",L"must fail without identity"}).exitCode==128,"missing identity was not reproduced");
  rc::Repository repo{L"test",remote.wstring(),work.wstring(),L"設計 太郎",L"first@example.invalid"};
  struct Progress {std::wstring stage,command;int completed{},total{};bool hasResult{};};std::vector<Progress> progress;
  auto callback=[&](const std::wstring& stage,const std::wstring& command,int completed,int total,const rc::ProcessResult* result){progress.push_back({stage,command,completed,total,result!=nullptr});};
  require(git.CommitPush(repo,{L"初回.txt"},L"identity supplied by application",callback).exitCode==0,"application did not configure identity before commit/push");
  require(progress.size()==6&&progress[0].completed==0&&progress[1].completed==1&&progress[3].completed==2&&progress[5].completed==3,"add/commit/push progress was not reported");
  require(progress[0].command.find(L"add")!=std::wstring::npos&&progress[2].command.find(L"commit")!=std::wstring::npos&&progress[4].command.find(L"push")!=std::wstring::npos,"running add/commit/push commands were not reported");
  require(run(work,{L"log",L"-1",L"--format=%an <%ae>"})==L"設計 太郎 <first@example.invalid>","wrong first commit identity");

  // Saving settings updates an existing repository without cloning it again.
  repo.userName=L"変更 後の氏名";repo.userEmail=L"changed@example.invalid";
  require(git.Configure(repo).exitCode==0,"settings were not applied to existing repository");
  require(run(work,{L"config",L"--local",L"--get",L"user.name"})==repo.userName,"name was not saved locally");
  require(run(work,{L"config",L"--local",L"--get",L"user.email"})==repo.userEmail,"email was not saved locally");
  require(!std::filesystem::exists(global),"application changed global configuration");

  // A missing local identity is healed on the next save/send, even with a different global user.
  run(work,{L"config",L"--file",global.wstring(),L"user.name",L"Other global user"});
  run(work,{L"config",L"--file",global.wstring(),L"user.email",L"other@example.invalid"});
  run(work,{L"config",L"--local",L"--unset",L"user.name"});run(work,{L"config",L"--local",L"--unset",L"user.email"});
  std::ofstream(work/L"追加.txt")<<"second";
  require(git.CommitPush(repo,{L"追加.txt"},L"repair missing local configuration").exitCode==0,"identity was not restored before sending");
  require(run(work,{L"log",L"-1",L"--format=%an <%ae>"})==L"変更 後の氏名 <changed@example.invalid>","global user overrode application identity");
  std::filesystem::create_directories(work/L"機構"/L"部品");std::ofstream(work/L"機構"/L"部品"/L"追加.SLDPRT")<<"cad";
  require(git.CommitPush(repo,{L"機構/部品/追加.SLDPRT"},L"フォルダー内の部品を追加").exitCode==0,"nested file commit failed");
  const auto latest=git.LatestCommits(repo,{L"機構/部品/追加.SLDPRT",L"機構/部品",L"機構"});
  for(const auto& path:{L"機構/部品/追加.SLDPRT",L"機構/部品",L"機構"})require(latest.contains(path)&&latest.at(path).message==L"フォルダー内の部品を追加","folder latest commit was not aggregated");
  require(run(work,{L"config",L"--file",global.wstring(),L"--get",L"user.name"})==L"Other global user","global user was modified");
  const auto before=run(work,{L"rev-parse",L"HEAD"});repo.userName=L"  ";
  const auto invalid=git.CommitPush(repo,{L"追加.txt"},L"must not commit");
  require(invalid.exitCode!=0&&!invalid.error.empty(),"missing name was accepted");
  repo.userName=L"valid";repo.userEmail=L" ";require(git.Configure(repo).exitCode!=0,"missing email was accepted");
  require(run(work,{L"rev-parse",L"HEAD"})==before,"invalid identity changed history");
  std::cout<<"PASS: reproduced exit 128, configured existing clone, saved settings, repaired identity before commit/push, preserved global config, rejected blanks.\n";
  return 0;
 }catch(const std::exception& error){std::cerr<<error.what()<<"\n";return 1;}
}
