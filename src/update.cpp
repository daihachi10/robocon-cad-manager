#include "update.h"
#include <winhttp.h>
#include <bcrypt.h>

#pragma comment(lib, "winhttp.lib")
#pragma comment(lib, "bcrypt.lib")

namespace rc {
namespace {
constexpr wchar_t kManifestUrl[] = L"https://github.com/daihachi10/robocon-cad-manager/releases/latest/download/update.json";

bool ParseHttpsUrl(const std::wstring& value, URL_COMPONENTS& components, std::wstring& error) {
  components = {};
  components.dwStructSize = sizeof(components);
  if (!WinHttpCrackUrl(value.c_str(), 0, 0, &components) || components.nScheme != INTERNET_SCHEME_HTTPS) {
    error = L"更新先URLがHTTPSではありません。";
    return false;
  }
  return true;
}

bool HttpGet(const std::wstring& url, std::vector<BYTE>& body, std::wstring& error) {
  URL_COMPONENTS parts{};
  if (!ParseHttpsUrl(url, parts, error)) return false;
  std::wstring host(parts.lpszHostName, parts.dwHostNameLength);
  std::wstring path(parts.lpszUrlPath, parts.dwUrlPathLength);
  if (parts.dwExtraInfoLength) path.append(parts.lpszExtraInfo, parts.dwExtraInfoLength);
  HINTERNET session = WinHttpOpen(L"RoboconCadManager/0.1", WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY, nullptr, nullptr, 0);
  if (!session) { error = L"更新確認の通信を開始できません。"; return false; }
  WinHttpSetTimeouts(session, 5000, 5000, 10000, 10000);
  HINTERNET connection = WinHttpConnect(session, host.c_str(), parts.nPort, 0);
  HINTERNET request = connection ? WinHttpOpenRequest(connection, L"GET", path.c_str(), nullptr,
      WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE) : nullptr;
  bool result = false;
  if (request && WinHttpSendRequest(request, WINHTTP_NO_ADDITIONAL_HEADERS, 0, nullptr, 0, 0, 0) && WinHttpReceiveResponse(request, nullptr)) {
    DWORD status{}, statusSize = sizeof(status);
    WinHttpQueryHeaders(request, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, WINHTTP_HEADER_NAME_BY_INDEX, &status, &statusSize, WINHTTP_NO_HEADER_INDEX);
    if (status >= 200 && status < 300) {
      for (;;) {
        DWORD available{};
        if (!WinHttpQueryDataAvailable(request, &available)) break;
        if (!available) { result = true; break; }
        const auto old = body.size(); body.resize(old + available);
        DWORD read{};
        if (!WinHttpReadData(request, body.data() + old, available, &read)) { body.resize(old); break; }
        body.resize(old + read);
      }
    } else error = L"更新サーバーがHTTP "+std::to_wstring(status)+L"を返しました。";
  }
  if (!result && error.empty()) error = L"更新情報を取得できません。ネットワーク接続を確認してください。";
  if (request) WinHttpCloseHandle(request); if (connection) WinHttpCloseHandle(connection); WinHttpCloseHandle(session);
  return result;
}

bool Sha256(const std::filesystem::path& file, std::wstring& digest, std::wstring& error) {
  BCRYPT_ALG_HANDLE algorithm{}; BCRYPT_HASH_HANDLE hash{};
  DWORD objectSize{}, dataSize{};
  if (BCryptOpenAlgorithmProvider(&algorithm, BCRYPT_SHA256_ALGORITHM, nullptr, 0) < 0 ||
      BCryptGetProperty(algorithm, BCRYPT_OBJECT_LENGTH, reinterpret_cast<PUCHAR>(&objectSize), sizeof(objectSize), &dataSize, 0) < 0) {
    error = L"SHA-256検証を初期化できません。"; if (algorithm) BCryptCloseAlgorithmProvider(algorithm, 0); return false;
  }
  std::vector<BYTE> object(objectSize), result(32); if (BCryptCreateHash(algorithm, &hash, object.data(), objectSize, nullptr, 0, 0) < 0) { error=L"SHA-256検証を初期化できません。"; BCryptCloseAlgorithmProvider(algorithm,0); return false; }
  std::ifstream stream(file, std::ios::binary); std::vector<BYTE> buffer(1024*1024);
  if (!stream) error=L"更新ファイルを開けません。";
  while (stream && error.empty()) {
    stream.read(reinterpret_cast<char*>(buffer.data()), buffer.size());
    const auto count = stream.gcount();
    if (count > 0 && BCryptHashData(hash, buffer.data(), static_cast<ULONG>(count), 0) < 0) { error=L"更新ファイルを検証できません。"; break; }
  }
  if (error.empty() && BCryptFinishHash(hash, result.data(), static_cast<ULONG>(result.size()), 0) >= 0) { static const wchar_t hex[]=L"0123456789abcdef"; for (BYTE b:result) { digest+=hex[b>>4]; digest+=hex[b&15]; } }
  else if (error.empty()) error=L"更新ファイルを検証できません。";
  BCryptDestroyHash(hash); BCryptCloseAlgorithmProvider(algorithm,0); return error.empty();
}
bool Newer(const std::wstring& a, const std::wstring& b) {
  auto parse=[](const std::wstring& s){std::vector<int> out;std::wistringstream in(s);std::wstring part;while(std::getline(in,part,L'.')){try{out.push_back(std::stoi(part));}catch(...){out.push_back(0);}}return out;};
  auto x=parse(a), y=parse(b); x.resize(3); y.resize(3); return x>y;
}
}

std::wstring CurrentVersion() {
#ifdef RC_VERSION
  return RC_VERSION;
#else
  return L"0.1.0";
#endif
}

UpdateResult CheckForUpdate() {
  UpdateResult result; std::vector<BYTE> bytes;
  if (!HttpGet(kManifestUrl, bytes, result.error)) return result;
  std::wstring parseError; auto json=Json::Parse(Wide(std::string(reinterpret_cast<char*>(bytes.data()), bytes.size())), &parseError);
  if (!json || json->type!=Json::Type::Object) { result.error=L"更新情報の形式が正しくありません。"; return result; }
  if (auto v=json->Find(L"version")) result.info.version=v->StringOr(); if (auto v=json->Find(L"installerUrl")) result.info.installerUrl=v->StringOr(); if (auto v=json->Find(L"sha256")) result.info.sha256=v->StringOr(); if (auto v=json->Find(L"size")) result.info.size=static_cast<uint64_t>(v->number); if (auto v=json->Find(L"notes")) result.info.notes=v->StringOr();
  if (result.info.version.empty() || result.info.installerUrl.empty() || result.info.sha256.size()!=64 || result.info.size==0) { result.error=L"更新情報に必要な項目がありません。"; return result; }
  URL_COMPONENTS ignored{}; if (!ParseHttpsUrl(result.info.installerUrl, ignored, result.error)) return result;
  result.ok=true; result.available=Newer(result.info.version, CurrentVersion()); return result;
}

bool DownloadAndVerifyUpdate(const UpdateInfo& info, std::filesystem::path& installer, std::wstring& error) {
  std::vector<BYTE> data; if (!HttpGet(info.installerUrl, data, error)) return false;
  wchar_t temp[MAX_PATH]; if (!GetTempPathW(MAX_PATH,temp)) { error=L"更新用一時フォルダを取得できません。"; return false; }
  std::filesystem::path dir=std::filesystem::path(temp)/L"RoboconCadManagerUpdate"; std::error_code ec; std::filesystem::create_directories(dir,ec); if(ec){error=L"更新用一時フォルダを作成できません。";return false;}
  installer=dir/(L"RoboconCadManagerSetup-"+info.version+L".exe"); std::ofstream out(installer,std::ios::binary|std::ios::trunc); if(!out){error=L"更新ファイルを保存できません。";return false;} out.write(reinterpret_cast<const char*>(data.data()),static_cast<std::streamsize>(data.size())); out.close();
  if (data.size()!=info.size) { error=L"更新ファイルのサイズが一致しません。"; DeleteFileW(installer.c_str()); return false; }
  std::wstring hash; if(!Sha256(installer,hash,error)||CompareStringOrdinal(hash.data(),-1,info.sha256.data(),-1,TRUE)!=CSTR_EQUAL){error=L"更新ファイルのSHA-256が一致しません。インストールを中止しました。";DeleteFileW(installer.c_str());return false;} return true;
}
}
