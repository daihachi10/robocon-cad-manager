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
std::vector<CadFile>ScanCad(const Repository&r){std::vector<CadFile>v;std::error_code e;std::filesystem::recursive_directory_iterator i(r.folder,std::filesystem::directory_options::skip_permission_denied,e),end;if(e)return v;while(i!=end){const auto path=i->path();const bool directory=i->is_directory(e);if(e){e.clear();i.increment(e);continue;}if(directory&&path.filename()==L".git"){i.disable_recursion_pending();i.increment(e);continue;}const bool regular=i->is_regular_file(e);if(e){e.clear();i.increment(e);continue;}const auto name=path.filename().wstring();if(regular&&!name.starts_with(L"~$")&&Managed(path.wstring())){CadFile f;f.path=path;f.bytes=i->file_size(e);if(e){f.bytes=0;e.clear();}f.suspiciousName=name.find(L"最終")!=std::wstring::npos;std::ifstream in(f.path,std::ios::binary);std::string s(80,'\0');in.read(s.data(),static_cast<std::streamsize>(s.size()));s.resize(static_cast<size_t>(in.gcount()));f.lfsPointer=s.starts_with("version https://git-lfs.github.com/spec/");v.push_back(std::move(f));}i.increment(e);if(e)e.clear();}return v;}bool OpenFile(const std::filesystem::path&p,std::wstring&e){auto h=ShellExecuteW(nullptr,L"open",p.c_str(),nullptr,p.parent_path().c_str(),SW_SHOWNORMAL);if((INT_PTR)h<=32){e=LastError((DWORD)(INT_PTR)h);return false;}return true;}std::vector<std::wstring>ConflictingFiles(const std::filesystem::path&r){auto x=RunProcess(L"git",{L"diff",L"--name-only",L"--diff-filter=U"},r);std::vector<std::wstring>v;std::wistringstream s(x.output);for(std::wstring l;std::getline(s,l);)if(!Trim(l).empty()&&!std::filesystem::path(l).filename().wstring().starts_with(L"~$"))v.push_back(Trim(l));return v;}}
