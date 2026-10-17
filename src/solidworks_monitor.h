#pragma once
#include "common.h"
namespace rc {
enum class SolidWorksEventKind { Opened, Closed, BeginContextEdit, EndContextEdit };
struct SolidWorksEvent { SolidWorksEventKind kind{}; std::wstring path; };
class SolidWorksMonitor {
 public:
  using Callback = std::function<void(const SolidWorksEvent&)>;
  SolidWorksMonitor() = default;
  ~SolidWorksMonitor();
  void Start(HWND notifyWindow, UINT attachMessage, Callback callback);
  void Stop();
  void Attach();
  std::vector<std::wstring> OpenDocuments() const;
 private:
  struct Subscription;
  HWND notifyWindow_{}; UINT attachMessage_{}; HWINEVENTHOOK eventHook_{}; Callback callback_;
  IDispatch* app_{}; std::vector<Subscription*> subscriptions_;
  void Subscribe(IDispatch* object, IDispatch* document = nullptr);
  void EnumerateDocuments();
  void Dispatch(Subscription* subscription, DISPID dispid, DISPPARAMS* params);
  static void CALLBACK WindowEvent(HWINEVENTHOOK, DWORD, HWND, LONG, LONG, DWORD, DWORD);
};
}
