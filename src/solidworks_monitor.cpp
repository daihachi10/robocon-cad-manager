#include "solidworks_monitor.h"
namespace rc { namespace {
SolidWorksMonitor* gMonitor{};
HRESULT ActiveApplication(IDispatch** application){
 *application=nullptr;CLSID clsid{};auto hr=CLSIDFromProgID(L"SldWorks.Application",&clsid);if(FAILED(hr))return hr;
 IUnknown* unknown{};hr=GetActiveObject(clsid,nullptr,&unknown);if(FAILED(hr))return hr;
 hr=unknown->QueryInterface(IID_IDispatch,reinterpret_cast<void**>(application));unknown->Release();return hr;
}
HRESULT ReadValue(IDispatch* object,const wchar_t* name,VARIANT& value){
 VariantInit(&value);LPOLESTR names[]={const_cast<LPOLESTR>(name)};DISPID id{};
 auto hr=object->GetIDsOfNames(IID_NULL,names,1,LOCALE_USER_DEFAULT,&id);if(FAILED(hr))return hr;
 DISPPARAMS params{};return object->Invoke(id,IID_NULL,LOCALE_USER_DEFAULT,DISPATCH_METHOD|DISPATCH_PROPERTYGET,&params,&value,nullptr,nullptr);
}
std::wstring InvokeString(IDispatch* object, const wchar_t* name) { if(!object)return{}; LPOLESTR names[]={const_cast<LPOLESTR>(name)}; DISPID id{}; if(FAILED(object->GetIDsOfNames(IID_NULL,names,1,LOCALE_USER_DEFAULT,&id)))return{}; DISPPARAMS p{}; VARIANT value; VariantInit(&value); const auto hr=object->Invoke(id,IID_NULL,LOCALE_USER_DEFAULT,DISPATCH_METHOD|DISPATCH_PROPERTYGET,&p,&value,nullptr,nullptr); std::wstring result; if(SUCCEEDED(hr)&&value.vt==VT_BSTR&&value.bstrVal)result=value.bstrVal; VariantClear(&value); return result; }
IDispatch* InvokeDocument(IDispatch* object, const wchar_t* name, const std::wstring& argument=L"") { if(!object)return nullptr; LPOLESTR names[]={const_cast<LPOLESTR>(name)}; DISPID id{}; if(FAILED(object->GetIDsOfNames(IID_NULL,names,1,LOCALE_USER_DEFAULT,&id)))return nullptr; VARIANT arg; VariantInit(&arg); DISPPARAMS p{}; if(!argument.empty()){arg.vt=VT_BSTR;arg.bstrVal=SysAllocString(argument.c_str());p.cArgs=1;p.rgvarg=&arg;} VARIANT value;VariantInit(&value); const auto hr=object->Invoke(id,IID_NULL,LOCALE_USER_DEFAULT,DISPATCH_METHOD|DISPATCH_PROPERTYGET,&p,&value,nullptr,nullptr); if(arg.bstrVal)SysFreeString(arg.bstrVal); IDispatch* result=nullptr;if(SUCCEEDED(hr)&&value.vt==VT_DISPATCH&&value.pdispVal){result=value.pdispVal;result->AddRef();}VariantClear(&value);return result; }
std::wstring FirstPath(DISPPARAMS* p){if(!p)return{};for(UINT i=0;i<p->cArgs;++i){const auto& v=p->rgvarg[i];if(v.vt==VT_BSTR&&v.bstrVal)return v.bstrVal;if(v.vt==VT_DISPATCH&&v.pdispVal){auto path=InvokeString(v.pdispVal,L"GetPathName");if(!path.empty())return path;}}return{};}
bool IsSolidWorksWindow(HWND window){DWORD pid{};GetWindowThreadProcessId(window,&pid);HANDLE process=OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION,FALSE,pid);if(!process)return false;wchar_t path[MAX_PATH];DWORD size=MAX_PATH;const bool ok=QueryFullProcessImageNameW(process,0,path,&size);CloseHandle(process);return ok&&IEndsWith(path,L"\\sldworks.exe");}
}
struct SolidWorksMonitor::Subscription {
 SolidWorksMonitor* owner{};GUID source{}; IConnectionPoint* point{}; DWORD cookie{}; ITypeInfo* type{}; IDispatch* document{}; std::wstring path;
 struct Sink final: IDispatch { Subscription* sub{}; ULONG refs{1}; HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid,void** value) override{if(iid==IID_IUnknown||iid==IID_IDispatch||(sub&&iid==sub->source)){*value=this;AddRef();return S_OK;}*value=nullptr;return E_NOINTERFACE;} ULONG STDMETHODCALLTYPE AddRef() override{return ++refs;} ULONG STDMETHODCALLTYPE Release() override{const auto count=--refs;if(!count)delete this;return count;} HRESULT STDMETHODCALLTYPE GetTypeInfoCount(UINT* count) override{*count=0;return S_OK;} HRESULT STDMETHODCALLTYPE GetTypeInfo(UINT,LCID,ITypeInfo**) override{return E_NOTIMPL;} HRESULT STDMETHODCALLTYPE GetIDsOfNames(REFIID,LPOLESTR*,UINT,LCID,DISPID*) override{return DISP_E_UNKNOWNNAME;} HRESULT STDMETHODCALLTYPE Invoke(DISPID id,REFIID,LCID,WORD,DISPPARAMS* params,VARIANT*,EXCEPINFO*,UINT*) override{sub->owner->Dispatch(sub,id,params);return S_OK;} };
 Sink* sink{};
 ~Subscription(){if(point&&cookie)point->Unadvise(cookie);if(sink)sink->Release();if(type)type->Release();if(document)document->Release();if(point)point->Release();}
};
SolidWorksMonitor::~SolidWorksMonitor(){Stop();}
void SolidWorksMonitor::Start(HWND window,UINT message,Callback callback){notifyWindow_=window;attachMessage_=message;callback_=std::move(callback);gMonitor=this;EnumWindows([](HWND w,LPARAM context)->BOOL{if(IsSolidWorksWindow(w))reinterpret_cast<SolidWorksMonitor*>(context)->windows_.insert(w);return TRUE;},reinterpret_cast<LPARAM>(this));eventHook_=SetWinEventHook(EVENT_OBJECT_DESTROY,EVENT_OBJECT_SHOW,nullptr,WindowEvent,0,0,WINEVENT_OUTOFCONTEXT);Attach();}
void SolidWorksMonitor::Disconnect(){for(auto* item:subscriptions_)delete item;subscriptions_.clear();if(app_){app_->Release();app_=nullptr;}}
void SolidWorksMonitor::Stop(){if(eventHook_){UnhookWinEvent(eventHook_);eventHook_=nullptr;}Disconnect();windows_.clear();if(gMonitor==this)gMonitor=nullptr;callback_={};}
void CALLBACK SolidWorksMonitor::WindowEvent(HWINEVENTHOOK,DWORD event,HWND window,LONG object,LONG child,DWORD,DWORD){
 if(!gMonitor||object!=OBJID_WINDOW||child!=CHILDID_SELF)return;
 if(event==EVENT_OBJECT_DESTROY){if(gMonitor->windows_.erase(window))PostMessageW(gMonitor->notifyWindow_,gMonitor->attachMessage_,0,0);}
 else if(event==EVENT_OBJECT_SHOW&&GetAncestor(window,GA_ROOT)==window&&IsSolidWorksWindow(window)){
  gMonitor->windows_.insert(window);PostMessageW(gMonitor->notifyWindow_,gMonitor->attachMessage_,0,0);
 }
}
bool SolidWorksMonitor::RefreshConnection(){
 IDispatch* active{};const auto hr=(lookup_?lookup_:ActiveApplication)(&active);
 if(FAILED(hr)){
  if(hr==MK_E_UNAVAILABLE||hr==REGDB_E_CLASSNOTREG||hr==CO_E_CLASSSTRING){Disconnect();return true;}
  return false;
 }
 IUnknown *oldIdentity{},*newIdentity{};
 if(app_)app_->QueryInterface(IID_IUnknown,reinterpret_cast<void**>(&oldIdentity));
 if(active)active->QueryInterface(IID_IUnknown,reinterpret_cast<void**>(&newIdentity));
 const bool same=oldIdentity&&oldIdentity==newIdentity;
 if(oldIdentity)oldIdentity->Release();if(newIdentity)newIdentity->Release();
 if(same){active->Release();return true;}
 Disconnect();app_=active;if(app_)Subscribe(app_);return true;
}
void SolidWorksMonitor::Attach(){if(RefreshConnection()&&app_)EnumerateDocuments();}
bool SolidWorksMonitor::OpenDocuments(std::vector<std::wstring>& paths){
 paths.clear();if(!RefreshConnection())return false;if(!app_)return true;
 VARIANT value{};auto hr=ReadValue(app_,L"GetFirstDocument",value);
 if(FAILED(hr)){VariantClear(&value);return false;}
 IDispatch* doc=value.vt==VT_DISPATCH?value.pdispVal:nullptr;if(doc)doc->AddRef();VariantClear(&value);
 std::set<std::wstring> seen;
 while(doc){
  hr=ReadValue(doc,L"GetPathName",value);
  if(FAILED(hr)){VariantClear(&value);doc->Release();paths.clear();return false;}
  if(value.vt==VT_BSTR&&value.bstrVal){std::wstring path=value.bstrVal;if(!path.empty()){if(!seen.insert(path).second){VariantClear(&value);doc->Release();paths.clear();return false;}paths.push_back(std::move(path));}}
  VariantClear(&value);hr=ReadValue(doc,L"GetNext",value);doc->Release();
  if(FAILED(hr)){VariantClear(&value);paths.clear();return false;}
  doc=value.vt==VT_DISPATCH?value.pdispVal:nullptr;if(doc)doc->AddRef();VariantClear(&value);
 }
 // Release subscriptions to documents that have been closed; do this outside event callbacks.
 for(auto it=subscriptions_.begin();it!=subscriptions_.end();){auto* sub=*it;if(sub->document&&!sub->path.empty()&&!seen.contains(sub->path)){delete sub;it=subscriptions_.erase(it);}else ++it;}
 return true;
}
void SolidWorksMonitor::Subscribe(IDispatch* object,IDispatch* document){if(!object)return;if(document){const auto path=InvokeString(document,L"GetPathName");for(const auto* item:subscriptions_)if(item->document&&item->path==path)return;}IProvideClassInfo2* info{};IConnectionPointContainer* container{};GUID source{};if(FAILED(object->QueryInterface(IID_IProvideClassInfo2,reinterpret_cast<void**>(&info))))return;if(FAILED(info->GetGUID(GUIDKIND_DEFAULT_SOURCE_DISP_IID,&source))){info->Release();return;}info->Release();if(FAILED(object->QueryInterface(IID_IConnectionPointContainer,reinterpret_cast<void**>(&container))))return;IConnectionPoint* point{};if(FAILED(container->FindConnectionPoint(source,&point))){container->Release();return;}container->Release();ITypeInfo* type{};ITypeInfo* objectType{};UINT index{};ITypeLib* library{};if(SUCCEEDED(object->GetTypeInfo(0,LOCALE_USER_DEFAULT,&objectType))&&SUCCEEDED(objectType->GetContainingTypeLib(&library,&index))){library->GetTypeInfoOfGuid(source,&type);library->Release();objectType->Release();}auto* sub=new Subscription;sub->owner=this;sub->source=source;sub->point=point;sub->type=type;sub->document=document;if(document){document->AddRef();sub->path=InvokeString(document,L"GetPathName");}sub->sink=new Subscription::Sink;sub->sink->sub=sub;if(FAILED(point->Advise(sub->sink,&sub->cookie))){delete sub;return;}subscriptions_.push_back(sub);}
void SolidWorksMonitor::EnumerateDocuments(){IDispatch* doc=InvokeDocument(app_,L"GetFirstDocument");while(doc){const auto path=InvokeString(doc,L"GetPathName");Subscribe(doc,doc);if(callback_&&!path.empty())callback_({SolidWorksEventKind::Opened,path});IDispatch* next=InvokeDocument(doc,L"GetNext");doc->Release();doc=next;}}
void SolidWorksMonitor::Dispatch(Subscription* sub,DISPID id,DISPPARAMS* params){
 if(!callback_)return;BSTR raw{};UINT count{};std::wstring name;
 if(sub&&sub->type&&SUCCEEDED(sub->type->GetNames(id,&raw,1,&count))&&count){name=raw;SysFreeString(raw);}
 if(name.empty())return;
 // Opening can fail after a pre-notification. Only record confirmed opens.
 if(name==L"FileOpenPostNotify"){
  const auto path=FirstPath(params);IDispatch* doc=path.empty()?nullptr:InvokeDocument(app_,L"GetOpenDocumentByName",path);
  if(doc){Subscribe(doc,doc);callback_({SolidWorksEventKind::Opened,path});doc->Release();}
  PostMessageW(notifyWindow_,attachMessage_,0,0);return;
 }
 if(name.find(L"FileClose")!=std::wstring::npos||name.find(L"DestroyNotify")!=std::wstring::npos){
  auto path=FirstPath(params);if(path.empty()&&sub)path=sub->path;
  if(!path.empty())callback_({SolidWorksEventKind::Closed,path});
  // Shutdown can have no path. Reconcile outside the COM callback.
  PostMessageW(notifyWindow_,attachMessage_,0,0);return;
 }
 if(name.find(L"BeginInContextEdit")!=std::wstring::npos){auto path=FirstPath(params);if(!path.empty())callback_({SolidWorksEventKind::BeginContextEdit,path});return;}
 if(name.find(L"EndInContextEdit")!=std::wstring::npos){auto path=FirstPath(params);if(path.empty()&&sub)path=sub->path;if(!path.empty())callback_({SolidWorksEventKind::EndContextEdit,path});}
}
}