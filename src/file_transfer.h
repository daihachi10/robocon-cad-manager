#pragma once
#include "common.h"
namespace rc {
class FileDropTarget final : public IDropTarget {
 public:
  using EffectCallback = std::function<DWORD(POINTL)>;
  using DropCallback = std::function<void(IDataObject*,POINTL)>;
  FileDropTarget(HWND window,EffectCallback effect,DropCallback drop):window_(window),effect_(std::move(effect)),drop_(std::move(drop)){}
  HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid,void** value) override;
  ULONG STDMETHODCALLTYPE AddRef() override { return ++refs_; }
  ULONG STDMETHODCALLTYPE Release() override { const auto count=--refs_;if(!count)delete this;return count; }
  HRESULT STDMETHODCALLTYPE DragEnter(IDataObject*,DWORD,POINTL,DWORD*) override;
  HRESULT STDMETHODCALLTYPE DragOver(DWORD,POINTL,DWORD*) override;
  HRESULT STDMETHODCALLTYPE DragLeave() override;
  HRESULT STDMETHODCALLTYPE Drop(IDataObject*,DWORD,POINTL,DWORD*) override;
 private:
  ULONG refs_{1}; HWND window_{}; EffectCallback effect_; DropCallback drop_; bool acceptsFiles_{};
  void ClearHighlight();
};
std::vector<std::filesystem::path> ExtractDropFiles(IDataObject* object);
bool SetClipboardFiles(HWND owner,const std::vector<std::filesystem::path>& files,std::wstring& error);
std::vector<std::filesystem::path> ReadClipboardFiles(HWND owner);
}
