#include "app.h"
#include <iostream>
#include <stdexcept>

namespace {
void Require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
void Write(const std::filesystem::path& path,const std::string& text){std::filesystem::create_directories(path.parent_path());std::ofstream out(path,std::ios::binary|std::ios::trunc);out<<text;Require(static_cast<bool>(out),"fixture write failed");}
const rc::BrowserItem& Find(const std::vector<rc::BrowserItem>& items,const std::wstring& path){const auto it=std::find_if(items.begin(),items.end(),[&](const auto& item){return item.path==path;});Require(it!=items.end(),"missing browser item");return *it;}
}

namespace rc {
struct AppBrowserTest {
 static LRESULT CALLBACK Procedure(HWND window,UINT message,WPARAM w,LPARAM l){
  if(message==WM_NCCREATE){auto* create=reinterpret_cast<CREATESTRUCTW*>(l);SetWindowLongPtrW(window,GWLP_USERDATA,reinterpret_cast<LONG_PTR>(create->lpCreateParams));}
  auto* app=reinterpret_cast<App*>(GetWindowLongPtrW(window,GWLP_USERDATA));
  // Only route test controls and refresh completions. Never run production startup/network/update hooks.
  if(app&&(message==WM_NOTIFY||message==WM_COMMAND||message==WM_APP_DONE||message==WM_APP+43))return app->WndProc(window,message,w,l);
  return DefWindowProcW(window,message,w,l);
 }
 static void PumpUntil(App& app,const std::function<bool()>& done){
  const auto deadline=GetTickCount64()+20000;
  while(!done()&&GetTickCount64()<deadline){
   MSG message;while(PeekMessageW(&message,nullptr,0,0,PM_REMOVE)){TranslateMessage(&message);DispatchMessageW(&message);}
   MsgWaitForMultipleObjects(0,nullptr,FALSE,20,QS_ALLINPUT);
  }
  Require(done(),"native UI/notification test timed out");
  (void)app;
 }
 static int Row(const App& app,const std::wstring& path){
  const auto it=std::find(app.listPaths_.begin(),app.listPaths_.end(),path);Require(it!=app.listPaths_.end(),"native list row missing");return static_cast<int>(it-app.listPaths_.begin());
 }
 static void Select(App& app,const std::wstring& path){const int row=Row(app,path);ListView_SetItemState(app.list_,-1,0,LVIS_SELECTED|LVIS_FOCUSED);ListView_SetItemState(app.list_,row,LVIS_SELECTED|LVIS_FOCUSED,LVIS_SELECTED|LVIS_FOCUSED);}
 static void Snapshot(HWND window,const std::filesystem::path& output){
  RECT rect{};GetClientRect(window,&rect);const int width=rect.right,height=rect.bottom;
  BITMAPINFO info{};info.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);info.bmiHeader.biWidth=width;info.bmiHeader.biHeight=-height;info.bmiHeader.biPlanes=1;info.bmiHeader.biBitCount=32;info.bmiHeader.biCompression=BI_RGB;
  HDC dc=GetDC(window),memory=CreateCompatibleDC(dc);void* pixels{};HBITMAP bitmap=CreateDIBSection(dc,&info,DIB_RGB_COLORS,&pixels,nullptr,0);auto old=SelectObject(memory,bitmap);
  SendMessageW(window,WM_PRINT,reinterpret_cast<WPARAM>(memory),PRF_CLIENT|PRF_CHILDREN|PRF_ERASEBKGND);
  BITMAPFILEHEADER header{};header.bfType=0x4d42;header.bfOffBits=sizeof(header)+sizeof(BITMAPINFOHEADER);header.bfSize=header.bfOffBits+width*height*4;
  std::ofstream file(output,std::ios::binary);file.write(reinterpret_cast<const char*>(&header),sizeof(header));file.write(reinterpret_cast<const char*>(&info.bmiHeader),sizeof(info.bmiHeader));file.write(static_cast<const char*>(pixels),static_cast<std::streamsize>(width)*height*4);
  SelectObject(memory,old);DeleteObject(bitmap);DeleteDC(memory);ReleaseDC(window,dc);
 }
 static void Run(const Repository& repository,const std::filesystem::path& git,const std::filesystem::path& output){
  INITCOMMONCONTROLSEX controls{sizeof(controls),ICC_LISTVIEW_CLASSES|ICC_BAR_CLASSES};InitCommonControlsEx(&controls);
  WNDCLASSW cls{};cls.lpfnWndProc=Procedure;cls.hInstance=GetModuleHandleW(nullptr);cls.lpszClassName=L"RoboconCadManager.FolderBrowserTest";cls.hbrBackground=reinterpret_cast<HBRUSH>(COLOR_WINDOW+1);RegisterClassW(&cls);
  App app;app.instance_=cls.hInstance;app.git_=GitClient(git);app.config_.Upsert(repository);app.selected_=0;
  app.window_=CreateWindowW(cls.lpszClassName,L"フォルダ表示テスト",WS_OVERLAPPEDWINDOW,0,0,1120,700,nullptr,nullptr,cls.hInstance,&app);Require(app.window_!=nullptr,"test window failed");
  app.CreateBrowserControls();app.cad_=ScanCad(repository);std::wstring error;app.changes_=app.git_.Status(repository,error);Require(error.empty(),"native UI status failed");app.changeChecks_=UpdatedChangeChecks(app.changes_,{});app.Populate();
  Require(SendMessageW(app.filter_,CB_GETCURSEL,0,0)==0&&GetText(app.filter_)==L"すべてのファイル","file filter selection is blank");
  Require(!IsWindowEnabled(GetDlgItem(app.window_,kBrowseBack)),"back enabled at initial root");
  Select(app,L"機構 A");NMITEMACTIVATE doubleClick{};doubleClick.hdr.hwndFrom=app.list_;doubleClick.hdr.code=NM_DBLCLK;doubleClick.iItem=Row(app,L"機構 A");SendMessageW(app.window_,WM_NOTIFY,0,reinterpret_cast<LPARAM>(&doubleClick));
  Require(app.folderNavigation_.Current()==L"機構 A","double click did not enter folder");
  Require(IsWindowEnabled(GetDlgItem(app.window_,kBrowseBack)),"back disabled after navigation");
  Select(app,L"機構 A/パーツ");app.HandleCommand(106);
  wchar_t text[512]{};ListView_GetItemText(app.list_,Row(app,L"機構 A/パーツ/既存.SLDPRT"),0,text,512);Require(std::wstring(text)==L"既存.SLDPRT","file name still contains folder prefix");
  const int fileRow=Row(app,L"機構 A/パーツ/既存.SLDPRT");Require(ListView_GetCheckState(app.list_,fileRow),"nested modification was not checked");ListView_SetCheckState(app.list_,fileRow,FALSE);
  app.HandleCommand(kBrowseBack);app.HandleCommand(kBrowseForward);Require(!ListView_GetCheckState(app.list_,Row(app,L"機構 A/パーツ/既存.SLDPRT")),"manual check state lost during back/forward");
  app.HandleCommand(kBrowseUp);app.HandleCommand(kBrowseUp);Require(app.folderNavigation_.Current().empty(),"up did not return to root");
  ListView_SetCheckState(app.list_,Row(app,L"機構 A"),TRUE);Require(app.changeChecks_[L"機構 A/パーツ/既存.SLDPRT"],"folder selection did not select descendants");
  ListView_SetCheckState(app.list_,Row(app,L"機構 A"),FALSE);Require(!app.changeChecks_[L"機構 A/新しい 階層/部品1.SLDPRT"],"folder deselection did not clear descendant checks");
  app.HandleCommand(kBrowseBack);app.NavigateFolder(L"docs");Require(!app.folderNavigation_.CanForward(),"new navigation did not clear forward history");app.NavigateFolder(L"");
  app.filterMode_=1;app.Populate();Require(Find(app.browserItems_,L"機構 A").directory,"filter hid folder navigation");app.filterMode_=0;app.Populate();
  for(UINT dpi:{96u,120u,144u,192u}){
   SetWindowPos(app.window_,nullptr,0,0,MulDiv(1120,dpi,96),MulDiv(700,dpi,96),SWP_NOZORDER|SWP_NOACTIVATE);app.UpdateFonts(dpi);
   RECT client{},list{},label{};GetClientRect(app.window_,&client);GetWindowRect(app.list_,&list);GetWindowRect(app.folderLabel_,&label);MapWindowPoints(nullptr,app.window_,reinterpret_cast<POINT*>(&list),2);MapWindowPoints(nullptr,app.window_,reinterpret_cast<POINT*>(&label),2);
   Require(list.top>=label.bottom&&list.right<=client.right&&list.bottom<=client.bottom,"DPI layout overlaps or exceeds client");
   int width{},height{};ImageList_GetIconSize(app.images_,&width,&height);Require(width==MulDiv(24,dpi,96)&&width==height,"DPI icon size did not update");
   LOGFONTW font{};GetObjectW(app.font_,sizeof(font),&font);Require(std::wstring(font.lfFaceName)==L"Yu Gothic UI","wrong Japanese font");
   Snapshot(app.window_,output/(L"folder-browser-"+std::to_wstring(dpi)+L".bmp"));
  }
  SetWindowPos(app.window_,nullptr,0,0,1120,700,SWP_NOZORDER|SWP_NOACTIVATE);app.UpdateFonts(96);app.NavigateFolder(L"機構 A/パーツ");Snapshot(app.window_,output/L"folder-browser-inside.bmp");app.NavigateFolder(L"");
  // Exercise the actual recursive Windows file notification and refresh path, with no polling in the app.
  app.StartLocalWatcher();app.RefreshLocal();PumpUntil(app,[&]{return !app.localRefreshRunning_;});
  Write(std::filesystem::path(repository.folder)/L"自動監視/深い/自動.SLDPRT","new watcher content");
  PumpUntil(app,[&]{return app.changeChecks_.contains(L"自動監視/深い/自動.SLDPRT")&&!app.localRefreshRunning_;});
  Require(app.changeChecks_[L"自動監視/深い/自動.SLDPRT"],"recursive notification did not auto-select nested file");
  app.StopLocalWatcher();PumpUntil(app,[&]{return !app.localRefreshRunning_;});
  DestroyWindow(app.window_);if(app.images_)ImageList_Destroy(app.images_);if(app.font_)DeleteObject(app.font_);if(app.strikeFont_)DeleteObject(app.strikeFont_);
 }
};
}

