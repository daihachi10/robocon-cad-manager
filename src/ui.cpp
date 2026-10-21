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
    int y = 12;
    for (auto& [label, value] : rows) {
      CreateWindowW(L"STATIC", label, WS_CHILD | WS_VISIBLE, 12, y, 430, 18, window, nullptr, nullptr, nullptr);
      HWND edit = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", value->c_str(),
                                  WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_AUTOHSCROLL,
                                  12, y + 19, 430, 24, window, nullptr, nullptr, nullptr);
      state->edits.push_back(edit);
      y += 50;
    }
    CreateWindowW(L"STATIC", L"認証はGitの認証管理機能を使用します。アプリ用トークンは不要です。",
                  WS_CHILD | WS_VISIBLE, 12, y, 440, 18, window, nullptr, nullptr, nullptr);
    CreateWindowW(L"STATIC", L"変更はmainまたはorigin/mainへ直接送信します。",
                  WS_CHILD | WS_VISIBLE, 12, y + 22, 440, 18, window, nullptr, nullptr, nullptr);
    HWND locking = CreateWindowW(L"BUTTON", L"LFSロック機能を使用する（通常はオフ）", WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_AUTOCHECKBOX,
                  12, y + 48, 285, 24, window, reinterpret_cast<HMENU>(202), nullptr, nullptr);
    SendMessageW(locking, BM_SETCHECK, state->fields->lfsLockEnabled ? BST_CHECKED : BST_UNCHECKED, 0);
    CreateWindowW(L"BUTTON", L"LFSロック設定を追加して有効化", WS_CHILD | WS_VISIBLE,
                  12, y + 78, 220, 28, window, reinterpret_cast<HMENU>(201), nullptr, nullptr);
    CreateWindowW(L"BUTTON", L"保存", WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON,
                  275, y + 78, 80, 28, window, reinterpret_cast<HMENU>(IDOK), nullptr, nullptr);
    CreateWindowW(L"BUTTON", L"キャンセル", WS_CHILD | WS_VISIBLE,
                  360, y + 78, 82, 28, window, reinterpret_cast<HMENU>(IDCANCEL), nullptr, nullptr);
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
    CreateWindowW(L"STATIC", state->label.c_str(), WS_CHILD | WS_VISIBLE, 12, 12, 385, 19, window, nullptr, nullptr, nullptr);
    CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", state->value->c_str(),
                    WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_AUTOHSCROLL,
                    12, 35, 385, 24, window, reinterpret_cast<HMENU>(100), nullptr, nullptr);
    CreateWindowW(L"BUTTON", L"実行", WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON,
                  230, 68, 75, 27, window, reinterpret_cast<HMENU>(IDOK), nullptr, nullptr);
    CreateWindowW(L"BUTTON", L"キャンセル", WS_CHILD | WS_VISIBLE,
                  312, 68, 85, 27, window, reinterpret_cast<HMENU>(IDCANCEL), nullptr, nullptr);
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
    state->list = CreateWindowExW(WS_EX_CLIENTEDGE, WC_LISTVIEWW, L"", WS_CHILD|WS_VISIBLE|WS_TABSTOP|LVS_REPORT|LVS_SINGLESEL|LVS_SHOWSELALWAYS, 12, 12, 660, 210, window, reinterpret_cast<HMENU>(302), nullptr, nullptr);
    ListView_SetExtendedListViewStyle(state->list, LVS_EX_FULLROWSELECT|LVS_EX_DOUBLEBUFFER);
    const wchar_t* headings[] = {L"コミットメッセージ", L"日付", L"変更したユーザー"}; const int widths[] = {330, 140, 180};
    for (int i=0;i<3;++i) { LVCOLUMNW column{}; column.mask=LVCF_TEXT|LVCF_WIDTH; column.pszText=const_cast<LPWSTR>(headings[i]); column.cx=widths[i]; ListView_InsertColumn(state->list,i,&column); }
    for (size_t i=0;i<state->revisions->size();++i) { const auto& revision=(*state->revisions)[i]; LVITEMW item{}; item.mask=LVIF_TEXT|LVIF_PARAM; item.iItem=static_cast<int>(i); item.pszText=const_cast<LPWSTR>(revision.message.c_str()); item.lParam=static_cast<LPARAM>(i); ListView_InsertItem(state->list,&item); ListView_SetItemText(state->list,static_cast<int>(i),1,const_cast<LPWSTR>(revision.date.c_str())); ListView_SetItemText(state->list,static_cast<int>(i),2,const_cast<LPWSTR>(revision.author.c_str())); }
    if (state->revisions->empty()) CreateWindowW(L"STATIC", L"このファイルの履歴はありません。", WS_CHILD|WS_VISIBLE, 18, 32, 400, 24, window, nullptr, nullptr, nullptr);
    if (state->allowRestore) CreateWindowW(L"BUTTON", L"この状態に戻す", WS_CHILD|WS_VISIBLE|WS_TABSTOP|BS_DEFPUSHBUTTON, 470, 238, 105, 28, window, reinterpret_cast<HMENU>(301), nullptr, nullptr);
    CreateWindowW(L"BUTTON", L"閉じる", WS_CHILD|WS_VISIBLE|WS_TABSTOP, 585, 238, 85, 28, window, reinterpret_cast<HMENU>(IDCANCEL), nullptr, nullptr);
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

bool ShowSetupDialog(HWND owner, SetupFields& fields) {
  SetupState state{&fields};
  auto* templateData = MakeTemplateSize(470, 400);
  const auto result = DialogBoxIndirectParamW(GetModuleHandleW(nullptr), templateData, owner, SetupProc, reinterpret_cast<LPARAM>(&state));
  LocalFree(templateData);
  return result != -1 && state.accepted;
}

std::optional<std::wstring> Prompt(HWND owner, const std::wstring& caption, const std::wstring& label, const std::wstring& initial) {
  std::wstring value = initial;
  PromptState state{&value, caption, label};
  auto* templateData = MakeTemplateSize(470, 112);
  const auto result = DialogBoxIndirectParamW(GetModuleHandleW(nullptr), templateData, owner, PromptProc, reinterpret_cast<LPARAM>(&state));
  LocalFree(templateData);
  if (result == -1 || value.empty()) return {};
  return value;
}

std::optional<size_t> ShowHistoryDialog(HWND owner,const std::vector<GitRevision>& revisions,bool allowRestore) {
  HistoryState state{&revisions,allowRestore}; auto* templateData=MakeTemplateSize(700,285); DialogBoxIndirectParamW(GetModuleHandleW(nullptr),templateData,owner,HistoryProc,reinterpret_cast<LPARAM>(&state)); LocalFree(templateData); return state.selected;
}

}
