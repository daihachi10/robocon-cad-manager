#include "ui.h"

namespace rc {
namespace {

HFONT CreateJapaneseFont(HWND window) {
  const UINT dpi = GetDpiForWindow(window);
  return CreateFontW(-MulDiv(10, static_cast<int>(dpi), 72), 0, 0, 0, FW_NORMAL,
                     FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                     CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                     DEFAULT_PITCH | FF_DONTCARE, L"Yu Gothic UI");
}

BOOL CALLBACK SetChildFont(HWND child, LPARAM font) {
  SendMessageW(child, WM_SETFONT, font, TRUE);
  return TRUE;
}

void ApplyJapaneseFont(HWND window, HFONT font) {
  SendMessageW(window, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);
  EnumChildWindows(window, SetChildFont, reinterpret_cast<LPARAM>(font));
}

int ScaleForDpi(int value,UINT dpi){return MulDiv(value,static_cast<int>(dpi),96);}

void FitAndCenterDialog(HWND window,int clientWidth,int clientHeight) {
  const UINT dpi=GetDpiForWindow(window);std::vector<std::pair<HWND,RECT>> children;
  struct ChildList{HWND parent;std::vector<std::pair<HWND,RECT>>* items;} childList{window,&children};
  EnumChildWindows(window,[](HWND child,LPARAM value){auto* list=reinterpret_cast<ChildList*>(value);if(GetParent(child)!=list->parent)return TRUE;RECT rect{};GetWindowRect(child,&rect);MapWindowPoints(nullptr,list->parent,reinterpret_cast<POINT*>(&rect),2);list->items->push_back({child,rect});return TRUE;},reinterpret_cast<LPARAM>(&childList));
  RECT outer{0,0,ScaleForDpi(clientWidth,dpi),ScaleForDpi(clientHeight,dpi)};AdjustWindowRectExForDpi(&outer,static_cast<DWORD>(GetWindowLongPtrW(window,GWL_STYLE)),FALSE,static_cast<DWORD>(GetWindowLongPtrW(window,GWL_EXSTYLE)),dpi);
  const int width=outer.right-outer.left,height=outer.bottom-outer.top;RECT anchor{};HWND owner=GetWindow(window,GW_OWNER);if(!owner||!GetWindowRect(owner,&anchor)){SystemParametersInfoW(SPI_GETWORKAREA,0,&anchor,0);}int x=anchor.left+((anchor.right-anchor.left)-width)/2,y=anchor.top+((anchor.bottom-anchor.top)-height)/2;SetWindowPos(window,nullptr,x,y,width,height,SWP_NOZORDER|SWP_NOACTIVATE);
  for(const auto& [child,rect]:children)MoveWindow(child,ScaleForDpi(rect.left,dpi),ScaleForDpi(rect.top,dpi),ScaleForDpi(rect.right-rect.left,dpi),ScaleForDpi(rect.bottom-rect.top,dpi),FALSE);
}

struct SetupState {
  SetupFields* fields{};
  std::vector<HWND> edits;
  HFONT font{};
  bool accepted{};
};

struct PromptState {
  std::wstring* value{};
  std::wstring caption;
  std::wstring label;
  HFONT font{};
};

struct HistoryState { const std::vector<GitRevision>* revisions{}; bool allowRestore{}; std::optional<size_t> selected; HFONT font{}; HWND list{}; };

INT_PTR CALLBACK SetupProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam) {
  auto* state = reinterpret_cast<SetupState*>(GetWindowLongPtrW(window, GWLP_USERDATA));
  if (message == WM_INITDIALOG) {
    state = reinterpret_cast<SetupState*>(lParam);
    SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(state));
    SetWindowTextW(window, L"設定");
    std::vector<std::pair<const wchar_t*, std::wstring*>> rows = {
        {L"表示名", &state->fields->name},
        {L"GitHubリポジトリURL または 所有者/リポジトリ", &state->fields->remote},
        {L"作業フォルダ", &state->fields->folder},
        {L"変更記録に使う氏名", &state->fields->userName},
        {L"変更記録に使うメールアドレス", &state->fields->userEmail},
    };
    int y = 14;
    for (auto& [label, value] : rows) {
      CreateWindowW(L"STATIC", label, WS_CHILD | WS_VISIBLE | SS_CENTERIMAGE, 14, y, 490, 22, window, nullptr, nullptr, nullptr);
      HWND edit = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", value->c_str(),
                                  WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_AUTOHSCROLL,
                                  14, y + 24, 490, 30, window, nullptr, nullptr, nullptr);
      state->edits.push_back(edit);
      y += 58;
    }
    CreateWindowW(L"STATIC", L"認証はGitの認証管理機能を使用します。アプリ用トークンは不要です。",
                  WS_CHILD | WS_VISIBLE | SS_CENTERIMAGE, 14, y, 490, 22, window, nullptr, nullptr, nullptr);
    CreateWindowW(L"STATIC", L"変更はmainまたはorigin/mainへ直接送信します。",
                  WS_CHILD | WS_VISIBLE | SS_CENTERIMAGE, 14, y + 24, 490, 22, window, nullptr, nullptr, nullptr);
    HWND locking = CreateWindowW(L"BUTTON", L"LFSロック機能を使用する（通常はオフ）", WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_AUTOCHECKBOX,
                  14, y + 52, 300, 28, window, reinterpret_cast<HMENU>(202), nullptr, nullptr);
    SendMessageW(locking, BM_SETCHECK, state->fields->lfsLockEnabled ? BST_CHECKED : BST_UNCHECKED, 0);
    CreateWindowW(L"BUTTON", L"LFSロック設定を追加して有効化", WS_CHILD | WS_VISIBLE,
                  14, y + 86, 240, 32, window, reinterpret_cast<HMENU>(201), nullptr, nullptr);
    CreateWindowW(L"BUTTON", L"保存", WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON,
                  326, y + 86, 82, 32, window, reinterpret_cast<HMENU>(IDOK), nullptr, nullptr);
    CreateWindowW(L"BUTTON", L"キャンセル", WS_CHILD | WS_VISIBLE,
                  416, y + 86, 88, 32, window, reinterpret_cast<HMENU>(IDCANCEL), nullptr, nullptr);
    FitAndCenterDialog(window, 520, 430);
    state->font = CreateJapaneseFont(window);
    ApplyJapaneseFont(window, state->font);
    return TRUE;
  }
  if (message == WM_COMMAND && (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL || LOWORD(wParam) == 201)) {
    if (LOWORD(wParam) == 201) { state->fields->enableLfsSetup = true; state->fields->lfsLockEnabled = true; }
    if (LOWORD(wParam) == IDOK || LOWORD(wParam) == 201) {
      std::vector<std::wstring*> values = {&state->fields->name, &state->fields->remote, &state->fields->folder, &state->fields->userName, &state->fields->userEmail};
      for (size_t i = 0; i < values.size(); ++i) *values[i] = GetText(state->edits[i]);
      if (LOWORD(wParam) == IDOK) state->fields->lfsLockEnabled = SendMessageW(GetDlgItem(window, 202), BM_GETCHECK, 0, 0) == BST_CHECKED;
      state->accepted = true;
    }
    EndDialog(window, 0);
    return TRUE;
  }
  if (message == WM_DESTROY && state && state->font) {
    DeleteObject(state->font);
    state->font = nullptr;
  }
  return FALSE;
}

