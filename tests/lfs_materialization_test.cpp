#include "cad.h"
#include "git.h"
#include <iostream>
#include <stdexcept>

namespace {
void Require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
void Write(const std::filesystem::path& path,const std::string& value){std::ofstream file(path,std::ios::binary);file<<value;Require(bool(file),"write failed");}
bool IsPointer(const rc::Repository& repo,const std::wstring& relative){std::ifstream input(std::filesystem::path(repo.folder)/relative,std::ios::binary);std::string head(80,'\0');input.read(head.data(),static_cast<std::streamsize>(head.size()));head.resize(static_cast<size_t>(input.gcount()));return head.starts_with("version https://git-lfs.github.com/spec/");}
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
  const auto listed=run(clone,{L"lfs",L"ls-files",L"-l"});const auto separator=listed.output.find(L' ');Require(separator==64,"could not find LFS object ID");
  const auto pointer=root/L"pointer.txt";Write(pointer,"version https://git-lfs.github.com/spec/v1\noid sha256:"+rc::Utf8(listed.output.substr(0,64))+"\nsize "+std::to_string(content.size())+"\n");
  std::filesystem::copy_file(pointer,clone/L"巨大.SLDPRT",std::filesystem::copy_options::overwrite_existing);Require(IsPointer(destination,L"巨大.SLDPRT"),"pointer fixture missing");
  Require(git.LfsPull(destination,{L"巨大.SLDPRT"}).exitCode==0,"manual LFS retrieval failed");Require(!IsPointer(destination,L"巨大.SLDPRT")&&std::filesystem::file_size(clone/L"巨大.SLDPRT")==content.size(),"manual retrieval did not materialize LFS file");
  std::filesystem::copy_file(pointer,clone/L"巨大.SLDPRT",std::filesystem::copy_options::overwrite_existing);Require(IsPointer(destination,L"巨大.SLDPRT"),"second pointer fixture missing");
  Require(git.Pull(destination).exitCode==0,"up-to-date pull plus LFS repair failed");Require(!IsPointer(destination,L"巨大.SLDPRT")&&std::filesystem::file_size(clone/L"巨大.SLDPRT")==content.size(),"pull did not repair LFS pointer");
  // Older repositories may have LFS pointer files but no .gitattributes. Git LFS
  // fetches the object, yet its standard checkout then leaves the pointer unchanged.
  // The app must safely materialize the verified local object in that case.
  std::filesystem::copy_file(pointer,clone/L"巨大.SLDPRT",std::filesystem::copy_options::overwrite_existing);std::filesystem::remove(clone/L".gitattributes");
  Require(IsPointer(destination,L"巨大.SLDPRT"),"attribute-less pointer fixture missing");
  Require(git.LfsPull(destination,{L"巨大.SLDPRT"}).exitCode==0,"attribute-less LFS retrieval failed");
  Require(!IsPointer(destination,L"巨大.SLDPRT")&&std::filesystem::file_size(clone/L"巨大.SLDPRT")==content.size(),"attribute-less LFS retrieval did not materialize the CAD file");
  Require(!git.IsDirty(destination,L"巨大.SLDPRT"),"attribute-less materialization left the CAD file modified");
  std::cout<<"PASS: clone, explicit retrieval, an up-to-date update, and an attribute-less LFS pointer materialize CAD content.\n";return 0;
 }catch(const std::exception& error){std::cerr<<error.what()<<"\n";return 1;}
}