int wmain(int argc,wchar_t** argv){
 if(argc!=3)return 1;
 try{
  SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
  const auto root=std::filesystem::path(argv[2]);const auto work=root/L"作業フォルダ";std::filesystem::create_directories(work);
  const auto global=root/L"isolated-global.config";SetEnvironmentVariableW(L"GIT_CONFIG_GLOBAL",global.c_str());SetEnvironmentVariableW(L"GIT_CONFIG_NOSYSTEM",L"1");
  rc::Repository repository{L"test",L"",work.wstring(),L"test",L"test@example.invalid"};rc::GitClient git(argv[1]);
  auto run=[&](const std::vector<std::wstring>& args){const auto result=git.Run(work,args);Require(result.exitCode==0,"fixture git command failed");};
  run({L"init",L"-b",L"main",L"--quiet"});Require(git.Configure(repository).exitCode==0,"fixture identity failed");
  Write(work/L"機構 A/パーツ/既存.SLDPRT","base content");Write(work/L"削除フォルダ/deleted.SLDASM","delete me");Write(work/L"README.md","readme");std::filesystem::create_directories(work/L"空フォルダ");run({L"add",L"."});run({L"commit",L"-m",L"base",L"--quiet"});
  Write(work/L"機構 A/パーツ/既存.SLDPRT","changed longer content");std::filesystem::remove(work/L"削除フォルダ/deleted.SLDASM");std::filesystem::remove(work/L"削除フォルダ");
  Write(work/L"機構 A/新しい 階層/部品1.SLDPRT","new part");Write(work/L"機構 A/新しい 階層/~$temporary.SLDPRT","temporary");Write(work/L"図面/drawing.SLDDRW","drawing");Write(work/L"docs/note.md","note");Write(work/L".git/hidden.SLDPRT","git internals");run({L"add",L"--",L"図面/drawing.SLDDRW"});run({L"config",L"status.showUntrackedFiles",L"no"});
  std::wstring error;auto changes=git.Status(repository,error);Require(error.empty(),"status failed");
  const auto part=std::find_if(changes.begin(),changes.end(),[](const auto& change){return change.path==L"機構 A/新しい 階層/部品1.SLDPRT";});Require(part!=changes.end()&&part->code==L"??","untracked folder was not expanded into individual files");
  const auto files=rc::ScanCad(repository);Require(std::none_of(files.begin(),files.end(),[](const auto& file){return file.path.generic_wstring().find(L"/.git/")!=std::wstring::npos||rc::IsTemporaryCadPath(file.path.wstring());}),"internal/temporary files scanned");
  auto items=rc::BuildFolderItems(repository,L"",files,changes,0);Require(Find(items,L"機構 A").directory,"root folder missing");Require(Find(items,L"空フォルダ").directory,"empty folder missing");Require(Find(items,L"削除フォルダ").directory,"virtual deleted folder missing");Require(Find(items,L"README.md").name==L"README.md","normal local file missing");
  for(const auto& item:items)Require(item.name.find(L'/')==std::wstring::npos,"flattened folder prefix still visible");
  items=rc::BuildFolderItems(repository,L"削除フォルダ",files,changes,0);Require(!Find(items,L"削除フォルダ/deleted.SLDASM").local,"deleted file reported as local");
  items=rc::BuildFolderItems(repository,L"機構 A/新しい 階層",files,changes,1);Require(items.size()==1&&items.front().name==L"部品1.SLDPRT","nested filter or temporary exclusion failed");
  auto checks=rc::UpdatedChangeChecks(changes,{});Require(checks[L"機構 A/パーツ/既存.SLDPRT"]&&checks[L"機構 A/新しい 階層/部品1.SLDPRT"],"nested changes were not auto-selected");checks[L"機構 A/パーツ/既存.SLDPRT"]=false;checks=rc::UpdatedChangeChecks(changes,checks);Require(!checks[L"機構 A/パーツ/既存.SLDPRT"],"manual deselection lost on refresh");
  rc::FolderNavigation navigation;Require(navigation.Navigate(L"機構 A\\パーツ")&&navigation.Current()==L"機構 A/パーツ","path normalization failed");Require(navigation.Up()&&navigation.Back()&&navigation.Forward(),"navigation history failed");Require(!navigation.Navigate(L"../outside")&&!navigation.Navigate(L"C:\\outside"),"navigation escaped repository");
  rc::AppBrowserTest::Run(repository,argv[1],root);
  std::cout<<"PASS: nested new/modified/deleted files, folder view/history, native double-click/checkboxes, 100-200% DPI, recursive Windows notifications.\n";return 0;
 }catch(const std::exception& error){std::cerr<<error.what()<<"\n";return 1;}
}