INT_PTR CALLBACK PromptProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam) {
  auto* state = reinterpret_cast<PromptState*>(GetWindowLongPtrW(window, GWLP_USERDATA));
  if (message == WM_INITDIALOG) {
    state = reinterpret_cast<PromptState*>(lParam);
    SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(state));
    SetWindowTextW(window, state->caption.c_str());
    CreateWindowW(L"STATIC", state->label.c_str(), WS_CHILD | WS_VISIBLE | SS_CENTERIMAGE, 14, 12, 432, 22, window, nullptr, nullptr, nullptr);
    CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", state->value->c_str(),
                    WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_AUTOHSCROLL,
                    14, 38, 432, 30, window, reinterpret_cast<HMENU>(100), nullptr, nullptr);
    CreateWindowW(L"BUTTON", L"実行", WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON,
                  274, 82, 80, 32, window, reinterpret_cast<HMENU>(IDOK), nullptr, nullptr);
    CreateWindowW(L"BUTTON", L"キャンセル", WS_CHILD | WS_VISIBLE,
                  362, 82, 84, 32, window, reinterpret_cast<HMENU>(IDCANCEL), nullptr, nullptr);
    FitAndCenterDialog(window, 460, 130);
    state->font = CreateJapaneseFont(window);
    ApplyJapaneseFont(window, state->font);
    return TRUE;
  }
  if (message == WM_COMMAND && (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL)) {
    if (LOWORD(wParam) == IDOK) *state->value = GetText(GetDlgItem(window, 100));
    else state->value->clear();
    EndDialog(window, 0);
    return TRUE;
  }
  if (message == WM_DESTROY && state && state->font) {
    DeleteObject(state->font);
    state->font = nullptr;
  }
  return FALSE;
}

