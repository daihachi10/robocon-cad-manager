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
  components.dwHostNameLength = static_cast<DWORD>(-1);
  components.dwUrlPathLength = static_cast<DWORD>(-1);
  components.dwExtraInfoLength = static_cast<DWORD>(-1);
  if (!WinHttpCrackUrl(value.c_str(), 0, 0, &components)) {
    const auto code = GetLastError();
    error = L"更新先URLを解析できません。（Windowsエラー " + std::to_wstring(code) + L"）\n" + Trim(LastError(code));
    return false;
  }
  if (components.nScheme != INTERNET_SCHEME_HTTPS) {
    error = L"更新先URLがHTTPSではありません。";
    return false;
  }
  return true;
}

std::wstring HttpError(const std::wstring& operation, DWORD code = GetLastError()) {
  if(code==ERROR_WINHTTP_TIMEOUT)return operation+L"が時間内に完了しませんでした。（Windowsエラー 12002）\n通信が一時的に止まっています。時間をおいて再試行してください。";
  return operation + L"に失敗しました。（Windowsエラー " + std::to_wstring(code) + L"）\n" + Trim(LastError(code));
}

bool HttpRead(const std::wstring& url,const std::function<bool(const BYTE*,DWORD)>& consume,uint64_t maximum,std::wstring& error,int receiveTimeout) {
  error.clear();
  URL_COMPONENTS parts{};
  if (!ParseHttpsUrl(url, parts, error)) return false;
  std::wstring host(parts.lpszHostName, parts.dwHostNameLength);
  std::wstring path(parts.lpszUrlPath, parts.dwUrlPathLength);
  if (parts.dwExtraInfoLength) path.append(parts.lpszExtraInfo, parts.dwExtraInfoLength);
  if (host.empty()) { error = L"更新先URLにホスト名がありません。"; return false; }
  if (path.empty()) path = L"/";
  HINTERNET session = WinHttpOpen(L"RoboconCadManager/0.1", WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY, nullptr, nullptr, 0);
  if (!session) { error = HttpError(L"更新確認の通信開始"); return false; }
  WinHttpSetTimeouts(session, 10000, 15000, 30000, receiveTimeout);
  HINTERNET connection = WinHttpConnect(session, host.c_str(), parts.nPort, 0);
  HINTERNET request = connection ? WinHttpOpenRequest(connection, L"GET", path.c_str(), nullptr,
      WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE) : nullptr;
  bool result = false;
  if (!connection) error = HttpError(L"更新サーバーへの接続");
  else if (!request) error = HttpError(L"更新要求の作成");
  else if (!WinHttpSendRequest(request, WINHTTP_NO_ADDITIONAL_HEADERS, 0, nullptr, 0, 0, 0)) error = HttpError(L"更新要求の送信");
  else if (!WinHttpReceiveResponse(request, nullptr)) error = HttpError(L"更新サーバーからの応答受信");
  else {
    DWORD status{}, statusSize = sizeof(status);
    if (!WinHttpQueryHeaders(request, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, WINHTTP_HEADER_NAME_BY_INDEX, &status, &statusSize, WINHTTP_NO_HEADER_INDEX)) {
      error = HttpError(L"更新サーバーの応答確認");
    } else if (status >= 200 && status < 300) {
      std::vector<BYTE> buffer(64*1024);
      uint64_t received{};
      for (;;) {
        DWORD read{};
        if (!WinHttpReadData(request, buffer.data(), static_cast<DWORD>(buffer.size()), &read)) { error = HttpError(L"更新データの受信"); break; }
        if (!read) { result = true; break; }
        if(read>maximum-received){error=L"更新サーバーから想定より大きいデータを受信しました。";break;}
        received+=read;
        if(!consume(buffer.data(),read))break;
      }
    } else error = L"更新サーバーがHTTP "+std::to_wstring(status)+L"を返しました。";
  }
  if (!result && error.empty()) error = L"更新情報を取得できません。ネットワーク接続を確認してください。";
  if (request) WinHttpCloseHandle(request); if (connection) WinHttpCloseHandle(connection); WinHttpCloseHandle(session);
  return result;
}

bool HttpGet(const std::wstring& url,std::vector<BYTE>& body,std::wstring& error){
  body.clear();
  return HttpRead(url,[&](const BYTE* bytes,DWORD count){body.insert(body.end(),bytes,bytes+count);return true;},1024*1024,error,30000);
}

