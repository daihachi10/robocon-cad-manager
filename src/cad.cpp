#include "cad.h"
#include "process_runner.h"
namespace rc { namespace { bool Managed(const std::wstring&p){for(auto&s:{L".sldprt",L".sldasm",L".slddrw",L".stl",L".step",L".stp",L".dxf",L".dwg",L".pdf"})if(IEndsWith(p,s))return true;return false;} }
bool MakeCadWritable(const std::filesystem::path& path,std::wstring& error) {
 const auto attributes=GetFileAttributesW(path.c_str());
 if(attributes==INVALID_FILE_ATTRIBUTES){error=L"ファイルの属性を確認できません。\n"+LastError();return false;}
 if((attributes&FILE_ATTRIBUTE_READONLY)==0)return true;
 const auto writable=attributes&~FILE_ATTRIBUTE_READONLY;
 if(!SetFileAttributesW(path.c_str(),writable?writable:FILE_ATTRIBUTE_NORMAL)){
  error=L"保存できるようにファイルの読み取り専用を解除できません。\n"+LastError();return false;
 }
 return true;
}
std::vector<CadFile> ScanCad(const Repository&r){
 std::vector<CadFile> files;std::error_code error;
 std::filesystem::recursive_directory_iterator it(r.folder,std::filesystem::directory_options::skip_permission_denied,error),end;
 if(error)return files;
 while(it!=end){
  const auto path=it->path();const auto name=path.filename().wstring();
  const bool directory=it->is_directory(error);
  if(error){error.clear();it.increment(error);continue;}
  const auto attributes=GetFileAttributesW(path.c_str());
  const bool excluded=name==L".git"||name.starts_with(L"~$")||
   (attributes!=INVALID_FILE_ATTRIBUTES&&(attributes&FILE_ATTRIBUTE_REPARSE_POINT));
  if(excluded){if(directory)it.disable_recursion_pending();}
  else if(directory){CadFile entry;entry.path=path;entry.directory=true;files.push_back(std::move(entry));}
  else if(it->is_regular_file(error)&&!error){
   CadFile file;file.path=path;file.bytes=it->file_size(error);if(error){file.bytes=0;error.clear();}
   if(Managed(path.wstring())){
    file.suspiciousName=name.find(L"最終")!=std::wstring::npos;
    std::ifstream input(path,std::ios::binary);std::string head(80,'\0');input.read(head.data(),static_cast<std::streamsize>(head.size()));head.resize(static_cast<size_t>(input.gcount()));
    file.lfsPointer=head.starts_with("version https://git-lfs.github.com/spec/");
   }
   files.push_back(std::move(file));
  }
  error.clear();it.increment(error);if(error)error.clear();
 }
 return files;
}
bool OpenFile(const std::filesystem::path&p,std::wstring&e){auto h=ShellExecuteW(nullptr,L"open",p.c_str(),nullptr,p.parent_path().c_str(),SW_SHOWNORMAL);if((INT_PTR)h<=32){e=LastError((DWORD)(INT_PTR)h);return false;}return true;}
std::vector<std::wstring>ConflictingFiles(const std::filesystem::path&r){auto x=RunProcess(L"git",{L"diff",L"--name-only",L"--diff-filter=U"},r);std::vector<std::wstring>v;std::wistringstream s(x.output);for(std::wstring l;std::getline(s,l);)if(!Trim(l).empty()&&!std::filesystem::path(l).filename().wstring().starts_with(L"~$"))v.push_back(Trim(l));return v;}}
