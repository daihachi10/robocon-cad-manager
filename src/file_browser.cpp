#include "file_browser.h"

namespace rc {
bool NormalizeBrowserPath(const std::wstring& value,std::wstring& normalized){
 std::wstring slash=value;std::replace(slash.begin(),slash.end(),L'\\',L'/');
 const auto path=std::filesystem::path(slash).lexically_normal();
 if(path.has_root_path())return false;
 normalized=path.generic_wstring();
 if(normalized==L".")normalized.clear();
 while(!normalized.empty()&&normalized.back()==L'/')normalized.pop_back();
 return normalized!=L".."&&!normalized.starts_with(L"../");
}
bool IsTemporaryCadPath(const std::wstring& path){
 for(const auto& part:std::filesystem::path(path))if(part.wstring().starts_with(L"~$"))return true;
 return false;
}
bool MatchesCadFilter(const std::wstring& path,int mode){
 if(mode==1)return IEndsWith(path,L".sldprt");
 if(mode==2)return IEndsWith(path,L".sldasm");
 if(mode==3)return IEndsWith(path,L".slddrw");
 return true;
}

std::vector<BrowserItem> BuildFolderItems(const Repository& repository,const std::wstring& folder,
 const std::vector<CadFile>& files,const std::vector<GitStatus>& changes,int filterMode){
 std::wstring current;if(!NormalizeBrowserPath(folder,current))return{};
 const auto prefix=current.empty()?L"":current+L"/";
 std::map<std::wstring,BrowserItem> items;
 auto itemFor=[&](const std::wstring& path,bool directory)->BrowserItem*{
  if(IsTemporaryCadPath(path)||path==L".git"||path.starts_with(L".git/"))return nullptr;
  if(!path.starts_with(prefix)||path.size()<=prefix.size())return nullptr;
  const auto remainder=path.substr(prefix.size());const auto separator=remainder.find(L'/');
  const bool isFolder=separator!=std::wstring::npos||directory;
  if(!isFolder&&!MatchesCadFilter(path,filterMode))return nullptr;
  const auto name=separator==std::wstring::npos?remainder:remainder.substr(0,separator);
  const auto child=prefix+name;
  auto& item=items[child];item.path=child;item.name=name;item.directory=item.directory||isFolder;
  return &item;
 };
 for(const auto& file:files){
  std::wstring path;
  if(!NormalizeBrowserPath(file.path.lexically_relative(repository.folder).generic_wstring(),path))continue;
  if(auto* item=itemFor(path,file.directory)){
   if(path==item->path){item->local=true;item->bytes=file.bytes;item->lfsPointer=file.lfsPointer;item->suspiciousName=file.suspiciousName;}
  }
 }
 for(const auto& change:changes){
  std::wstring path;if(!NormalizeBrowserPath(change.path,path))continue;
  if(auto* item=itemFor(path,false)){
   item->changedPaths.push_back(change.path);
   if(!item->directory)item->changeCode=change.code;
  }
 }
 std::vector<BrowserItem> result;
 for(auto& [path,item]:items)result.push_back(std::move(item));
 std::sort(result.begin(),result.end(),[](const BrowserItem& a,const BrowserItem& b){
  if(a.directory!=b.directory)return a.directory;
  const auto order=StrCmpLogicalW(a.name.c_str(),b.name.c_str());return order==0?a.path<b.path:order<0;
 });
 return result;
}

std::map<std::wstring,bool> UpdatedChangeChecks(const std::vector<GitStatus>& changes,const std::map<std::wstring,bool>& previous){
 std::map<std::wstring,bool> result;
 for(const auto& change:changes)if(!IsTemporaryCadPath(change.path)){
  const auto old=previous.find(change.path);result[change.path]=old==previous.end()||old->second;
 }
 return result;
}
bool FolderNavigation::Navigate(const std::wstring& folder){
 std::wstring path;if(!NormalizeBrowserPath(folder,path)||path==Current())return false;
 history_.resize(index_+1);history_.push_back(std::move(path));++index_;return true;
}
bool FolderNavigation::Back(){if(!CanBack())return false;--index_;return true;}
bool FolderNavigation::Forward(){if(!CanForward())return false;++index_;return true;}
bool FolderNavigation::Up(){if(Current().empty())return false;return Navigate(std::filesystem::path(Current()).parent_path().generic_wstring());}
}
