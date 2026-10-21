#pragma once
#include "common.h"
namespace rc {
// LFS locking is intentionally opt-in.  Repositories saved by older versions
// do not contain this value and therefore remain freely editable.
struct Repository { std::wstring name, remote, folder, userName, userEmail; bool lfsLockEnabled{}; };
struct Policy { int version{1}; std::wstring mainBranch{L"main"}, backupGuide; std::vector<std::wstring> leaders; };
class ConfigStore { std::filesystem::path root_; std::vector<Repository> repositories_; public: ConfigStore(); const std::vector<Repository>& Repositories()const{return repositories_;} bool Load(std::wstring& error); bool Save(std::wstring& error)const; void Upsert(Repository r); std::filesystem::path Root()const{return root_;} };
std::optional<Policy> ReadPolicy(const std::filesystem::path& repo,std::wstring& error);
}