INT_PTR CALLBACK HistoryProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam) {
  auto* state = reinterpret_cast<HistoryState*>(GetWindowLongPtrW(window, GWLP_USERDATA));
  if (message == WM_INITDIALOG) {
    state = reinterpret_cast<HistoryState*>(lParam); SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(state));
    SetWindowTextW(window, state->allowRestore ? L"以前の状態に戻す" : L"過去の変更履歴");
    state->list = CreateWindowExW(WS_EX_CLIENTEDGE, WC_LISTVIEWW, L"", WS_CHILD|WS_VISIBLE|WS_TABSTOP|LVS_REPORT|LVS_SINGLESEL|LVS_SHOWSELALWAYS, 14, 14, 692, 240, window, reinterpret_cast<HMENU>(302), nullptr, nullptr);
    ListView_SetExtendedListViewStyle(state->list, LVS_EX_FULLROWSELECT|LVS_EX_DOUBLEBUFFER);
    const wchar_t* headings[] = {L"コミットメッセージ", L"日付", L"変更したユーザー"}; const int widths[] = {350, 150, 180};const UINT dpi=GetDpiForWindow(window);
    for (int i=0;i<3;++i) { LVCOLUMNW column{}; column.mask=LVCF_TEXT|LVCF_WIDTH; column.pszText=const_cast<LPWSTR>(headings[i]); column.cx=ScaleForDpi(widths[i],dpi); ListView_InsertColumn(state->list,i,&column); }
    for (size_t i=0;i<state->revisions->size();++i) { const auto& revision=(*state->revisions)[i]; LVITEMW item{}; item.mask=LVIF_TEXT|LVIF_PARAM; item.iItem=static_cast<int>(i); item.pszText=const_cast<LPWSTR>(revision.message.c_str()); item.lParam=static_cast<LPARAM>(i); ListView_InsertItem(state->list,&item); ListView_SetItemText(state->list,static_cast<int>(i),1,const_cast<LPWSTR>(revision.date.c_str())); ListView_SetItemText(state->list,static_cast<int>(i),2,const_cast<LPWSTR>(revision.author.c_str())); }
    if (state->revisions->empty()) CreateWindowW(L"STATIC", L"このファイルの履歴はありません。", WS_CHILD|WS_VISIBLE, 20, 36, 430, 26, window, nullptr, nullptr, nullptr);
    if (state->allowRestore) CreateWindowW(L"BUTTON", L"この状態に戻す", WS_CHILD|WS_VISIBLE|WS_TABSTOP|BS_DEFPUSHBUTTON, 486, 270, 118, 34, window, reinterpret_cast<HMENU>(301), nullptr, nullptr);
    CreateWindowW(L"BUTTON", L"閉じる", WS_CHILD|WS_VISIBLE|WS_TABSTOP, 612, 270, 94, 34, window, reinterpret_cast<HMENU>(IDCANCEL), nullptr, nullptr);
    FitAndCenterDialog(window,720,320);
    state->font=CreateJapaneseFont(window); ApplyJapaneseFont(window,state->font); return TRUE;
  }
  if (message==WM_COMMAND && (LOWORD(wParam)==IDCANCEL || LOWORD(wParam)==301)) {
    if (LOWORD(wParam)==301 && state->allowRestore && state->list) { const int row=ListView_GetNextItem(state->list,-1,LVNI_SELECTED); if(row>=0 && row<static_cast<int>(state->revisions->size())) state->selected=static_cast<size_t>(row); else { MessageBoxW(window,L"戻す履歴を選択してください。",L"履歴を選択してください",MB_ICONINFORMATION); return TRUE; } }
    EndDialog(window,0); return TRUE;
  }
  if (message==WM_DESTROY && state && state->font) { DeleteObject(state->font); state->font=nullptr; }
  return FALSE;
}

DLGTEMPLATE* MakeTemplateSize(short width,short height) {
  auto* templateData = static_cast<DLGTEMPLATE*>(LocalAlloc(LPTR, 128));
  templateData->style = WS_POPUP | WS_CAPTION | WS_SYSMENU | DS_MODALFRAME;
  templateData->x = 10;
  templateData->y = 10;
  templateData->cx = width;
  templateData->cy = height;
  return templateData;
}

}

