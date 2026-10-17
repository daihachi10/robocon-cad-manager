#pragma once
#include "common.h"
namespace rc { struct ProcessResult { DWORD exitCode{MAXDWORD}; std::wstring output, error; }; ProcessResult RunProcess(const std::filesystem::path& exe,const std::vector<std::wstring>& args,const std::filesystem::path& working={},const std::wstring& input={}); std::wstring QuoteArg(const std::wstring& value); }
