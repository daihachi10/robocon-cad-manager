#include "app.h"
#include "ui.h"
#include "resource.h"
namespace rc {
namespace {
constexpr WORD kSetup=100,kRefresh=101,kClone=102,kPull=104,kCommit=105,kOpen=106,kMore=107,kRefreshLocal=108,kFilter=109,kLfsSetup=110,kLocks=111,kUnlock=112,kUpdate=113;
constexpr UINT kLocalChangedMessage=WM_APP+43;
const wchar_t* kClass=L"RoboconCadManager.Main";
// This program has a single top-level window.  Do not use GWLP_USERDATA while
// WM_NCCREATE is being processed: DefWindowProc may initialize that slot.
// Keeping the pointer here makes WM_CREATE reach App::WndProc reliably.
App* gApp{};
LRESULT CALLBACK Procedure(HWND h,UINT msg,WPARAM w,LPARAM l){
 if(msg==WM_NCCREATE){DefWindowProcW(h,msg,w,l);return TRUE;}
 if(gApp)return gApp->WndProc(h,msg,w,l);
 return DefWindowProcW(h,msg,w,l);
}



std::wstring RelativePath(const Repository& repository,const std::filesystem::path& path){std::error_code error;auto relative=std::filesystem::relative(path,repository.folder,error);return error?path.filename().wstring():relative.generic_wstring();}
std::wstring RepositoryDocumentPath(const Repository& repository,const std::wstring& absolute){
 auto root=std::filesystem::path(repository.folder).lexically_normal().generic_wstring();while(!root.empty()&&root.back()==L'/')root.pop_back();root+=L'/';
 const auto path=std::filesystem::path(absolute).lexically_normal().generic_wstring();
 if(path.size()<=root.size()||CompareStringOrdinal(path.c_str(),static_cast<int>(root.size()),root.c_str(),static_cast<int>(root.size()),TRUE)!=CSTR_EQUAL)return{};
 std::wstring relative;return NormalizeBrowserPath(path.substr(root.size()),relative)?relative:std::wstring{};
}

bool IsLockableCad(const std::wstring& path){return IEndsWith(path,L".sldprt")||IEndsWith(path,L".sldasm")||IEndsWith(path,L".slddrw");}
HICON CreateApplicationIcon(int size){return static_cast<HICON>(LoadImageW(GetModuleHandleW(nullptr),MAKEINTRESOURCEW(IDI_APPLICATION_ICON),IMAGE_ICON,size,size,LR_DEFAULTCOLOR));}
HICON CreateLockIcon(int size,bool person){BITMAPV5HEADER header{};header.bV5Size=sizeof(header);header.bV5Width=size;header.bV5Height=-size;header.bV5Planes=1;header.bV5BitCount=32;header.bV5Compression=BI_RGB;void* pixels{};HDC dc=GetDC(nullptr);HBITMAP color=CreateDIBSection(dc,reinterpret_cast<BITMAPINFO*>(&header),DIB_RGB_COLORS,&pixels,nullptr,0);ReleaseDC(nullptr,dc);if(!color||!pixels)return nullptr;auto* data=static_cast<DWORD*>(pixels);std::fill(data,data+size*size,0u);const DWORD green=0xFF5AA778u,red=0xFF2333EAu;auto box=[&](int x,int y,int w,int h,DWORD c){for(int yy=y;yy<y+h&&yy<size;++yy)for(int xx=x;xx<x+w&&xx<size;++xx)if(xx>=0&&yy>=0)data[yy*size+xx]=c;};const int b=size/5;box(b,size/2,size-2*b,size/2,person?red:green);box(size/2-b/2,b,b,size/2-b,person?red:green);if(person){box(size*2/3,size/4,size/5,size/5,red);box(size*3/5,size*2/5,size/3,size/5,red);}HBITMAP mask=CreateBitmap(size,size,1,1,nullptr);ICONINFO info{};info.fIcon=TRUE;info.hbmColor=color;info.hbmMask=mask;auto icon=CreateIconIndirect(&info);DeleteObject(mask);DeleteObject(color);return icon;}
}
int App::Run(HINSTANCE instance){
 instance_=instance;const auto oleResult=OleInitialize(nullptr);if(FAILED(oleResult)){MessageBoxW(nullptr,L"ファイルのドラッグ＆ドロップを初期化できません。",L"起動できません",MB_ICONERROR);return static_cast<int>(oleResult);} SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
 INITCOMMONCONTROLSEX controls{sizeof(controls),ICC_LISTVIEW_CLASSES|ICC_BAR_CLASSES}; InitCommonControlsEx(&controls);
 HICON largeIcon=CreateApplicationIcon(32),smallIcon=CreateApplicationIcon(16);WNDCLASSEXW cls{sizeof(cls)};cls.hInstance=instance_;cls.lpszClassName=kClass;cls.lpfnWndProc=Procedure;cls.hCursor=LoadCursorW(nullptr,IDC_ARROW);cls.hIcon=largeIcon;cls.hIconSm=smallIcon;cls.hbrBackground=(HBRUSH)(COLOR_WINDOW+1);RegisterClassExW(&cls);
 gApp=this;
 window_=CreateWindowExW(0,kClass,L"高専ロボコン CAD 管理",WS_OVERLAPPEDWINDOW,CW_USEDEFAULT,CW_USEDEFAULT,1120,700,nullptr,nullptr,instance_,nullptr);
 if(!window_){auto code=GetLastError();auto error=LastError(code);auto title=L"メインウィンドウの作成に失敗しました ("+std::to_wstring(code)+L")";MessageBoxW(nullptr,error.c_str(),title.c_str(),MB_ICONERROR);OleUninitialize();return(int)code;} ShowWindow(window_,SW_SHOWNORMAL);UpdateWindow(window_);
 MSG message;while(GetMessageW(&message,nullptr,0,0)){TranslateMessage(&message);DispatchMessageW(&message);}DestroyIcon(largeIcon);DestroyIcon(smallIcon);solidworks_.Stop();OleUninitialize();gApp=nullptr;return(int)message.wParam;
}
LRESULT App::WndProc(HWND h,UINT msg,WPARAM w,LPARAM l){
 if(!window_)window_=h;
 switch(msg){
 case WM_CREATE:{
  CreateBrowserControls();
  dropTarget_=new FileDropTarget(list_,[this](POINTL p){return DropEffect(p);},[this](IDataObject* o,POINTL p){HandleDroppedFiles(o,p);});const auto dropResult=RegisterDragDrop(list_,dropTarget_);if(FAILED(dropResult)){dropTarget_->Release();dropTarget_=nullptr;const auto error=L"ファイルのドロップ受付を登録できません。\n"+LastError(static_cast<DWORD>(dropResult));MessageBoxW(window_,error.c_str(),L"ドラッグ＆ドロップ",MB_ICONWARNING);}
  std::wstring error;config_.Load(error);if(!error.empty())MessageBoxW(window_,error.c_str(),L"設定",MB_ICONWARNING);
  if(!config_.Repositories().empty()){selected_=0;StartLocalWatcher();}
  solidworks_.Start(window_,WM_APP_SW_ATTACH,[this](const rc::SolidWorksEvent& event){HandleSolidWorksEvent(event);});
  if(Current())Refresh();else Populate();PostMessageW(window_,WM_APP_UPDATE,0,0);return 0;
 }
 case WM_SIZE:LayoutBrowser();return 0;
 case WM_SYSKEYDOWN:if(w==VK_LEFT){HandleCommand(kBrowseBack);return 0;}if(w==VK_RIGHT){HandleCommand(kBrowseForward);return 0;}if(w==VK_UP){HandleCommand(kBrowseUp);return 0;}break;
 case WM_GETMINMAXINFO:{auto* limits=reinterpret_cast<MINMAXINFO*>(l);const auto dpi=GetDpiForWindow(window_);limits->ptMinTrackSize={MulDiv(640,dpi,96),MulDiv(320,dpi,96)};return 0;}
 case WM_COMMAND:if(LOWORD(w)==kFilter&&HIWORD(w)==CBN_SELCHANGE){filterMode_=static_cast<int>(SendMessageW(filter_,CB_GETCURSEL,0,0));Populate();return 0;}HandleCommand(LOWORD(w));return 0;
 case WM_ACTIVATE:if(LOWORD(w)!=WA_INACTIVE&&Current())RefreshLocal();return 0;
 case kLocalChangedMessage:localRefreshPosted_.store(false);if(Current())RefreshLocal();return 0;
 case WM_APP_DONE:{auto task=(UiTask*)l;task->action();delete task;return 0;} case WM_APP_SW_ATTACH:solidworks_.Attach();SyncSolidWorksOpenState();return 0;
 case WM_APP_UPDATE:CheckForUpdates(false);return 0;
 case WM_NOTIFY:return HandleListNotification(l);
 case WM_DPICHANGED:{auto rect=(RECT*)l;SetWindowPos(window_,nullptr,rect->left,rect->top,rect->right-rect->left,rect->bottom-rect->top,SWP_NOZORDER|SWP_NOACTIVATE);UpdateFonts(GetDpiForWindow(window_));return 0;}
 case WM_DESTROY:operation_.reset();StopLocalWatcher();solidworks_.Stop();if(dropTarget_){RevokeDragDrop(list_);dropTarget_->Release();dropTarget_=nullptr;}if(images_)ImageList_Destroy(images_);if(font_)DeleteObject(font_);if(strikeFont_)DeleteObject(strikeFont_);PostQuitMessage(0);return 0;
 }return DefWindowProcW(h,msg,w,l);
}
Repository* App::Current(){return selected_>=0&&selected_<(int)config_.Repositories().size()?const_cast<Repository*>(&config_.Repositories()[selected_]):nullptr;}
const Repository* App::Current()const{return selected_>=0&&selected_<(int)config_.Repositories().size()?&config_.Repositories()[selected_]:nullptr;}
void App::SetStatus(const std::wstring& text){SetWindowTextW(status_,text.c_str());}
bool App::BeginOperation(const std::wstring& title,const std::wstring& stage,const std::wstring& command){if(operation_&&operation_->Visible()){SetForegroundWindow(window_);return false;}operation_=std::make_unique<OperationDialog>();if(!operation_->Show(window_,title,stage,command)){operation_.reset();MessageBoxW(window_,L"処理状況の画面を表示できません。",L"処理を開始できません",MB_ICONERROR);return false;}return true;}
void App::PostOperationStep(const std::wstring& stage,const std::wstring& command,int completed,int total,const std::wstring& details){auto* task=new UiTask{[this,stage,command,completed,total,details]{if(operation_&&operation_->Visible())operation_->Step(stage,command,completed,total,details);}};if(!PostMessageW(window_,WM_APP_DONE,0,reinterpret_cast<LPARAM>(task)))delete task;}
void App::CompleteOperation(){if(operation_)operation_->Close();}
void App::FailOperation(const std::wstring& summary,const std::wstring& command,const std::wstring& details){if(operation_&&operation_->Visible())operation_->Fail(summary,command,details);else MessageBoxW(window_,details.c_str(),L"処理に失敗しました",MB_ICONERROR);}
void App::DeleteSelectedFile(){SyncSolidWorksOpenState();const int selected=ListView_GetNextItem(list_,-1,LVNI_SELECTED);if(selected<0||selected>=static_cast<int>(listPaths_.size())){MessageBoxW(window_,L"削除するファイルを選択してください。",L"ファイルを選択してください",MB_ICONINFORMATION);return;}if(selected<static_cast<int>(browserItems_.size())&&browserItems_[selected].directory){MessageBoxW(window_,L"フォルダの中へ移動し、削除するファイルを選択してください。",L"ファイルを選択してください",MB_ICONINFORMATION);return;}const auto path=listPaths_[selected];auto local=localFiles_.find(path);if(local==localFiles_.end()){MessageBoxW(window_,L"この項目には削除できるローカルファイルがありません。",L"ファイルを削除できません",MB_ICONINFORMATION);return;}if(opened_.contains(path)){MessageBoxW(window_,L"SolidWorksで開いているファイルは削除できません。先に閉じてください。",L"ファイルを削除できません",MB_ICONWARNING);return;}const auto message=L"次のファイルをローカルから削除します。\n\n"+path+L"\n\n削除後は保存して送信するまでGitHubへは反映されません。続けますか？";if(MessageBoxW(window_,message.c_str(),L"ファイルを削除",MB_YESNO|MB_ICONWARNING)!=IDYES)return;if(!DeleteFileW(local->second.c_str())){const auto error=LastError();MessageBoxW(window_,error.c_str(),L"ファイルを削除できません",MB_ICONERROR);return;}SetStatus(L"ファイルを削除しました。保存して送信すると共有されます。");RefreshLocal();}
void App::StartLocalWatcher(){
 StopLocalWatcher();++repositoryGeneration_;
 const auto* current=Current();const auto folderPath=current?current->folder:std::wstring{};
 if(folderPath!=browsedRepository_){
  browsedRepository_=folderPath;folderNavigation_.Reset();opened_.clear();locking_.clear();closing_.clear();pendingUnlock_.clear();cad_.clear();changes_.clear();changeChecks_.clear();latestCommits_.clear();locks_.clear();locksKnown_=false;Populate();
 }
auto* repository=Current();if(!repository||repository->folder.empty())return;localChangeStop_=CreateEventW(nullptr,TRUE,FALSE,nullptr);if(!localChangeStop_)return;const auto folder=repository->folder;localRefreshPosted_=false;localChangeThread_=std::thread([this,folder]{const HANDLE directory=CreateFileW(folder.c_str(),FILE_LIST_DIRECTORY,FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,nullptr,OPEN_EXISTING,FILE_FLAG_BACKUP_SEMANTICS|FILE_FLAG_OVERLAPPED,nullptr);if(directory==INVALID_HANDLE_VALUE)return;const HANDLE completion=CreateEventW(nullptr,TRUE,FALSE,nullptr);if(!completion){CloseHandle(directory);return;}std::vector<BYTE> buffer(64*1024);OVERLAPPED overlapped{};overlapped.hEvent=completion;const HANDLE handles[]={localChangeStop_,completion};for(;;){ResetEvent(completion);DWORD ignored{};if(!ReadDirectoryChangesW(directory,buffer.data(),static_cast<DWORD>(buffer.size()),TRUE,FILE_NOTIFY_CHANGE_FILE_NAME|FILE_NOTIFY_CHANGE_DIR_NAME|FILE_NOTIFY_CHANGE_LAST_WRITE|FILE_NOTIFY_CHANGE_SIZE,&ignored,&overlapped,nullptr)&&GetLastError()!=ERROR_IO_PENDING)break;const auto result=WaitForMultipleObjects(2,handles,FALSE,INFINITE);if(result==WAIT_OBJECT_0){CancelIoEx(directory,&overlapped);DWORD cancelled{};GetOverlappedResult(directory,&overlapped,&cancelled,TRUE);break;}if(result!=WAIT_OBJECT_0+1)break;DWORD bytes{};if(!GetOverlappedResult(directory,&overlapped,&bytes,FALSE))continue;bool relevant=bytes==0;for(DWORD offset=0;offset<bytes;){auto* entry=reinterpret_cast<FILE_NOTIFY_INFORMATION*>(buffer.data()+offset);std::wstring name(entry->FileName,entry->FileNameLength/sizeof(wchar_t));if(name!=L".git"&&!name.starts_with(L".git\\")&&!std::filesystem::path(name).filename().wstring().starts_with(L"~$")){relevant=true;}if(entry->NextEntryOffset==0)break;offset+=entry->NextEntryOffset;}if(relevant&&!localRefreshPosted_.exchange(true))PostMessageW(window_,kLocalChangedMessage,0,0);}CloseHandle(completion);CloseHandle(directory);});}
void App::StopLocalWatcher(){if(localChangeStop_)SetEvent(localChangeStop_);if(localChangeThread_.joinable())localChangeThread_.join();if(localChangeNotify_&&localChangeNotify_!=INVALID_HANDLE_VALUE)FindCloseChangeNotification(localChangeNotify_);if(localChangeStop_)CloseHandle(localChangeStop_);localChangeNotify_=nullptr;localChangeStop_=nullptr;localRefreshPosted_=false;}
void App::UpdateFonts(UINT dpi){
 if(font_)DeleteObject(font_);if(strikeFont_)DeleteObject(strikeFont_);
 font_=CreateFontW(-MulDiv(10,static_cast<int>(dpi),72),0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY,DEFAULT_PITCH|FF_DONTCARE,L"Yu Gothic UI");
 LOGFONTW strike{};GetObjectW(font_,sizeof(strike),&strike);strike.lfStrikeOut=TRUE;strikeFont_=CreateFontIndirectW(&strike);
 EnumChildWindows(window_,[](HWND child,LPARAM font){SendMessageW(child,WM_SETFONT,font,TRUE);return TRUE;},reinterpret_cast<LPARAM>(font_));
 const int widths[]={300,170,190,310,90};for(int i=0;i<5;++i)ListView_SetColumnWidth(list_,i,MulDiv(widths[i],dpi,96));
 UpdateBrowserImages(dpi);LayoutBrowser(dpi);
}
void App::UpdateBrowserImages(UINT dpi){
 if(!dpi)dpi=GetDpiForWindow(window_);
 const int size=MulDiv(24,dpi,96);
 auto images=ImageList_Create(size,size,ILC_COLOR32|ILC_MASK,3,0);
 if(!images)return;
 for(bool person:{false,true}){auto icon=CreateLockIcon(size,person);ImageList_AddIcon(images,icon);DestroyIcon(icon);}
 SHSTOCKICONINFO folder{sizeof(folder)};
 if(SUCCEEDED(SHGetStockIconInfo(SIID_FOLDER,SHGSI_ICON|SHGSI_SMALLICON,&folder))){ImageList_AddIcon(images,folder.hIcon);DestroyIcon(folder.hIcon);}
 const auto old=ListView_SetImageList(list_,images,LVSIL_SMALL);images_=images;if(old)ImageList_Destroy(old);
}
void App::Background(std::function<void()> task){std::thread([this,task=std::move(task)]()mutable{try{task();}catch(const std::exception& ex){auto message=Wide(ex.what());if(message.empty())message=L"バックグラウンド処理でエラーが発生しました。";auto*done=new UiTask{[this,message]{SetStatus(message);MessageBoxW(window_,message.c_str(),L"処理エラー",MB_ICONERROR);}};if(!PostMessageW(window_,WM_APP_DONE,0,reinterpret_cast<LPARAM>(done)))delete done;}catch(...){auto*done=new UiTask{[this]{const auto message=L"バックグラウンド処理で不明なエラーが発生しました。";SetStatus(message);MessageBoxW(window_,message,L"処理エラー",MB_ICONERROR);}};if(!PostMessageW(window_,WM_APP_DONE,0,reinterpret_cast<LPARAM>(done)))delete done;}}).detach();}
void App::Refresh(){locksKnown_=false;RefreshLocal();if(Current()&&Current()->lfsLockEnabled)RefreshLocks();else{locks_.clear();pendingUnlock_.clear();Populate();}}
void App::RefreshLocal(){
 SyncSolidWorksOpenState();
 if(!Current())return;
 if(localRefreshRunning_){localRefreshAgain_=true;return;}
 localRefreshRunning_=true;localRefreshAgain_=false;
 SetStatus(L"ローカルの変更を更新しています。");
 Background([this,r=*Current(),generation=repositoryGeneration_]{
  std::vector<CadFile> files;std::vector<GitStatus> changes;std::map<std::wstring,LastCommit> commits;std::wstring error;
  try{
   files=ScanCad(r);changes=git_.Status(r,error);
   changes.erase(std::remove_if(changes.begin(),changes.end(),[](const GitStatus& change){return IsTemporaryCadPath(change.path);}),changes.end());
   if(error.empty()){
    std::set<std::wstring> paths;
    for(const auto& file:files)if(!file.directory)paths.insert(RelativePath(r,file.path));
    for(const auto& change:changes)paths.insert(change.path);
    commits=git_.LatestCommits(r,std::vector<std::wstring>(paths.begin(),paths.end()));
   }
  }catch(const std::exception&){error=L"ローカル情報を読み取れません。作業フォルダとファイルへのアクセスを確認してください。";}
  auto* task=new UiTask{[this,generation,folder=r.folder,files=std::move(files),changes=std::move(changes),commits=std::move(commits),error]()mutable{
   localRefreshRunning_=false;
   if(Current()&&Current()->folder==folder&&repositoryGeneration_==generation){
    if(error.empty()){
     changeChecks_=UpdatedChangeChecks(changes,changeChecks_);cad_=std::move(files);changes_=std::move(changes);latestCommits_=std::move(commits);Populate();
     SetStatus(L"一覧を更新しました。変更 "+std::to_wstring(changes_.size())+L" 件");
    }else SetStatus(error);
   }
   if(localRefreshAgain_){localRefreshAgain_=false;RefreshLocal();}
  }};
  if(!PostMessageW(window_,WM_APP_DONE,0,reinterpret_cast<LPARAM>(task)))delete task;
 });
}
void App::RefreshLocks(){
 if(!Current()||!Current()->lfsLockEnabled){locks_.clear();locksKnown_=false;Populate();return;}
 Background([this,r=*Current(),generation=repositoryGeneration_]{
  std::wstring error;std::lock_guard guard(lockWorkMutex_);auto result=git_.ListLocks(r,error);
  PostMessageW(window_,WM_APP_DONE,0,(LPARAM)new UiTask{[this,generation,result=std::move(result),error]()mutable{
   if(repositoryGeneration_!=generation)return;
   if(error.empty()){locks_.clear();for(auto& lock:result)locks_[lock.path]=std::move(lock);locksKnown_=true;}else locksKnown_=false;
   Populate();if(!error.empty())SetStatus(error);
  }});
 });
}
void App::ContinueWithForeignLock(const std::wstring& path,const std::filesystem::path& localPath,const std::wstring& owner,bool openAfter){
 const auto message=(owner.empty()?std::wstring(L"他の利用者が編集中です。"):owner+L" さんが編集中です。")+
  L"\n\n"+path+L"\n\nこのPCで開いて保存できます。同じファイルの変更が重ならないよう、編集者と確認してください。";
 MessageBoxW(window_,message.c_str(),L"他の人が編集中です",MB_OK|MB_ICONWARNING);
 std::wstring error;
 if(!MakeCadWritable(localPath,error)){
  if(openAfter)opened_.erase(path);
  MessageBoxW(window_,error.c_str(),L"ファイルを編集できません",MB_ICONERROR);SetStatus(error);Populate();return;
 }
 if(openAfter&&!OpenFile(localPath,error)){
  opened_.erase(path);MessageBoxW(window_,error.c_str(),L"ファイルを開けません",MB_ICONERROR);SetStatus(error);
 }
 Populate();
}

void App::LockPath(const std::wstring& path,bool openAfter){
 if(!Current())return;
 if(!Current()->lfsLockEnabled){
  if(!openAfter){opened_.insert(path);Populate();return;}
  const auto localPath=std::filesystem::path(Current()->folder)/std::filesystem::path(path);
  std::wstring error;
  if(!MakeCadWritable(localPath,error)){MessageBoxW(window_,error.c_str(),L"ファイルを編集できません",MB_ICONERROR);SetStatus(error);return;}
  if(!OpenFile(localPath,error)){MessageBoxW(window_,error.c_str(),L"ファイルを開けません",MB_ICONERROR);SetStatus(error);return;}
  opened_.insert(path);Populate();return;
 }
 const bool alreadyOpen=!openAfter&&!opened_.insert(path).second;
 if(alreadyOpen&&!openAfter)return; // Suppress duplicate SolidWorks open/context notifications.
 const auto localPath=std::filesystem::path(Current()->folder)/std::filesystem::path(path);
 auto known=locks_.find(path);
 if(locksKnown_&&known!=locks_.end()){
  if(!known->second.ours){ContinueWithForeignLock(path,localPath,known->second.owner,openAfter);return;}
  Populate();
  if(openAfter){std::wstring error;if(!OpenFile(localPath,error)){opened_.erase(path);MessageBoxW(window_,error.c_str(),L"ファイルを開けません",MB_ICONERROR);ClosedPath(path);}}
  return;
 }
 if(!locking_.insert(path).second)return;
 Background([this,r=*Current(),path,openAfter,localPath,generation=repositoryGeneration_]{
  std::wstring error;std::optional<LockInfo> lock,foreign;
  {std::lock_guard guard(lockWorkMutex_);
   lock=git_.Lock(r,path,error);
   if(!lock){ // Also handle another user taking the lock after the last refresh.
    std::wstring listError;const auto current=git_.ListLocks(r,listError);
    if(listError.empty())for(const auto& item:current)if(item.path==path&&!item.ours){foreign=item;break;}
   }
  }
  PostMessageW(window_,WM_APP_DONE,0,(LPARAM)new UiTask{[this,path,openAfter,localPath,generation,lock=std::move(lock),foreign=std::move(foreign),error]{
   if(repositoryGeneration_!=generation)return;locking_.erase(path);
   if(lock){
    locks_[path]=*lock;locksKnown_=true;Populate();
    if(openAfter){std::wstring openError;if(!OpenFile(localPath,openError)){opened_.erase(path);MessageBoxW(window_,openError.c_str(),L"ファイルを開けません",MB_ICONERROR);ClosedPath(path);}}
    else if(!opened_.contains(path))ClosedPath(path);
   }else if(foreign){
    locks_[path]=*foreign;locksKnown_=true;
    if(openAfter||opened_.contains(path))ContinueWithForeignLock(path,localPath,foreign->owner,openAfter);
   }else{
    SetStatus(error);
    MessageBoxW(window_,error.c_str(),L"ロックできません",MB_ICONWARNING);Populate();
   }
  }});
 });
}
void App::ClosedPath(const std::wstring& path){
 opened_.erase(path);if(!Current()||!Current()->lfsLockEnabled){Populate();return;}const auto found=locks_.find(path);
 if(found==locks_.end()||!found->second.ours){Populate();return;}
 if(!closing_.insert(path).second)return;
 Background([this,r=*Current(),path,lock=found->second,generation=repositoryGeneration_]{
  const bool dirty=git_.IsDirty(r,path);
  PostMessageW(window_,WM_APP_DONE,0,reinterpret_cast<LPARAM>(new UiTask{[this,r,path,lock,generation,dirty]{
   if(repositoryGeneration_!=generation)return;
   if(opened_.contains(path)){closing_.erase(path);return;}
   if(dirty){closing_.erase(path);pendingUnlock_.insert(path);SetStatus(L"変更済みのため、送信完了までロックを保持します。 "+path);Populate();return;}
   Background([this,r,path,lock,generation]{
    std::wstring error;bool ok;{std::lock_guard guard(lockWorkMutex_);ok=git_.Unlock(r,lock,error);}
    PostMessageW(window_,WM_APP_DONE,0,reinterpret_cast<LPARAM>(new UiTask{[this,path,ok,error,generation]{
     if(repositoryGeneration_!=generation)return;closing_.erase(path);
     if(ok){locks_.erase(path);pendingUnlock_.erase(path);}else SetStatus(error);Populate();
    }}));
   });
  }}));
 });
}
bool App::SyncSolidWorksOpenState(){
 if(syncingSolidWorks_)return false;syncingSolidWorks_=true;
 std::vector<std::wstring> documents;const bool confirmed=solidworks_.OpenDocuments(documents);
 if(!confirmed){syncingSolidWorks_=false;return false;}
 std::set<std::wstring> actual;const auto* repository=Current();
 if(repository)for(const auto& absolute:documents){
  auto relative=RepositoryDocumentPath(*repository,absolute);if(!relative.empty()&&IsLockableCad(relative)&&!IsTemporaryCadPath(relative))actual.insert(std::move(relative));
 }
 std::vector<std::wstring> closed;
 for(const auto& path:opened_){const bool exists=std::any_of(actual.begin(),actual.end(),[&](const auto& value){return CompareStringOrdinal(value.c_str(),-1,path.c_str(),-1,TRUE)==CSTR_EQUAL;});if(!exists)closed.push_back(path);}
 for(const auto& path:closed)ClosedPath(path);
 syncingSolidWorks_=false;return true;
}
void App::ReleasePendingLocks(){
 if(!Current()||!Current()->lfsLockEnabled||!SyncSolidWorksOpenState())return;
 std::vector<std::wstring> release;for(const auto& [path,lock]:locks_)if(lock.ours&&!opened_.contains(path)&&!locking_.contains(path))release.push_back(path);
 for(const auto& path:release)ClosedPath(path);
}
void App::HandleSolidWorksEvent(const SolidWorksEvent& event){auto* repo=Current();if(!repo)return;auto path=RepositoryDocumentPath(*repo,event.path);if(path.empty())return;if(IsTemporaryCadPath(path)||!IsLockableCad(path))return;switch(event.kind){case SolidWorksEventKind::Opened:case SolidWorksEventKind::BeginContextEdit:LockPath(path);break;case SolidWorksEventKind::Closed:case SolidWorksEventKind::EndContextEdit:ClosedPath(path);break;}}
}