LRESULT CALLBACK OperationDialog::Procedure(HWND window,UINT message,WPARAM wParam,LPARAM lParam){
 auto* self=reinterpret_cast<OperationDialog*>(GetWindowLongPtrW(window,GWLP_USERDATA));
 if(message==WM_NCCREATE){self=reinterpret_cast<OperationDialog*>(reinterpret_cast<CREATESTRUCTW*>(lParam)->lpCreateParams);SetWindowLongPtrW(window,GWLP_USERDATA,reinterpret_cast<LONG_PTR>(self));self->window_=window;}
 if(!self)return DefWindowProcW(window,message,wParam,lParam);
 if(message==WM_CLOSE){if(self->closable_)self->Close();return 0;}
 if(message==WM_COMMAND&&LOWORD(wParam)==1){if(self->closable_)self->Close();return 0;}
 if(message==WM_DPICHANGED){const auto* rect=reinterpret_cast<RECT*>(lParam);SetWindowPos(window,nullptr,rect->left,rect->top,rect->right-rect->left,rect->bottom-rect->top,SWP_NOZORDER|SWP_NOACTIVATE);self->Layout(HIWORD(wParam));return 0;}
 if(message==WM_NCDESTROY){self->ReleaseWindow();SetWindowLongPtrW(window,GWLP_USERDATA,0);return 0;}
 return DefWindowProcW(window,message,wParam,lParam);
}

OperationDialog::~OperationDialog(){Close();if(font_)DeleteObject(font_);}

void OperationDialog::Layout(UINT dpi){
 if(!window_)return;if(!dpi)dpi=GetDpiForWindow(window_);auto s=[&](int value){return ScaleForDpi(value,dpi);};
 if(font_)DeleteObject(font_);font_=CreateJapaneseFont(window_);EnumChildWindows(window_,SetChildFont,reinterpret_cast<LPARAM>(font_));
 RECT client{};GetClientRect(window_,&client);const int margin=s(16),width=client.right-margin*2;
 MoveWindow(stage_,margin,s(14),width,s(28),TRUE);MoveWindow(progress_,margin,s(48),width,s(18),TRUE);
 MoveWindow(GetDlgItem(window_,10),margin,s(78),width,s(22),TRUE);MoveWindow(command_,margin,s(102),width,s(70),TRUE);
 MoveWindow(GetDlgItem(window_,11),margin,s(184),width,s(22),TRUE);MoveWindow(output_,margin,s(208),width,s(150),TRUE);
 MoveWindow(close_,client.right-margin-s(100),s(375),s(100),s(36),TRUE);
}

