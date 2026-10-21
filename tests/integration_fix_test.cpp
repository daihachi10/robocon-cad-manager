#include "app.h"
#include <iostream>
#include <stdexcept>

namespace {
void Check(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
class Document final:public IDispatch {
 public:
 ULONG refs{1};Document* first{};Document* next{};std::wstring path;bool busy{};
 HRESULT STDMETHODCALLTYPE QueryInterface(REFIID id,void** out)override{if(!out)return E_POINTER;*out=nullptr;if(id==IID_IUnknown||id==IID_IDispatch){*out=this;AddRef();return S_OK;}return E_NOINTERFACE;}
 ULONG STDMETHODCALLTYPE AddRef()override{return ++refs;}
 ULONG STDMETHODCALLTYPE Release()override{return --refs;}
 HRESULT STDMETHODCALLTYPE GetTypeInfoCount(UINT* count)override{*count=0;return S_OK;}
 HRESULT STDMETHODCALLTYPE GetTypeInfo(UINT,LCID,ITypeInfo**)override{return E_NOTIMPL;}
 HRESULT STDMETHODCALLTYPE GetIDsOfNames(REFIID,LPOLESTR* names,UINT,LCID,DISPID* id)override{
  if(wcscmp(names[0],L"GetFirstDocument")==0)*id=1;else if(wcscmp(names[0],L"GetPathName")==0)*id=2;else if(wcscmp(names[0],L"GetNext")==0)*id=3;else return DISP_E_UNKNOWNNAME;return S_OK;
 }
 HRESULT STDMETHODCALLTYPE Invoke(DISPID id,REFIID,LCID,WORD,DISPPARAMS*,VARIANT* out,EXCEPINFO*,UINT*)override{
  if(busy)return RPC_E_CALL_REJECTED;VariantInit(out);
  if(id==2){out->vt=VT_BSTR;out->bstrVal=SysAllocString(path.c_str());return S_OK;}
  auto* value=id==1?first:next;if(value){out->vt=VT_DISPATCH;out->pdispVal=value;value->AddRef();}return S_OK;
 }
};
Document* active{};HRESULT lookupResult=S_OK;
HRESULT Lookup(IDispatch** out){*out=nullptr;if(FAILED(lookupResult))return lookupResult;if(!active)return MK_E_UNAVAILABLE;*out=active;active->AddRef();return S_OK;}

class Files final:public IDataObject {
 ULONG refs_{1};std::vector<std::filesystem::path> paths_;
 public:
 explicit Files(std::vector<std::filesystem::path> paths):paths_(std::move(paths)){}
 HRESULT STDMETHODCALLTYPE QueryInterface(REFIID id,void** out)override{if(!out)return E_POINTER;*out=nullptr;if(id==IID_IUnknown||id==IID_IDataObject){*out=this;AddRef();return S_OK;}return E_NOINTERFACE;}
 ULONG STDMETHODCALLTYPE AddRef()override{return ++refs_;}ULONG STDMETHODCALLTYPE Release()override{return --refs_;}
 HRESULT STDMETHODCALLTYPE QueryGetData(FORMATETC* f)override{return f&&f->cfFormat==CF_HDROP&&(f->tymed&TYMED_HGLOBAL)?S_OK:DV_E_FORMATETC;}
 HRESULT STDMETHODCALLTYPE GetData(FORMATETC* f,STGMEDIUM* medium)override{
  if(FAILED(QueryGetData(f)))return DV_E_FORMATETC;size_t bytes=sizeof(DROPFILES)+sizeof(wchar_t);for(const auto& p:paths_)bytes+=(p.wstring().size()+1)*sizeof(wchar_t);
  auto memory=GlobalAlloc(GMEM_MOVEABLE|GMEM_ZEROINIT,bytes);if(!memory)return E_OUTOFMEMORY;auto* header=static_cast<DROPFILES*>(GlobalLock(memory));header->pFiles=sizeof(DROPFILES);header->fWide=TRUE;
  auto* destination=reinterpret_cast<wchar_t*>(reinterpret_cast<BYTE*>(header)+sizeof(DROPFILES));for(const auto& p:paths_){const auto value=p.wstring();std::copy(value.begin(),value.end(),destination);destination+=value.size()+1;}GlobalUnlock(memory);medium->tymed=TYMED_HGLOBAL;medium->hGlobal=memory;medium->pUnkForRelease=nullptr;return S_OK;
 }
 HRESULT STDMETHODCALLTYPE GetDataHere(FORMATETC*,STGMEDIUM*)override{return E_NOTIMPL;}
 HRESULT STDMETHODCALLTYPE GetCanonicalFormatEtc(FORMATETC*,FORMATETC*)override{return E_NOTIMPL;}
 HRESULT STDMETHODCALLTYPE SetData(FORMATETC*,STGMEDIUM*,BOOL)override{return E_NOTIMPL;}
 HRESULT STDMETHODCALLTYPE EnumFormatEtc(DWORD,IEnumFORMATETC**)override{return E_NOTIMPL;}
 HRESULT STDMETHODCALLTYPE DAdvise(FORMATETC*,DWORD,IAdviseSink*,DWORD*)override{return OLE_E_ADVISENOTSUPPORTED;}
 HRESULT STDMETHODCALLTYPE DUnadvise(DWORD)override{return OLE_E_ADVISENOTSUPPORTED;}
 HRESULT STDMETHODCALLTYPE EnumDAdvise(IEnumSTATDATA**)override{return OLE_E_ADVISENOTSUPPORTED;}
};
}

namespace rc {
struct SolidWorksMonitorTest {static void Isolate(SolidWorksMonitor& monitor){monitor.lookup_=Lookup;}};
struct IntegrationFixTest {
 static LRESULT CALLBACK Procedure(HWND window,UINT message,WPARAM w,LPARAM l){
  if(message==WM_NCCREATE){auto* create=reinterpret_cast<CREATESTRUCTW*>(l);SetWindowLongPtrW(window,GWLP_USERDATA,reinterpret_cast<LONG_PTR>(create->lpCreateParams));}
  auto* app=reinterpret_cast<App*>(GetWindowLongPtrW(window,GWLP_USERDATA));
  if(app&&(message==WM_APP_DONE||message==WM_NOTIFY))return app->WndProc(window,message,w,l);return DefWindowProcW(window,message,w,l);
 }
 static void Pump(const std::function<bool()>& done){const auto deadline=GetTickCount64()+15000;while(!done()&&GetTickCount64()<deadline){MSG msg;while(PeekMessageW(&msg,nullptr,0,0,PM_REMOVE)){TranslateMessage(&msg);DispatchMessageW(&msg);}MsgWaitForMultipleObjects(0,nullptr,FALSE,10,QS_ALLINPUT);}Check(done(),"copy/refresh did not finish");}
 static POINTL AtRow(HWND list,int row){RECT rect{};ListView_GetItemRect(list,row,&rect,LVIR_BOUNDS);POINT p{rect.left+50,(rect.top+rect.bottom)/2};ClientToScreen(list,&p);return{p.x,p.y};}
 static void Run(const std::filesystem::path& git,const std::filesystem::path& root){
  INITCOMMONCONTROLSEX controls{sizeof(controls),ICC_LISTVIEW_CLASSES|ICC_BAR_CLASSES};InitCommonControlsEx(&controls);
  Repository repo{L"test",L"",(root/L"作業 フォルダー").wstring(),L"test",L"test@example.invalid"};std::filesystem::create_directories(repo.folder);
  Document application,part,restarted;
  App app;app.git_=GitClient(git);app.config_.Upsert(repo);app.selected_=0;SolidWorksMonitorTest::Isolate(app.solidworks_);
  WNDCLASSW cls{};cls.lpfnWndProc=Procedure;cls.hInstance=GetModuleHandleW(nullptr);cls.lpszClassName=L"RoboconCadManager.FixTest";RegisterClassW(&cls);
  app.instance_=cls.hInstance;app.window_=CreateWindowW(cls.lpszClassName,L"テスト",WS_OVERLAPPEDWINDOW,0,0,1120,700,nullptr,nullptr,cls.hInstance,&app);Check(app.window_!=nullptr,"hidden test window creation failed");app.CreateBrowserControls();app.LayoutBrowser();
  application.first=&part;part.path=(std::filesystem::path(repo.folder)/L"部品.SLDPRT").wstring();active=&application;
  app.opened_.insert(L"部品.SLDPRT");Check(app.SyncSolidWorksOpenState()&&app.opened_.contains(L"部品.SLDPRT"),"confirmed open document lost");
  // Busy/failed COM is unknown, not a successful empty document list.
  application.busy=true;Check(!app.SyncSolidWorksOpenState()&&app.opened_.contains(L"部品.SLDPRT"),"COM error incorrectly cleared open document");application.busy=false;
  application.first=nullptr;Check(app.SyncSolidWorksOpenState()&&app.opened_.empty(),"closed document left a ghost open state");
  app.opened_.insert(L"部品.SLDPRT");active=nullptr;Check(app.SyncSolidWorksOpenState()&&app.opened_.empty(),"exited SolidWorks left ghost state");
  restarted.first=&part;active=&restarted;std::vector<std::wstring> paths;Check(app.solidworks_.OpenDocuments(paths)&&paths.size()==1,"failed to reconnect after restart");
  active=nullptr;Check(app.SyncSolidWorksOpenState(),"missing application should confirm no open documents");
  auto run=[&](const std::vector<std::wstring>& args){Check(app.git_.Run(repo.folder,args).exitCode==0,"fixture git failed");};run({L"init",L"-b",L"main",L"--quiet"});Check(app.git_.Configure(repo).exitCode==0,"fixture identity failed");
  std::filesystem::create_directories(std::filesystem::path(repo.folder)/L"追加先");std::ofstream(std::filesystem::path(repo.folder)/L"既存.txt")<<"existing";std::ofstream(std::filesystem::path(repo.folder)/L"追加先/.gitkeep").close();
  app.cad_=ScanCad(repo);app.filterMode_=AllFiles;app.Populate();const auto source=root/L"日本語 ファイル.txt";std::ofstream(source)<<"drag payload";Files data({source});Check(ExtractDropFiles(&data)==std::vector<std::filesystem::path>{source},"CF_HDROP handle decoded incorrectly");
  auto* drop=new FileDropTarget(app.list_,[&](POINTL p){return app.DropEffect(p);},[&](IDataObject* object,POINTL p){app.HandleDroppedFiles(object,p);});
  Check(RegisterDragDrop(app.list_,drop)==S_OK,"native OLE drop target registration failed");Check(RegisterDragDrop(app.list_,drop)==DRAGDROP_E_ALREADYREGISTERED,"drop target not actually registered");
  const auto folder=std::find_if(app.browserItems_.begin(),app.browserItems_.end(),[](const auto& item){return item.directory;});Check(folder!=app.browserItems_.end(),"folder row missing");const int row=static_cast<int>(folder-app.browserItems_.begin());const auto folderPoint=AtRow(app.list_,row);
  DWORD effect=DROPEFFECT_COPY|DROPEFFECT_MOVE;Check(drop->DragEnter(&data,0,folderPoint,&effect)==S_OK&&effect==DROPEFFECT_COPY,"folder drag rejected");Check(ListView_GetItemState(app.list_,row,LVIS_DROPHILITED)!=0,"folder not highlighted");drop->DragLeave();Check(!ListView_GetItemState(app.list_,row,LVIS_DROPHILITED),"highlight stuck after drag leave");
  const auto file=std::find_if(app.browserItems_.begin(),app.browserItems_.end(),[](const auto& item){return !item.directory;});const auto filePoint=AtRow(app.list_,static_cast<int>(file-app.browserItems_.begin()));effect=DROPEFFECT_COPY;drop->DragEnter(&data,0,filePoint,&effect);Check(effect==DROPEFFECT_NONE,"file row accepted drop");drop->DragLeave();
  effect=DROPEFFECT_COPY;drop->DragEnter(&data,0,folderPoint,&effect);effect=DROPEFFECT_COPY;Check(drop->Drop(&data,0,folderPoint,&effect)==S_OK&&effect==DROPEFFECT_COPY,"drop callback rejected files");
  Pump([&]{return app.changeChecks_.contains(L"追加先/日本語 ファイル.txt")&&!app.localRefreshRunning_;});Check(app.changeChecks_[L"追加先/日本語 ファイル.txt"],"dropped file not automatically checked");Check(!std::filesystem::exists(std::filesystem::path(repo.folder)/L"追加先/.gitkeep"),"gitkeep not removed");
  const auto source2=root/L"二つ目.txt";std::ofstream(source2)<<"second payload";Files multiple({source,source2});
  RECT client{};GetClientRect(app.list_,&client);POINT blank{30,client.bottom-30};ClientToScreen(app.list_,&blank);POINTL blankPoint{blank.x,blank.y};
  effect=DROPEFFECT_COPY;drop->DragEnter(&multiple,0,blankPoint,&effect);Check(effect==DROPEFFECT_COPY,"empty list area rejected files");effect=DROPEFFECT_COPY;drop->Drop(&multiple,0,blankPoint,&effect);
  Pump([&]{return app.changeChecks_.contains(L"二つ目.txt")&&!app.localRefreshRunning_;});Check(std::filesystem::exists(std::filesystem::path(repo.folder)/L"日本語 ファイル.txt"),"multi-file drop missed first file");
  RevokeDragDrop(app.list_);drop->Release();app.solidworks_.Stop();DestroyWindow(app.window_);if(app.images_)ImageList_Destroy(app.images_);if(app.font_)DeleteObject(app.font_);if(app.strikeFont_)DeleteObject(app.strikeFont_);
 }
};
}
int wmain(int argc,wchar_t** argv){if(argc!=3)return 2;const auto ole=OleInitialize(nullptr);if(FAILED(ole))return 3;int result=0;try{const auto root=std::filesystem::path(argv[2]);std::filesystem::create_directories(root);const auto global=root/L"isolated.config";SetEnvironmentVariableW(L"GIT_CONFIG_GLOBAL",global.c_str());SetEnvironmentVariableW(L"GIT_CONFIG_NOSYSTEM",L"1");rc::IntegrationFixTest::Run(argv[1],root);std::cout<<"PASS: confirmed open/close, COM busy, SolidWorks exit/restart, native OLE registration, Unicode CF_HDROP, folder/file hit tests, highlight cleanup, actual background copy and automatic selection.\n";}catch(const std::exception& e){std::cerr<<e.what()<<"\n";result=1;}OleUninitialize();return result;}
