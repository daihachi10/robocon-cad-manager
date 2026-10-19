// Network smoke test only. The installer is downloaded/verified, never executed.
#include "update.cpp"
#include <iostream>

int wmain(){
 const auto result=rc::CheckForUpdate();
 if(!result.ok){std::wcerr<<result.error;return 1;}
 std::wcout<<L"Latest version: "<<result.info.version<<L"\n";
 std::filesystem::path installer;std::wstring error;
 if(!rc::DownloadAndVerifyUpdate(result.info,installer,error)){std::wcerr<<error;return 2;}
 if(std::filesystem::file_size(installer)!=result.info.size)return 3;
 if(!DeleteFileW(installer.c_str()))return 4;
 auto invalid=result.info;invalid.size=0;
 if(rc::DownloadAndVerifyUpdate(invalid,installer,error)||!installer.empty())return 5;
 // A tiny real response is sufficient to exercise hash rejection and partial-file cleanup.
 std::vector<BYTE> manifest;
 if(!rc::HttpGet(rc::kManifestUrl,manifest,error))return 6;
 auto badHash=result.info;badHash.installerUrl=rc::kManifestUrl;badHash.size=manifest.size();badHash.version=L"0.0.0";badHash.sha256=std::wstring(64,L'0');
 if(rc::DownloadAndVerifyUpdate(badHash,installer,error)||!installer.empty()||error.find(L"SHA-256")==std::wstring::npos)return 7;
 std::wcout<<L"PASS: streamed "<<result.info.size<<L" bytes, verified SHA-256, removed test download. Installer not executed.\n";
 return 0;
}