bool OperationDialog::Show(HWND owner,const std::wstring& title,const std::wstring& stage,const std::wstring& command){
 static std::once_flag registration;std::call_once(registration,[]{WNDCLASSEXW cls{sizeof(cls)};cls.hInstance=GetModuleHandleW(nullptr);cls.lpfnWndProc=OperationDialog::Procedure;cls.lpszClassName=L"RoboconCadManager.Operation";cls.hCursor=LoadCursorW(nullptr,IDC_ARROW);cls.hbrBackground=reinterpret_cast<HBRUSH>(COLOR_WINDOW+1);RegisterClassExW(&cls);});
 owner_=owner;closable_=false;const UINT dpi=owner?GetDpiForWindow(owner):96;RECT outer{0,0,ScaleForDpi(720,dpi),ScaleForDpi(430,dpi)};AdjustWindowRectExForDpi(&outer,WS_POPUP|WS_CAPTION|WS_SYSMENU,FALSE,WS_EX_DLGMODALFRAME,dpi);RECT anchor{};if(!owner||!GetWindowRect(owner,&anchor))SystemParametersInfoW(SPI_GETWORKAREA,0,&anchor,0);const int width=outer.right-outer.left,height=outer.bottom-outer.top,x=anchor.left+((anchor.right-anchor.left)-width)/2,y=anchor.top+((anchor.bottom-anchor.top)-height)/2;
 window_=CreateWindowExW(WS_EX_DLGMODALFRAME,L"RoboconCadManager.Operation",title.c_str(),WS_POPUP|WS_CAPTION|WS_SYSMENU,x,y,width,height,owner,nullptr,GetModuleHandleW(nullptr),this);if(!window_)return false;
 stage_=CreateWindowW(L"STATIC",stage.c_str(),WS_CHILD|WS_VISIBLE|SS_CENTERIMAGE,0,0,0,0,window_,nullptr,nullptr,nullptr);progress_=CreateWindowW(PROGRESS_CLASSW,L"",WS_CHILD|WS_VISIBLE|PBS_MARQUEE,0,0,0,0,window_,nullptr,nullptr,nullptr);
 CreateWindowW(L"STATIC",L"実行内容（デバッグ用）",WS_CHILD|WS_VISIBLE|SS_CENTERIMAGE,0,0,0,0,window_,reinterpret_cast<HMENU>(10),nullptr,nullptr);command_=CreateWindowExW(WS_EX_CLIENTEDGE,L"EDIT",command.c_str(),WS_CHILD|WS_VISIBLE|ES_MULTILINE|ES_READONLY|ES_AUTOVSCROLL|WS_VSCROLL,0,0,0,0,window_,nullptr,nullptr,nullptr);
 CreateWindowW(L"STATIC",L"出力・エラー",WS_CHILD|WS_VISIBLE|SS_CENTERIMAGE,0,0,0,0,window_,reinterpret_cast<HMENU>(11),nullptr,nullptr);output_=CreateWindowExW(WS_EX_CLIENTEDGE,L"EDIT",L"処理中です。完了までお待ちください。",WS_CHILD|WS_VISIBLE|ES_MULTILINE|ES_READONLY|ES_AUTOVSCROLL|WS_VSCROLL,0,0,0,0,window_,nullptr,nullptr,nullptr);close_=CreateWindowW(L"BUTTON",L"処理中…",WS_CHILD|WS_VISIBLE|WS_TABSTOP,0,0,0,0,window_,reinterpret_cast<HMENU>(1),nullptr,nullptr);EnableWindow(close_,FALSE);
 SendMessageW(progress_,PBM_SETMARQUEE,TRUE,35);Layout(dpi);if(owner)EnableWindow(owner,FALSE);ShowWindow(window_,owner&&IsWindowVisible(owner)?SW_SHOWNORMAL:SW_HIDE);UpdateWindow(window_);return true;
}

void OperationDialog::SetStage(const std::wstring& value){if(stage_)SetWindowTextW(stage_,value.c_str());}
void OperationDialog::Fail(const std::wstring& summary,const std::wstring& command,const std::wstring& details){if(!Visible())return;SetWindowTextW(window_,L"処理に失敗しました");SetWindowTextW(stage_,summary.c_str());if(!command.empty())SetWindowTextW(command_,command.c_str());SetWindowTextW(output_,details.c_str());SendMessageW(progress_,PBM_SETMARQUEE,FALSE,0);SendMessageW(progress_,PBM_SETSTATE,PBST_ERROR,0);SetWindowTextW(close_,L"閉じる");EnableWindow(close_,TRUE);closable_=true;SetFocus(close_);}
void OperationDialog::Close(){if(Visible())DestroyWindow(window_);}
void OperationDialog::ReleaseWindow(){HWND owner=owner_;window_=stage_=progress_=command_=output_=close_=nullptr;closable_=false;if(owner&&IsWindow(owner)){EnableWindow(owner,TRUE);SetForegroundWindow(owner);}owner_=nullptr;}

bool ShowSetupDialog(HWND owner, SetupFields& fields) {
  SetupState state{&fields};
  auto* templateData = MakeTemplateSize(300, 240);
  const auto result = DialogBoxIndirectParamW(GetModuleHandleW(nullptr), templateData, owner, SetupProc, reinterpret_cast<LPARAM>(&state));
  LocalFree(templateData);
  return result != -1 && state.accepted;
}

std::optional<std::wstring> Prompt(HWND owner, const std::wstring& caption, const std::wstring& label, const std::wstring& initial) {
  std::wstring value = initial;
  PromptState state{&value, caption, label};
  auto* templateData = MakeTemplateSize(280, 90);
  const auto result = DialogBoxIndirectParamW(GetModuleHandleW(nullptr), templateData, owner, PromptProc, reinterpret_cast<LPARAM>(&state));
  LocalFree(templateData);
  if (result == -1 || value.empty()) return {};
  return value;
}

std::optional<size_t> ShowHistoryDialog(HWND owner,const std::vector<GitRevision>& revisions,bool allowRestore) {
  HistoryState state{&revisions,allowRestore}; auto* templateData=MakeTemplateSize(430,210); DialogBoxIndirectParamW(GetModuleHandleW(nullptr),templateData,owner,HistoryProc,reinterpret_cast<LPARAM>(&state)); LocalFree(templateData); return state.selected;
}

}
