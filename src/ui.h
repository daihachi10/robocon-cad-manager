#pragma once
#include "common.h"
#include "git.h"
namespace rc {
struct SetupFields { std::wstring name,remote,folder,userName,userEmail; bool lfsLockEnabled{},enableLfsSetup{}; };
class OperationDialog {
 HWND owner_{},window_{},stage_{},progress_{},command_{},output_{},close_{};HFONT font_{};bool closable_{};
 static LRESULT CALLBACK Procedure(HWND,UINT,WPARAM,LPARAM);void Layout(UINT dpi=0);void ReleaseWindow();
 public: OperationDialog()=default;~OperationDialog();OperationDialog(const OperationDialog&)=delete;OperationDialog& operator=(const OperationDialog&)=delete;
 bool Show(HWND owner,const std::wstring& title,const std::wstring& stage,const std::wstring& command);bool Visible()const{return window_&&IsWindow(window_);}void SetStage(const std::wstring&);void Fail(const std::wstring& summary,const std::wstring& command,const std::wstring& details);void Close();
};
bool ShowSetupDialog(HWND owner,SetupFields& fields);std::optional<std::wstring> Prompt(HWND owner,const std::wstring&caption,const std::wstring&label,const std::wstring&initial=L"");std::optional<size_t> ShowHistoryDialog(HWND owner,const std::vector<GitRevision>& revisions,bool allowRestore);
}
