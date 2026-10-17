#pragma once
#include "common.h"
namespace rc { struct SetupFields { std::wstring name,remote,folder,userName,userEmail; bool enableLfsSetup{}; }; bool ShowSetupDialog(HWND owner,SetupFields& fields); std::optional<std::wstring> Prompt(HWND owner,const std::wstring&caption,const std::wstring&label,const std::wstring&initial=L""); }
