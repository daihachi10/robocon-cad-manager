#include "file_transfer.h"
namespace rc {
HRESULT STDMETHODCALLTYPE FileDropTarget::QueryInterface(REFIID iid,void** value){if(!value)return E_POINTER;if(iid==IID_IUnknown||iid==IID_IDropTarget){*value=this;AddRef();return S_OK;}*value=nullptr;return E_NOINTERFACE;}
void FileDropTarget::ClearHighlight(){if(IsWindow(window_))ListView_SetItemState(window_,-1,0,LVIS_DROPHILITED);}
HRESULT STDMETHODCALLTYPE FileDropTarget::DragEnter(IDataObject* object,DWORD,POINTL point,DWORD* effect){
 if(!effect)return E_POINTER;FORMATETC format{CF_HDROP,nullptr,DVASPECT_CONTENT,-1,TYMED_HGLOBAL};acceptsFiles_=object&&SUCCEEDED(object->QueryGetData(&format));
 *effect=acceptsFiles_&&effect_?(*effect&effect_(point)):DROPEFFECT_NONE;return S_OK;
}
HRESULT STDMETHODCALLTYPE FileDropTarget::DragOver(DWORD,POINTL point,DWORD* effect){if(!effect)return E_POINTER;*effect=acceptsFiles_&&effect_?(*effect&effect_(point)):DROPEFFECT_NONE;return S_OK;}
HRESULT STDMETHODCALLTYPE FileDropTarget::DragLeave(){acceptsFiles_=false;ClearHighlight();return S_OK;}
HRESULT STDMETHODCALLTYPE FileDropTarget::Drop(IDataObject* object,DWORD,POINTL point,DWORD* effect){
 if(!effect)return E_POINTER;*effect=acceptsFiles_&&effect_?(*effect&effect_(point)):DROPEFFECT_NONE;acceptsFiles_=false;ClearHighlight();
 try{if(*effect!=DROPEFFECT_NONE&&drop_)drop_(object,point);}catch(...){*effect=DROPEFFECT_NONE;return E_FAIL;}return S_OK;
}
std::vector<std::filesystem::path> ExtractDropFiles(IDataObject* object){std::vector<std::filesystem::path> result;if(!object)return result;FORMATETC format{CF_HDROP,nullptr,DVASPECT_CONTENT,-1,TYMED_HGLOBAL};STGMEDIUM medium{};if(FAILED(object->GetData(&format,&medium)))return result;auto drop=static_cast<HDROP>(medium.hGlobal);if(drop){const auto count=DragQueryFileW(drop,0xFFFFFFFF,nullptr,0);for(UINT i=0;i<count;++i){wchar_t path[32768];const auto length=DragQueryFileW(drop,i,path,static_cast<UINT>(std::size(path)));if(length)result.emplace_back(path);}}ReleaseStgMedium(&medium);return result;}
bool SetClipboardFiles(HWND owner,const std::vector<std::filesystem::path>& files,std::wstring& error){if(files.empty()){error=L"コピーするファイルを選択してください。";return false;}size_t chars=sizeof(DROPFILES);for(const auto& file:files)chars+=(file.wstring().size()+1)*sizeof(wchar_t);chars+=sizeof(wchar_t);HGLOBAL memory=GlobalAlloc(GMEM_MOVEABLE|GMEM_ZEROINIT,chars);if(!memory){error=LastError();return false;}auto* drop=static_cast<DROPFILES*>(GlobalLock(memory));drop->pFiles=sizeof(DROPFILES);drop->fWide=TRUE;auto* text=reinterpret_cast<wchar_t*>(reinterpret_cast<BYTE*>(drop)+sizeof(DROPFILES));for(const auto& file:files){const auto value=file.wstring();std::copy(value.begin(),value.end(),text);text+=value.size()+1;}GlobalUnlock(memory);if(!OpenClipboard(owner)){GlobalFree(memory);error=LastError();return false;}EmptyClipboard();if(!SetClipboardData(CF_HDROP,memory)){CloseClipboard();GlobalFree(memory);error=LastError();return false;}CloseClipboard();return true;}
std::vector<std::filesystem::path> ReadClipboardFiles(HWND owner){std::vector<std::filesystem::path> result;if(!OpenClipboard(owner))return result;if(IsClipboardFormatAvailable(CF_HDROP)){auto handle=GetClipboardData(CF_HDROP);if(handle){auto* drop=static_cast<HDROP>(handle);const auto count=DragQueryFileW(drop,0xFFFFFFFF,nullptr,0);for(UINT i=0;i<count;++i){wchar_t path[32768];const auto length=DragQueryFileW(drop,i,path,static_cast<UINT>(std::size(path)));if(length)result.emplace_back(path);}}}CloseClipboard();return result;}
}
