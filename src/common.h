#pragma once
#include <windows.h>
#include <commctrl.h>
#include <shellapi.h>
#include <shlobj.h>
#include <shlwapi.h>
#include <ole2.h>
#include <ocidl.h>
#include <process.h>
#include <filesystem>
#include <string>
#include <vector>
#include <map>
#include <optional>
#include <functional>
#include <thread>
#include <mutex>
#include <atomic>
#include <chrono>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <set>

namespace rc {
inline std::wstring LastError(DWORD code = GetLastError()) { wchar_t* b{}; FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER|FORMAT_MESSAGE_FROM_SYSTEM|FORMAT_MESSAGE_IGNORE_INSERTS,nullptr,code,0,reinterpret_cast<wchar_t*>(&b),0,nullptr); std::wstring s=b?b:L"不明なエラー"; if(b) LocalFree(b); return s; }
inline std::wstring Trim(std::wstring s) { const auto p=s.find_first_not_of(L" \t\r\n"); if(p==std::wstring::npos)return {}; const auto q=s.find_last_not_of(L" \t\r\n"); return s.substr(p,q-p+1); }
inline std::string Utf8(const std::wstring& v) { if(v.empty())return {}; int n=WideCharToMultiByte(CP_UTF8,0,v.data(),(int)v.size(),nullptr,0,nullptr,nullptr); std::string r(n,'\0'); WideCharToMultiByte(CP_UTF8,0,v.data(),(int)v.size(),r.data(),n,nullptr,nullptr); return r; }
inline std::wstring Wide(const std::string& v) { if(v.empty())return {}; int n=MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,v.data(),(int)v.size(),nullptr,0); if(!n)return {}; std::wstring r(n,L'\0'); MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,v.data(),(int)v.size(),r.data(),n); return r; }
inline bool IEndsWith(std::wstring s, std::wstring suffix) { if(s.size()<suffix.size())return false; return CompareStringOrdinal(s.data()+s.size()-suffix.size(),(int)suffix.size(),suffix.data(),(int)suffix.size(),TRUE)==CSTR_EQUAL; }
inline std::wstring GetText(HWND h) { int n=GetWindowTextLengthW(h); std::wstring s(static_cast<size_t>(n)+1,L'\0'); const int copied=GetWindowTextW(h,s.data(),n+1); s.resize(copied>0?static_cast<size_t>(copied):0); return s; }
inline void SetText(HWND h,const std::wstring& s){SetWindowTextW(h,s.c_str());}
}
