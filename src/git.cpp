#include "git.h"
#include "json.h"
#include <bcrypt.h>
#pragma comment(lib, "bcrypt.lib")
namespace rc { namespace {
std::filesystem::path FindGit(){wchar_t b[MAX_PATH];GetModuleFileNameW(nullptr,b,MAX_PATH);auto p=std::filesystem::path(b).parent_path()/L"tools"/L"git"/L"bin"/L"git.exe";if(std::filesystem::exists(p))return p;wchar_t env[MAX_PATH];auto n=GetEnvironmentVariableW(L"ProgramFiles",env,MAX_PATH);return std::filesystem::path(std::wstring(env,n))/L"Git"/L"cmd"/L"git.exe";}
std::wstring LfsError(const ProcessResult& r){if(!r.error.empty())return r.error;if(r.output.find(L"Authentication")!=std::wstring::npos||r.output.find(L"Username")!=std::wstring::npos)return L"GitHubに認証できません。Git Credential ManagerでGitHubへサインインしてください。";if(r.output.find(L"already locked")!=std::wstring::npos)return L"このファイルは他の利用者がロックしています。";if(r.output.find(L"connect")!=std::wstring::npos||r.output.find(L"network")!=std::wstring::npos)return L"GitHubへ接続できません。ネットワークを確認してください。";return r.output.empty()?L"Git LFSの処理に失敗しました。":Trim(r.output);}
std::vector<LockInfo> ParseLocks(const std::wstring& raw,const Repository& repo,std::wstring& error){auto j=Json::Parse(raw,&error);if(!j)return{};std::vector<LockInfo> out;auto append=[&](const Json&v,std::optional<bool> verifiedOurs={}){if(v.type!=Json::Type::Object)return;LockInfo l;if(auto x=v.Find(L"id"))l.id=x->StringOr();if(auto x=v.Find(L"path"))l.path=x->StringOr();if(auto x=v.Find(L"ours"))l.ours=x->BoolOr();if(verifiedOurs)l.ours=*verifiedOurs;if(auto o=v.Find(L"owner");o&&o->type==Json::Type::Object)if(auto n=o->Find(L"name"))l.owner=n->StringOr();if(!l.ours&&!repo.userName.empty()&&l.owner==repo.userName)l.ours=true;if(!l.path.empty())out.push_back(std::move(l));};if(j->type==Json::Type::Array){for(const auto&v:j->array)append(v,true);return out;}if(auto values=j->Find(L"locks");values&&values->type==Json::Type::Array)for(const auto&v:values->array)append(v);if(auto values=j->Find(L"ours");values&&values->type==Json::Type::Array)for(const auto&v:values->array)append(v,true);if(auto values=j->Find(L"theirs");values&&values->type==Json::Type::Array)for(const auto&v:values->array)append(v,false);if(auto one=j->Find(L"lock");one&&one->type==Json::Type::Object)append(*one,true);return out;}
bool HasLine(const std::wstring& all,const std::wstring& line){std::wistringstream s(all);for(std::wstring value;std::getline(s,value);)if(Trim(value)==line)return true;return false;}

bool EnsureLocalLfsAttributes(const Repository& repo,std::wstring& error){
 error.clear();const auto attributes=std::filesystem::path(repo.folder)/L".git"/L"info"/L"attributes";std::error_code directoryError;
 std::filesystem::create_directories(attributes.parent_path(),directoryError);if(directoryError){error=L"ローカルのLFS属性設定フォルダーを作成できません。";return false;}
 std::ifstream input(attributes,std::ios::binary);std::string bytes((std::istreambuf_iterator<char>(input)),{});auto text=Wide(bytes);
 const std::vector<std::wstring> lines={
  L"*.sldprt filter=lfs diff=lfs merge=lfs -text",L"*.SLDPRT filter=lfs diff=lfs merge=lfs -text",
  L"*.sldasm filter=lfs diff=lfs merge=lfs -text",L"*.SLDASM filter=lfs diff=lfs merge=lfs -text",
  L"*.slddrw filter=lfs diff=lfs merge=lfs -text",L"*.SLDDRW filter=lfs diff=lfs merge=lfs -text",
  L"*.stl filter=lfs diff=lfs merge=lfs -text",L"*.STL filter=lfs diff=lfs merge=lfs -text",
  L"*.step filter=lfs diff=lfs merge=lfs -text",L"*.STEP filter=lfs diff=lfs merge=lfs -text",
  L"*.stp filter=lfs diff=lfs merge=lfs -text",L"*.STP filter=lfs diff=lfs merge=lfs -text",
  L"*.dxf filter=lfs diff=lfs merge=lfs -text",L"*.DXF filter=lfs diff=lfs merge=lfs -text",
  L"*.dwg filter=lfs diff=lfs merge=lfs -text",L"*.DWG filter=lfs diff=lfs merge=lfs -text",
  L"*.pdf filter=lfs diff=lfs merge=lfs -text",L"*.PDF filter=lfs diff=lfs merge=lfs -text"};
 bool changed=false;for(const auto& line:lines)if(!HasLine(text,line)){if(!text.empty()&&text.back()!=L'\n')text+=L'\n';text+=line+L'\n';changed=true;}
 if(!changed)return true;const auto temporary=attributes.wstring()+L".robocon";std::ofstream output(temporary,std::ios::binary|std::ios::trunc);if(!output){error=L"ローカルのLFS属性設定を書き込めません。";return false;}output<<Utf8(text);output.close();if(!output||!MoveFileExW(temporary.c_str(),attributes.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)){DeleteFileW(temporary.c_str());error=L"ローカルのLFS属性設定を保存できません。";return false;}return true;
}

struct LfsPointer { std::wstring oid; uint64_t size{}; };

std::optional<LfsPointer> ReadLfsPointer(const std::filesystem::path& path){
 std::ifstream input(path,std::ios::binary);if(!input)return{};
 std::string bytes((std::istreambuf_iterator<char>(input)),{});if(bytes.size()>1024)return{};
 const auto text=Wide(bytes);if(!text.starts_with(L"version https://git-lfs.github.com/spec/v1"))return{};
 const auto oidAt=text.find(L"oid sha256:"),sizeAt=text.find(L"size ");if(oidAt==std::wstring::npos||sizeAt==std::wstring::npos)return{};
 const auto oidEnd=text.find_first_of(L"\r\n",oidAt);const auto oid=Trim(text.substr(oidAt+11,oidEnd==std::wstring::npos?std::wstring::npos:oidEnd-(oidAt+11)));
 const auto sizeEnd=text.find_first_of(L"\r\n",sizeAt);const auto size=Trim(text.substr(sizeAt+5,sizeEnd==std::wstring::npos?std::wstring::npos:sizeEnd-(sizeAt+5)));
 if(oid.size()!=64||oid.find_first_not_of(L"0123456789abcdefABCDEF")!=std::wstring::npos)return{};
 try{return LfsPointer{oid,std::stoull(size)};}catch(...){return{};}
}

bool Sha256Matches(const std::filesystem::path& file,const std::wstring& expected){
 BCRYPT_ALG_HANDLE algorithm{};BCRYPT_HASH_HANDLE hash{};DWORD objectSize{},written{};
 if(BCryptOpenAlgorithmProvider(&algorithm,BCRYPT_SHA256_ALGORITHM,nullptr,0)<0||
    BCryptGetProperty(algorithm,BCRYPT_OBJECT_LENGTH,reinterpret_cast<PUCHAR>(&objectSize),sizeof(objectSize),&written,0)<0){if(algorithm)BCryptCloseAlgorithmProvider(algorithm,0);return false;}
 std::vector<BYTE> object(objectSize),digest(32),buffer(1024*1024);
 bool ok=BCryptCreateHash(algorithm,&hash,object.data(),objectSize,nullptr,0,0)>=0;
 std::ifstream input(file,std::ios::binary);
 if(!input)ok=false;
 while(ok&&input){input.read(reinterpret_cast<char*>(buffer.data()),static_cast<std::streamsize>(buffer.size()));const auto count=input.gcount();if(count>0&&BCryptHashData(hash,buffer.data(),static_cast<ULONG>(count),0)<0)ok=false;}
 if(ok&&input.bad())ok=false;
 if(ok&&BCryptFinishHash(hash,digest.data(),static_cast<ULONG>(digest.size()),0)<0)ok=false;
 std::wstring actual;static constexpr wchar_t digits[]=L"0123456789abcdef";if(ok)for(const auto byte:digest){actual+=digits[byte>>4];actual+=digits[byte&15];}
 if(hash)BCryptDestroyHash(hash);BCryptCloseAlgorithmProvider(algorithm,0);
 return ok&&CompareStringOrdinal(actual.c_str(),-1,expected.c_str(),-1,TRUE)==CSTR_EQUAL;
}

bool MaterializeLfsPointers(const Repository& repo,const std::vector<std::wstring>& paths,std::wstring& error){
 error.clear();std::vector<std::filesystem::path> targets;
 if(paths.empty()){
  std::error_code ec;std::filesystem::recursive_directory_iterator it(repo.folder,std::filesystem::directory_options::skip_permission_denied,ec),end;
  while(!ec&&it!=end){const auto current=it->path();if(it->is_directory(ec)&&current.filename()==L".git"){it.disable_recursion_pending();++it;continue;}if(!ec&&it->is_regular_file(ec)&&ReadLfsPointer(current))targets.push_back(current);ec.clear();++it;}
 }else for(const auto& path:paths)targets.push_back(std::filesystem::path(repo.folder)/std::filesystem::path(path));
 const auto objects=std::filesystem::path(repo.folder)/L".git"/L"lfs"/L"objects";
 std::vector<std::wstring> failures;
 for(const auto& target:targets){
  const auto pointer=ReadLfsPointer(target);if(!pointer)continue;
  const auto attributes=GetFileAttributesW(target.c_str());
  if(attributes!=INVALID_FILE_ATTRIBUTES&&(attributes&FILE_ATTRIBUTE_REPARSE_POINT)){failures.push_back(target.filename().wstring()+L"（リンク先のファイルはLFS本体へ置き換えません）");continue;}
  const auto object=objects/pointer->oid.substr(0,2)/pointer->oid.substr(2,2)/pointer->oid;
  std::error_code sizeError;if(!std::filesystem::exists(object)||std::filesystem::file_size(object,sizeError)!=pointer->size||sizeError||!Sha256Matches(object,pointer->oid)){failures.push_back(target.lexically_relative(repo.folder).generic_wstring()+L"（LFS本体を取得できません）");continue;}
  wchar_t temporary[MAX_PATH]{};if(!GetTempFileNameW(target.parent_path().c_str(),L"rc",0,temporary)){failures.push_back(target.filename().wstring()+L"（一時ファイルを作成できません）");continue;}
  DeleteFileW(temporary);
  if(!CopyFileW(object.c_str(),temporary,FALSE)){failures.push_back(target.filename().wstring()+L"（LFS本体を展開できません）");continue;}
  SetFileAttributesW(target.c_str(),FILE_ATTRIBUTE_NORMAL);
  if(!MoveFileExW(temporary,target.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)){DeleteFileW(temporary);failures.push_back(target.filename().wstring()+L"（LFS本体を置き換えられません）");}
 }
 if(!failures.empty()){error=L"次のファイルはLFS本体へ展開できませんでした。\r\n";for(const auto& failure:failures)error+=L"・"+failure+L"\r\n";return false;}
 return true;
}
bool IsMainRef(const std::wstring& branch){return branch==L"main"||branch==L"origin/main"||branch==L"refs/heads/main"||branch==L"refs/remotes/origin/main";}
}
std::optional<std::pair<int,int>> ParseGitTransferProgress(const std::wstring& output){
 size_t start=std::wstring::npos;
 for(const auto marker:{L"Updating files:",L"Receiving objects:",L"Downloading LFS objects:",L"download ",L"checkout "}){const auto found=output.rfind(marker);if(found!=std::wstring::npos&&(start==std::wstring::npos||found>start))start=found;}
 if(start==std::wstring::npos)return{};
 const auto open=output.find(L'(',start),slash=output.find(L'/',open==std::wstring::npos?start:open+1),close=output.find(L')',slash==std::wstring::npos?start:slash+1);
 try{
  if(open!=std::wstring::npos&&slash!=std::wstring::npos&&close!=std::wstring::npos){const int completed=std::stoi(output.substr(open+1,slash-open-1)),total=std::stoi(output.substr(slash+1,close-slash-1));if(completed>=0&&total>0&&completed<=total)return std::pair{completed,total};}
  // Git LFS forced progress: "download 2/5 1048576/... path".
  const auto first=output.find_first_of(L"0123456789",start);const auto plainSlash=first==std::wstring::npos?std::wstring::npos:output.find(L'/',first);
  if(first!=std::wstring::npos&&plainSlash!=std::wstring::npos){const auto next=output.find_first_not_of(L"0123456789",plainSlash+1);const int completed=std::stoi(output.substr(first,plainSlash-first)),total=std::stoi(output.substr(plainSlash+1,next-plainSlash-1));if(completed>=0&&total>0&&completed<=total)return std::pair{completed,total};}
 }catch(...){ }
 return{};
}
GitClient::GitClient(std::filesystem::path executable):exe_(executable.empty()?FindGit():std::move(executable)){} bool GitClient::Available()const{return std::filesystem::exists(exe_);} ProcessResult GitClient::Run(const std::filesystem::path&r,const std::vector<std::wstring>&a)const{return RunProcess(exe_,a,r);}
void GitClient::NormalizeGithubRemote(const Repository&r)const{if(r.folder.empty()||!std::filesystem::exists(r.folder))return;auto current=Run(r.folder,{L"remote",L"get-url",L"origin"});if(current.exitCode)return;auto url=Trim(current.output);if(url.starts_with(L"http://github.com/")){url.replace(0,5,L"https");Run(r.folder,{L"remote",L"set-url",L"origin",url});}}
ProcessResult GitClient::Configure(const Repository&r)const{
 const auto name=Trim(r.userName),email=Trim(r.userEmail);
 if(name.empty()||email.empty())return{1,{},L"「設定」で「変更記録に使う氏名」と「変更記録に使うメールアドレス」を入力してください。"};
 auto result=Run(r.folder,{L"config",L"--local",L"user.name",name});
 if(!result.exitCode)result=Run(r.folder,{L"config",L"--local",L"user.email",email});
 if(result.exitCode){result.error=L"作業フォルダのGitに氏名・メールアドレスを設定できません。作業フォルダと書き込み権限を確認してください。";return result;}
 const auto savedName=Run(r.folder,{L"config",L"--local",L"--get",L"user.name"});
 const auto savedEmail=Run(r.folder,{L"config",L"--local",L"--get",L"user.email"});
 if(savedName.exitCode||savedEmail.exitCode||Trim(savedName.output)!=name||Trim(savedEmail.output)!=email)
  return{1,{},L"Gitに保存した氏名・メールアドレスを確認できません。設定を保存し直してください。"};
 return{};
}
ProcessResult GitClient::Clone(const Repository&r,const GitStepCallback& steps)const{
 auto url=r.remote;if(url.starts_with(L"http://github.com/"))url.replace(0,5,L"https");
 const std::vector<std::wstring> clone{L"clone",L"--recurse-submodules",url,r.folder};
 if(steps)steps(L"1/3 リポジトリを複製しています。",ProcessDisplayCommand(exe_,clone),0,3,nullptr);
 auto result=Run({},clone);if(steps)steps(L"1/3 リポジトリの複製結果",result.command,1,3,&result);if(result.exitCode)return result;
 return LfsPull(r,{}, {},steps);
}

ProcessResult GitClient::LfsPull(const Repository&r,const std::vector<std::wstring>& paths,const GitTransferProgress& progress,const GitStepCallback& steps)const{
 NormalizeGithubRemote(r);
 const std::vector<std::wstring> install{L"lfs",L"install",L"--local"};
 if(steps)steps(L"大容量ファイル用のGit設定を確認しています。",ProcessDisplayCommand(exe_,install),0,paths.empty()?3:1,nullptr);
 auto installed=Run(r.folder,install);
 if(installed.exitCode){installed.error=LfsError(installed);if(steps)steps(L"大容量ファイル用のGit設定に失敗しました。",installed.command,0,paths.empty()?3:1,&installed);return installed;}
 std::wstring attributesError;
 if(!EnsureLocalLfsAttributes(r,attributesError)){installed.exitCode=1;installed.error=attributesError;if(steps)steps(L"大容量ファイル用のGit設定に失敗しました。",installed.command,0,paths.empty()?3:1,&installed);return installed;}
 std::wstring include;
 for(const auto& path:paths){if(!include.empty())include+=L",";include+=path;}
 // Empty include/exclude overrides per-user LFS filters. A CAD clone must contain all
 // LFS data tracked by its checked-out revision, not a 0 KB pointer.
 std::vector<std::wstring> command{L"lfs",L"pull",L"--include="+include,L"--exclude="};
 if(steps)steps(paths.empty()?L"2/3 大容量ファイルの本体を取得しています。":L"大容量ファイルの本体を取得しています。",ProcessDisplayCommand(exe_,command),paths.empty()?1:0,paths.empty()?3:1,nullptr);
 int previous=-1,previousTotal=-1;
 auto result=RunProcess(exe_,command,r.folder,L"",[&](const std::wstring& output){
  if(!progress)return;const auto value=ParseGitTransferProgress(output);if(!value||(value->first==previous&&value->second==previousTotal))return;
  previous=value->first;previousTotal=value->second;progress(previous,value->second);
 },true);
 result.output=L"> "+installed.command+L"\r\n"+installed.output+L"\r\n"+result.output;
 result.command=installed.command+L"\r\n"+result.command;
 if(result.exitCode){result.error=LfsError(result);if(steps)steps(L"大容量ファイルの取得に失敗しました。",result.command,paths.empty()?2:1,paths.empty()?3:1,&result);return result;}
 // `pull` fetches LFS objects. Explicit checkout makes the working file real even
 // when clone/checkout had deliberately skipped the LFS smudge filter.
 const std::vector<std::wstring> checkout{L"lfs",L"checkout"};
 if(steps)steps(L"取得した大容量ファイルを展開しています。",ProcessDisplayCommand(exe_,checkout),paths.empty()?2:0,paths.empty()?3:1,nullptr);
 auto checked=RunProcess(exe_,checkout,r.folder,L"",{},true);
 checked.output=L"> "+result.command+L"\r\n"+result.output+L"\r\n"+checked.output;
 checked.command=result.command+L"\r\n"+checked.command;
 if(checked.exitCode)checked.error=LfsError(checked);
 if(!checked.exitCode){
  std::wstring materializeError;
  if(!MaterializeLfsPointers(r,paths,materializeError)){checked.exitCode=1;checked.error=materializeError;checked.output+=L"\r\n"+materializeError;}
 }
 if(steps)steps(checked.exitCode?L"大容量ファイルの展開に失敗しました。":L"大容量ファイルの取得が完了しました。",checked.command,paths.empty()?3:1,paths.empty()?3:1,&checked);
 return checked;
}
std::wstring GitClient::Branch(const Repository&r)const{auto x=Run(r.folder,{L"branch",L"--show-current"});return x.exitCode?L"":Trim(x.output);}
std::vector<GitStatus> GitClient::Status(const Repository&r,std::wstring&e)const{
 e.clear();auto result=Run(r.folder,{L"status",L"--porcelain=v1",L"-z",L"--untracked-files=all"});
 if(result.exitCode){e=L"作業フォルダを読み取れません。設定を確認してください。";return{};}
 std::vector<GitStatus> changes;size_t position=0;
 while(position<result.output.size()){
  const auto end=result.output.find(L'\0',position);if(end==std::wstring::npos)break;
  const auto record=result.output.substr(position,end-position);position=end+1;
  if(record.size()<4)continue;
  changes.push_back({record.substr(0,2),record.substr(3)});
  if(record[0]==L'R'||record[1]==L'R'||record[0]==L'C'||record[1]==L'C'){
   const auto sourceEnd=result.output.find(L'\0',position);if(sourceEnd==std::wstring::npos)break;position=sourceEnd+1;
  }
 }
 return changes;
}
std::map<std::wstring,LastCommit> GitClient::LatestCommits(const Repository&r,const std::vector<std::wstring>&paths)const{
 std::map<std::wstring,LastCommit> result;if(paths.empty())return result;
 const std::set<std::wstring> requested(paths.begin(),paths.end());auto log=Run(r.folder,{L"-c",L"core.quotepath=false",L"log",L"--format=%x1e%an%x1f%s",L"--name-only",L"--"});if(log.exitCode)return result;
 size_t start=0;while(start<log.output.size()){
  const auto end=log.output.find(L'\x1e',start);const auto record=log.output.substr(start,end==std::wstring::npos?std::wstring::npos:end-start);const auto separator=record.find(L'\x1f'),firstLine=record.find(L'\n');
  if(separator!=std::wstring::npos&&firstLine!=std::wstring::npos&&separator<firstLine){
   const LastCommit commit{Trim(record.substr(0,separator)),Trim(record.substr(separator+1,firstLine-separator-1))};std::wistringstream files(record.substr(firstLine+1));
   for(std::wstring path;std::getline(files,path);){path=Trim(path);if(path.empty())continue;if(requested.contains(path)&&!result.contains(path))result.emplace(path,commit);auto parent=std::filesystem::path(path).parent_path();while(!parent.empty()){const auto folder=parent.generic_wstring();if(requested.contains(folder)&&!result.contains(folder))result.emplace(folder,commit);parent=parent.parent_path();}}
  }
  if(end==std::wstring::npos)break;start=end+1;
 }
 return result;
}
std::vector<LockInfo> GitClient::ListLocks(const Repository&r,std::wstring&e)const{NormalizeGithubRemote(r);auto x=Run(r.folder,{L"lfs",L"locks",L"--verify",L"--json"});if(x.exitCode){e=LfsError(x);return{};}return ParseLocks(x.output,r,e);}
std::optional<LockInfo> GitClient::Lock(const Repository&r,const std::wstring&p,std::wstring&e)const{NormalizeGithubRemote(r);auto x=Run(r.folder,{L"lfs",L"lock",L"--json",L"--",p});if(x.exitCode){if(x.output.find(L"Lock exists")!=std::wstring::npos){std::wstring listError;auto current=ListLocks(r,listError);auto found=std::find_if(current.begin(),current.end(),[&](const LockInfo&lock){return lock.path==p;});if(found!=current.end()){if(found->ours)return*found;e=found->owner.empty()?L"このファイルは他の利用者がロックしています。":L"このファイルは "+found->owner+L" さんがロックしています。";return{};}if(!listError.empty()){e=listError;return{};}}e=LfsError(x);return{};}auto locks=ParseLocks(x.output,r,e);if(locks.empty()){e=L"Git LFSからロック情報を受け取れませんでした。";return{};}locks.front().ours=true;return locks.front();}
bool GitClient::Unlock(const Repository&r,const LockInfo&l,std::wstring&e)const{NormalizeGithubRemote(r);auto x=Run(r.folder,{L"lfs",L"unlock",L"--id",l.id,L"--json"});if(x.exitCode){e=LfsError(x);return false;}return true;} bool GitClient::IsDirty(const Repository&r,const std::wstring&p)const{auto x=Run(r.folder,{L"status",L"--porcelain=v1",L"--",p});return x.exitCode||!Trim(x.output).empty();}
ProcessResult GitClient::CommitPush(const Repository&r,const std::vector<std::wstring>&paths,const std::wstring&m,const GitStepCallback& progress)const{
 auto ready=RequireReady(r);if(ready.exitCode)return ready;if(paths.empty())return Push(r,progress);auto configured=Configure(r);if(configured.exitCode)return configured;
 constexpr int total=3;auto notify=[&](const std::wstring& stage,const std::wstring& command,int completed,const ProcessResult* result){if(progress)progress(stage,command,completed,total,result);};
 std::vector<std::wstring> add{L"add",L"--"};add.insert(add.end(),paths.begin(),paths.end());const auto addCommand=ProcessDisplayCommand(exe_,add);notify(L"1/3 選択したファイルを追加しています。",addCommand,0,nullptr);auto added=Run(r.folder,add);notify(L"1/3 ファイルの追加が完了しました。",added.command,1,&added);if(added.exitCode)return added;
 std::vector<std::wstring> commit{L"commit",L"-m",m,L"--"};commit.insert(commit.end(),paths.begin(),paths.end());const auto commitCommand=ProcessDisplayCommand(exe_,commit);notify(L"2/3 変更内容を記録しています。",commitCommand,1,nullptr);auto committed=Run(r.folder,commit);notify(L"2/3 変更内容の記録が完了しました。",committed.command,2,&committed);if(committed.exitCode)return committed;
 return Push(r,progress);
}
ProcessResult GitClient::SetupLfsLocking(const Repository&r,std::wstring&e)const{auto install=Run(r.folder,{L"lfs",L"install",L"--local"});if(install.exitCode){e=LfsError(install);return install;}const auto file=std::filesystem::path(r.folder)/L".gitattributes";std::ifstream input(file,std::ios::binary);std::string bytes((std::istreambuf_iterator<char>(input)),{});auto text=Wide(bytes);const std::vector<std::wstring> lines={L"*.sldprt filter=lfs diff=lfs merge=lfs -text lockable",L"*.SLDPRT filter=lfs diff=lfs merge=lfs -text lockable",L"*.sldasm filter=lfs diff=lfs merge=lfs -text lockable",L"*.SLDASM filter=lfs diff=lfs merge=lfs -text lockable",L"*.slddrw filter=lfs diff=lfs merge=lfs -text lockable",L"*.SLDDRW filter=lfs diff=lfs merge=lfs -text lockable",L"*.step filter=lfs diff=lfs merge=lfs -text",L"*.STEP filter=lfs diff=lfs merge=lfs -text"};for(const auto& line:lines)if(!HasLine(text,line)){if(!text.empty()&&text.back()!=L'\n')text+=L'\n';text+=line+L'\n';}std::ofstream output(file,std::ios::binary|std::ios::trunc);if(!output){e=L".gitattributesを書き込めません。";return{1,{},{e}};}output<<Utf8(text);auto remote=Run(r.folder,{L"remote",L"get-url",L"origin"});if(!remote.exitCode&&Trim(remote.output).starts_with(L"http://github.com/")){auto url=Trim(remote.output);url.replace(0,5,L"https");return Run(r.folder,{L"remote",L"set-url",L"origin",url});}return{};}
std::vector<GitRevision> GitClient::History(const Repository&r,const std::wstring&path,std::wstring&error)const{
 error.clear();std::vector<GitRevision> result;const auto log=Run(r.folder,{L"-c",L"core.quotepath=false",L"log",L"--follow",L"--format=%x1e%H%x1f%an%x1f%cd%x1f%s",L"--date=format:%Y/%m/%d %H:%M",L"--name-status",L"--",path});
 if(log.exitCode){error=L"ファイルの変更履歴を取得できません。";return result;}std::wstring historicalPath=path;size_t start=0;
 while(start<log.output.size()){const auto end=log.output.find(L'\x1e',start);const auto record=log.output.substr(start,end==std::wstring::npos?std::wstring::npos:end-start);const auto headerEnd=record.find(L'\n');
  if(headerEnd!=std::wstring::npos){const auto header=record.substr(0,headerEnd);const auto a=header.find(L'\x1f'),b=a==std::wstring::npos?std::wstring::npos:header.find(L'\x1f',a+1),c=b==std::wstring::npos?std::wstring::npos:header.find(L'\x1f',b+1);
   if(a!=std::wstring::npos&&b!=std::wstring::npos&&c!=std::wstring::npos){GitRevision revision{header.substr(0,a),header.substr(a+1,b-a-1),header.substr(b+1,c-b-1),Trim(header.substr(c+1)),historicalPath,true};std::wistringstream lines(record.substr(headerEnd+1));
    for(std::wstring line;std::getline(lines,line);){line=Trim(line);if(line.empty())continue;const auto tab=line.find(L'\t');if(tab==std::wstring::npos)continue;const auto status=line.substr(0,tab),values=line.substr(tab+1);if(status.starts_with(L"D")){revision.exists=false;revision.path=values;}else if(status.starts_with(L"R")){const auto second=values.find(L'\t');if(second!=std::wstring::npos){const auto oldPath=values.substr(0,second),newPath=values.substr(second+1);revision.path=newPath;if(newPath==historicalPath)historicalPath=oldPath;}}else if(status.starts_with(L"C")){const auto second=values.find(L'\t');revision.path=second==std::wstring::npos?values:values.substr(second+1);}}
    result.push_back(std::move(revision));}}
  if(end==std::wstring::npos)break;start=end+1;}
 return result;
}
bool GitClient::RestoreRevision(const Repository&r,const GitRevision&revision,const std::wstring&destination,std::wstring&error)const{
 error.clear();const auto target=std::filesystem::path(r.folder)/std::filesystem::path(destination);std::error_code fsError;std::filesystem::create_directories(target.parent_path(),fsError);if(fsError){error=L"復元先フォルダーを作成できません。";return false;}
 if(!revision.exists){SetFileAttributesW(target.c_str(),FILE_ATTRIBUTE_NORMAL);if(!DeleteFileW(target.c_str())&&GetLastError()!=ERROR_FILE_NOT_FOUND){error=L"選択した時点の削除状態へ戻せません。";return false;}return true;}
 wchar_t tempName[MAX_PATH]{};if(!GetTempFileNameW(target.parent_path().c_str(),L"rc",0,tempName)){error=L"復元用の一時ファイルを作成できません。";return false;}const std::filesystem::path temporary=tempName;
 const auto object=revision.sha+L":"+revision.path;const auto result=RunProcessToFile(exe_,{L"-c",L"core.quotepath=false",L"cat-file",L"--filters",L"--path="+destination,object},r.folder,temporary,true);if(result.exitCode){DeleteFileW(temporary.c_str());error=result.error.empty()?L"選択したコミットのファイル本体を取得できません。":Trim(result.error);return false;}
 SetFileAttributesW(target.c_str(),FILE_ATTRIBUTE_NORMAL);if(!MoveFileExW(temporary.c_str(),target.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)){DeleteFileW(temporary.c_str());error=L"復元したファイルを置き換えられません。";return false;}return true;
}
}
