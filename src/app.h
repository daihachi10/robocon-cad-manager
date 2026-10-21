#pragma once
#include "cad.h"
#include "git.h"
#include "solidworks_monitor.h"
#include "update.h"
#include "file_browser.h"
#include "file_transfer.h"
#include "ui.h"
namespace rc {
constexpr UINT WM_APP_DONE=WM_APP+41, WM_APP_SW_ATTACH=WM_APP+42, WM_APP_UPDATE=WM_APP+44;
constexpr WORD kBrowseBack=114,kBrowseForward=115,kBrowseUp=116,kOpenExplorer=117;
constexpr WORD kContextNewPart=400,kContextNewAssembly=401,kContextNewDrawing=402,kContextNewFolder=403,kContextPaste=404,kContextOpen=405,kContextRename=406,kContextDelete=407,kContextCopy=408,kContextHistory=409,kContextRestore=410,kContextDeleteFolder=411;
struct UiTask { std::function<void()> action; };
class App {
 friend struct AppBrowserTest;
 friend struct IntegrationFixTest;
 HINSTANCE instance_{}; HWND window_{},list_{},status_{},filter_{}; HIMAGELIST images_{}; HFONT font_{},strikeFont_{};
 ConfigStore config_; GitClient git_; SolidWorksMonitor solidworks_; int selected_{-1},filterMode_{};
 std::vector<CadFile> cad_; std::vector<GitStatus> changes_; std::map<std::wstring,LastCommit> latestCommits_; std::map<std::wstring,LockInfo> locks_; std::set<std::wstring> opened_,pendingUnlock_; std::map<std::wstring,bool> changeChecks_; std::vector<std::wstring> listPaths_; std::map<std::wstring,std::filesystem::path> localFiles_; bool locksKnown_{},populating_{}; std::mutex lockWorkMutex_; HANDLE localChangeStop_{}; HANDLE localChangeNotify_{}; std::thread localChangeThread_; std::atomic<bool> localRefreshPosted_{false};
 FolderNavigation folderNavigation_;std::vector<BrowserItem> browserItems_;HWND folderLabel_{};
 FileDropTarget* dropTarget_{};
 std::set<std::wstring> locking_,closing_;bool syncingSolidWorks_{};
 std::wstring browsedRepository_;bool localRefreshRunning_{},localRefreshAgain_{};uint64_t repositoryGeneration_{};
 std::unique_ptr<OperationDialog> operation_;
 public:
 void CreateBrowserControls();void LayoutBrowser(UINT dpi=0);void UpdateBrowserImages(UINT dpi=0);void UpdateNavigationBar();void NavigateFolder(const std::wstring&);void UpdateVisibleCheckStates();LRESULT HandleListNotification(LPARAM);
 void ContinueWithForeignLock(const std::wstring& path,const std::filesystem::path& localPath,const std::wstring& owner,bool openAfter);
 bool SyncSolidWorksOpenState();
 void ShowListContextMenu(POINT screen); void CopySelectedFile(); void PasteFromClipboard(const std::vector<std::filesystem::path>& files); void PasteFilesToFolder(const std::vector<std::filesystem::path>&,const std::wstring&); void HandleDroppedFiles(IDataObject*,POINTL); DWORD DropEffect(POINTL) const; void CreateNewItem(int kind); void RenameSelected(); void DeleteSelectedFolder(); void ShowHistory(const std::wstring&,bool);
 bool BeginOperation(const std::wstring& title,const std::wstring& stage,const std::wstring& command);void CompleteOperation();void FailOperation(const std::wstring& summary,const std::wstring& command,const std::wstring& details);
 int Run(HINSTANCE); LRESULT WndProc(HWND,UINT,WPARAM,LPARAM); void Refresh(); void RefreshLocal(); void RefreshLocks(); void StartLocalWatcher(); void StopLocalWatcher(); void ShowSetup(); void HandleCommand(WORD); void DeleteSelectedFile(); void Background(std::function<void()>); Repository* Current(); const Repository* Current()const; void SetStatus(const std::wstring&); void Populate(); void UpdateFonts(UINT); void HandleSolidWorksEvent(const SolidWorksEvent&); void LockPath(const std::wstring&,bool=false); void ClosedPath(const std::wstring&); void ReleasePendingLocks(); void CheckForUpdates(bool manual);
};
}
