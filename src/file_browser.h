#pragma once
#include "cad.h"
#include "git.h"

namespace rc {
enum CadFilterMode { SolidWorksFiles=0, AllFiles=1, PartsOnly=2, AssembliesOnly=3, DrawingsOnly=4 };
struct BrowserItem {
 std::wstring path,name,changeCode;
 bool directory{},local{},lfsPointer{},suspiciousName{};
 uintmax_t bytes{};
 std::vector<std::wstring> changedPaths;
};

bool NormalizeBrowserPath(const std::wstring& value,std::wstring& normalized);
bool IsTemporaryCadPath(const std::wstring& path);
bool MatchesCadFilter(const std::wstring& path,int mode);
std::vector<BrowserItem> BuildFolderItems(const Repository& repository,const std::wstring& folder,
 const std::vector<CadFile>& files,const std::vector<GitStatus>& changes,int filterMode);
// A Git LFS pointer is not a user edit.  Repositories missing .gitattributes can
// otherwise report every unfetched pointer as modified after one file is fetched.
void ExcludeUnfetchedLfsPointerChanges(const Repository& repository,const std::vector<CadFile>& files,std::vector<GitStatus>& changes);
std::map<std::wstring,bool> UpdatedChangeChecks(const std::vector<GitStatus>& changes,const std::map<std::wstring,bool>& previous);

class FolderNavigation {
 std::vector<std::wstring> history_{L""};
 size_t index_{};
 public:
 const std::wstring& Current()const{return history_[index_];}
 bool CanBack()const{return index_>0;}
 bool CanForward()const{return index_+1<history_.size();}
 void Reset(){history_={L""};index_=0;}
 bool Navigate(const std::wstring& folder);
 bool Back();bool Forward();bool Up();
};
}