bool GitHubOwnerRepository(const std::wstring& value,std::wstring& owner,std::wstring& repository,std::wstring& error) {
  owner.clear(); repository.clear();
  std::wstring url=Trim(value);
  if(url.starts_with(L"git@github.com:")) url=L"https://github.com/"+url.substr(15);
  if(url.find(L"://")==std::wstring::npos) url=L"https://github.com/"+url;
  URL_COMPONENTS parts{}; parts.dwStructSize=sizeof(parts); parts.dwHostNameLength=static_cast<DWORD>(-1); parts.dwUrlPathLength=static_cast<DWORD>(-1);
  if(!WinHttpCrackUrl(url.c_str(),0,0,&parts)) { error=L"GitHubリポジトリURLを読み取れません。"; return false; }
  const std::wstring host(parts.lpszHostName,parts.dwHostNameLength);
  if(CompareStringOrdinal(host.c_str(),-1,L"github.com",-1,TRUE)!=CSTR_EQUAL) { error=L"GitHubのリポジトリURLを設定してください。"; return false; }
  std::wstring path(parts.lpszUrlPath,parts.dwUrlPathLength);
  while(!path.empty()&&path.front()==L'/')path.erase(path.begin());
  while(!path.empty()&&path.back()==L'/')path.pop_back();
  std::vector<std::wstring> segments; std::wistringstream input(path); std::wstring segment;
  while(std::getline(input,segment,L'/'))if(!segment.empty())segments.push_back(segment);
  if(segments.size()!=2||segments[0].empty()||segments[1].empty()) { error=L"GitHubリポジトリURLに所有者とリポジトリ名を指定してください。"; return false; }
  owner=segments[0]; repository=segments[1];
  if(IEndsWith(repository,L".git"))repository.resize(repository.size()-4);
  if(owner.find_first_of(L"?#\\")!=std::wstring::npos||repository.empty()||repository.find_first_of(L"?#\\")!=std::wstring::npos) { error=L"GitHubリポジトリURLが正しくありません。"; return false; }
  return true;
}

bool Sha256(const std::filesystem::path& file, std::wstring& digest, std::wstring& error) {
  digest.clear();
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
  if(stream.bad()&&error.empty())error=L"検証する更新ファイルを読み取れません。";
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
  error.clear();installer.clear();
  if(info.size==0||info.size>512ull*1024*1024||info.version.empty()||
      info.version.find_first_not_of(L"0123456789.")!=std::wstring::npos||info.sha256.size()!=64||
      info.sha256.find_first_not_of(L"0123456789abcdefABCDEF")!=std::wstring::npos){error=L"更新情報のサイズ・バージョン・SHA-256が正しくありません。";return false;}
  wchar_t temp[MAX_PATH];const auto tempLength=GetTempPathW(MAX_PATH,temp);
  if (!tempLength||tempLength>=MAX_PATH) { error=L"更新用一時フォルダを取得できません。"; return false; }
  std::filesystem::path dir=std::filesystem::path(temp)/L"RoboconCadManagerUpdate"; std::error_code ec; std::filesystem::create_directories(dir,ec); if(ec){error=L"更新用一時フォルダを作成できません。";return false;}
  wchar_t partial[MAX_PATH];
  if(!GetTempFileNameW(dir.c_str(),L"RCM",0,partial)){error=L"更新用一時ファイルを作成できません。";return false;}
  struct TemporaryFile{const wchar_t* path;~TemporaryFile(){DeleteFileW(path);}} cleanup{partial};
  std::ofstream out(std::filesystem::path(partial),std::ios::binary|std::ios::trunc);
  if(!out){error=L"更新ファイルを保存できません。";return false;}
  uint64_t received{};
  const bool downloaded=HttpRead(info.installerUrl,[&](const BYTE* bytes,DWORD count){
    out.write(reinterpret_cast<const char*>(bytes),count);received+=count;
    if(!out){error=L"更新ファイルを書き込めません。空き容量を確認してください。";return false;}return true;
  },info.size,error,120000);
  out.close();
  if(!downloaded)return false;
  if(!out){error=L"更新ファイルを保存できません。";return false;}
  if(received!=info.size){error=L"更新ファイルのサイズが一致しません。";return false;}
  std::wstring hash;
  if(!Sha256(partial,hash,error))return false;
  if(CompareStringOrdinal(hash.c_str(),-1,info.sha256.c_str(),-1,TRUE)!=CSTR_EQUAL){error=L"更新ファイルのSHA-256が一致しません。インストールを中止しました。";return false;}
  const auto verified=dir/(L"RoboconCadManagerSetup-"+info.version+L".exe");
  if(!MoveFileExW(partial,verified.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)){error=L"検証済みの更新ファイルを保存できません。";return false;}
  installer=verified;return true;
}

bool DownloadGitIgnore(const std::wstring&,const std::filesystem::path& destination,std::wstring& error) {
  error.clear();
  // 配布したテンプレートを使う。非公開リポジトリでもネットワーク認証なしで追加できる。
  constexpr char content[] =
    "# gitignore file for a SolidWorks project\r\n\r\n"
    "# SolidWorks temporary files\r\n"
    "~$*.sldprt\r\n~$*.sldasm\r\n~$*.slddrw\r\n~$*.sld\r\n\r\n"
    "# For systems with case-sensitive (SolidWorks uses both)\r\n"
    "~$*.SLDPRT\r\n~$*.SLDASM\r\n~$*.SLDDRW\r\n~$*.SLD\r\n\r\n"
    "# SolidWorks backups and auto-recovery\r\n"
    "*.bak\r\n*.swbak\r\n\r\n"
    "# Cache of simulation results\r\n"
    "/simulation_results/\r\n";
  const auto temporary=destination.wstring()+L".download";
  std::ofstream output(std::filesystem::path(temporary),std::ios::binary|std::ios::trunc);
  if(!output) { error=L".gitignore の一時ファイルを作成できません。"; return false; }
  output.write(content,static_cast<std::streamsize>(sizeof(content)-1)); output.close();
  if(!output) { DeleteFileW(temporary.c_str()); error=L".gitignore を保存できません。"; return false; }
  if(!MoveFileExW(temporary.c_str(),destination.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)) { DeleteFileW(temporary.c_str()); error=L".gitignore を作業フォルダーへ配置できません。\n"+LastError(); return false; }
  return true;
}
}
