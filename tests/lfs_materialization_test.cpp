#include "cad.h"
#include "git.h"
#include <iostream>
#include <stdexcept>

namespace {
void Require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
void Write(const std::filesystem::path& path,const std::string& value){std::ofstream file(path,std::ios::binary);file<<value;Require(bool(file),"write failed");}
bool IsPointer(const rc::Repository& repo,const std::wstring& relative){const auto files=rc::ScanCad(repo);const auto found=std::find_if(files.begin(),files.end(),[&](const rc::CadFile& file){return file.path.filename().wstring()==relative;});return found!=files.end()&&found->lfsPointer;}
}

int wmain(int argc,wchar_t** argv){
 if(argc!=3)return 1;
 try{
  const std::filesystem::path root=argv[2],remote=root/L"remote.git",seed=root/L"seed",clone=root/L"clone";
  Require(!std::filesystem::exists(root),"fixture directory must be new");std::filesystem::create_directories(remote);std::filesystem::create_directories(seed);
  SetEnvironmentVariableW(L"GIT_CONFIG_GLOBAL",(root/L"isolated-global.config").c_str());SetEnvironmentVariableW(L"GIT_CONFIG_NOSYSTEM",L"1");
  rc::GitClient git(argv[1]);
  auto run=[&](const std::filesystem::path& folder,const std::vector<std::wstring>& args){const auto result=git.Run(folder,args);if(result.exitCode){std::wcerr<<result.command<<L"\n"<<result.output<<L"\n"<<result.error;throw std::runtime_error("fixture Git command failed");}return result;};
  run(remote,{L"init",L"--bare",L"-b",L"main"});run(seed,{L"init",L"-b",L"main"});run(seed,{L"remote",L"add",L"origin",remote.wstring()});run(seed,{L"lfs",L"install",L"--local"});run(seed,{L"lfs",L"track",L"*.SLDPRT"});
  rc::Repository source{L"source",remote.wstring(),seed.wstring(),L"LFS試験",L"lfs@example.invalid"};
  std::string content(320*1024,'A');content.replace(100,8,"CAD-DATA");Write(seed/L"巨大.SLDPRT",content);
  Require(git.CommitPush(source,{L".gitattributes",L"巨大.SLDPRT"},L"LFS object").exitCode==0,"seed LFS push failed");
  rc::Repository destination{L"clone",remote.wstring(),clone.wstring(),L"LFS取得",L"pull@example.invalid"};
  const auto cloned=git.Clone(destination);if(cloned.exitCode)std::wcerr<<cloned.command<<L"\n"<<cloned.output<<L"\n"<<cloned.error;Require(cloned.exitCode==0,"clone plus LFS pull failed");
  if(IsPointer(destination,L"巨大.SLDPRT")||std::filesystem::file_size(clone/L"巨大.SLDPRT")!=content.size())std::wcerr<<L"LFS clone result:\n"<<cloned.command<<L"\n"<<cloned.output<<L"\n"<<cloned.error<<L"\n";
  Require(!IsPointer(destination,L"巨大.SLDPRT")&&std::filesystem::file_size(clone/L"巨大.SLDPRT")==content.size(),"clone left an LFS pointer");
  const auto pointer=clone/L"pointer.txt";Require(rc::RunProcessToFile(argv[1],{L"show",L"HEAD:巨大.SLDPRT"},clone,pointer).exitCode==0,"could not read stored pointer");
  std::filesystem::copy_file(pointer,clone/L"巨大.SLDPRT",std::filesystem::copy_options::overwrite_existing);Require(IsPointer(destination,L"巨大.SLDPRT"),"pointer fixture missing");
  Require(git.LfsPull(destination,{L"巨大.SLDPRT"}).exitCode==0,"manual LFS retrieval failed");Require(!IsPointer(destination,L"巨大.SLDPRT")&&std::filesystem::file_size(clone/L"巨大.SLDPRT")==content.size(),"manual retrieval did not materialize LFS file");
  std::filesystem::copy_file(pointer,clone/L"巨大.SLDPRT",std::filesystem::copy_options::overwrite_existing);Require(IsPointer(destination,L"巨大.SLDPRT"),"second pointer fixture missing");
  Require(git.Pull(destination).exitCode==0,"up-to-date pull plus LFS repair failed");Require(!IsPointer(destination,L"巨大.SLDPRT")&&std::filesystem::file_size(clone/L"巨大.SLDPRT")==content.size(),"pull did not repair LFS pointer");
  std::cout<<"PASS: clone, explicit retrieval, and an up-to-date update materialize LFS CAD content; pointers are detectable.\n";return 0;
 }catch(const std::exception& error){std::cerr<<error.what()<<"\n";return 1;}
}
