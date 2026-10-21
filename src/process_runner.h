#pragma once
#include "common.h"
namespace rc { struct ProcessResult { DWORD exitCode{}; std::wstring output, error, command; }; ProcessResult RunProcess(const std::filesystem::path& exe,const std::vector<std::wstring>& args,const std::filesystem::path& working={},const std::wstring& input={}); ProcessResult RunProcessToFile(const std::filesystem::path& exe,const std::vector<std::wstring>& args,const std::filesystem::path& working,const std::filesystem::path& outputFile); std::wstring QuoteArg(const std::wstring& value); }
