#pragma once
#include "config.h"
namespace rc { struct CadFile { std::filesystem::path path; uintmax_t bytes{}; bool lfsPointer{}, suspiciousName{},directory{}; }; std::vector<CadFile> ScanCad(const Repository&r); bool OpenFile(const std::filesystem::path&p,std::wstring&error); bool MakeCadWritable(const std::filesystem::path& path,std::wstring& error); std::vector<std::wstring> ConflictingFiles(const std::filesystem::path&repo); }
