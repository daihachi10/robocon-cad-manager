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
    CreateWindowW(L"BUTTON", L"LFSロック設定を追加", WS_CHILD | WS_VISIBLE,
                  12, y + 52, 170, 28, window, reinterpret_cast<HMENU>(201), nullptr, nullptr);
    CreateWindowW(L"BUTTON", L"保存", WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON,
                  275, y + 52, 80, 28, window, reinterpret_cast<HMENU>(IDOK), nullptr, nullptr);
    CreateWindowW(L"BUTTON", L"キャンセル", WS_CHILD | WS_VISIBLE,
                  360, y + 52, 82, 28, window, reinterpret_cast<HMENU>(IDCANCEL), nullptr, nullptr);
    state->font = CreateJapaneseFont(window);
    ApplyJapaneseFont(window, state->font);
    return TRUE;
  }
  if (message == WM_COMMAND && (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL || LOWORD(wParam) == 201)) {
    if (LOWORD(wParam) == 201) state->fields->enableLfsSetup = true;
    if (LOWORD(wParam) == IDOK || LOWORD(wParam) == 201) {
      std::vector<std::wstring*> values = {&state->fields->name, &state->fields->remote, &state->fields->folder, &state->fields->userName, &state->fields->userEmail};
      for (size_t i = 0; i < values.size(); ++i) *values[i] = GetText(state->edits[i]);
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

DLGTEMPLATE* MakeTemplate(short height) {
  auto* templateData = static_cast<DLGTEMPLATE*>(LocalAlloc(LPTR, 128));
  templateData->style = WS_POPUP | WS_CAPTION | WS_SYSMENU | DS_MODALFRAME;
  templateData->x = 10;
  templateData->y = 10;
  templateData->cx = 470;
  templateData->cy = height;
  return templateData;
}

}

bool ShowSetupDialog(HWND owner, SetupFields& fields) {
  SetupState state{&fields};
  auto* templateData = MakeTemplate(370);
  const auto result = DialogBoxIndirectParamW(GetModuleHandleW(nullptr), templateData, owner, SetupProc, reinterpret_cast<LPARAM>(&state));
  LocalFree(templateData);
  return result != -1 && state.accepted;
}

std::optional<std::wstring> Prompt(HWND owner, const std::wstring& caption, const std::wstring& label, const std::wstring& initial) {
  std::wstring value = initial;
  PromptState state{&value, caption, label};
  auto* templateData = MakeTemplate(112);
  const auto result = DialogBoxIndirectParamW(GetModuleHandleW(nullptr), templateData, owner, PromptProc, reinterpret_cast<LPARAM>(&state));
  LocalFree(templateData);
  if (result == -1 || value.empty()) return {};
  return value;
}

}
