#include "process_runner.h"
namespace rc {
std::wstring QuoteArg(const std::wstring&v){if(v.empty())return L"\"\"";std::wstring r=L"\"";size_t slash=0;for(auto c:v){if(c==L'\\'){++slash;continue;}if(c==L'\"')r.append(slash*2+1,L'\\');else r.append(slash,L'\\');slash=0;r+=c;}r.append(slash*2,L'\\');return r+L"\"";}
namespace {
std::wstring SafeArgument(std::wstring value){
 const auto scheme=value.find(L"://");
 if(scheme!=std::wstring::npos){const auto start=scheme+3,slash=value.find(L'/',start),at=value.find(L'@',start);if(at!=std::wstring::npos&&(slash==std::wstring::npos||at<slash))value.replace(start,at-start,L"***");}
 return value;
}
std::wstring DisplayCommand(const std::filesystem::path& exe,const std::vector<std::wstring>& args){
 std::wstring command=QuoteArg(exe.filename().wstring());for(const auto& argument:args)command+=L" "+QuoteArg(SafeArgument(argument));return command;
}
std::wstring ActualCommand(const std::filesystem::path& exe,const std::vector<std::wstring>& args){
 std::wstring command=QuoteArg(exe.wstring());for(const auto& argument:args)command+=L" "+QuoteArg(argument);return command;
}
// Only the child Git process receives these overrides. Never change the UI process's
// environment: other background jobs and Credential Manager can run concurrently.
std::vector<wchar_t> GitEnvironment(const std::filesystem::path& exe){
 if(!IEndsWith(exe.filename().wstring(),L"git.exe"))return{};
 std::vector<std::wstring> entries;
 const std::vector<std::wstring> overrides{L"GIT_EDITOR=true",L"GIT_MERGE_AUTOEDIT=no",L"GIT_TERMINAL_PROMPT=0",L"LC_ALL=C"};
 if(auto block=GetEnvironmentStringsW()){
  for(auto p=block;*p;p+=wcslen(p)+1){
   std::wstring entry=p;bool replaced=false;
   for(const auto& value:overrides){const auto length=value.find(L'=')+1;if(entry.size()>=length&&CompareStringOrdinal(entry.data(),static_cast<int>(length),value.data(),static_cast<int>(length),TRUE)==CSTR_EQUAL)replaced=true;}
   if(!replaced)entries.push_back(std::move(entry));
  }
  FreeEnvironmentStringsW(block);
 }
 entries.insert(entries.end(),overrides.begin(),overrides.end());
 std::sort(entries.begin(),entries.end(),[](const auto& a,const auto& b){return CompareStringOrdinal(a.c_str(),-1,b.c_str(),-1,TRUE)==CSTR_LESS_THAN;});
 std::vector<wchar_t> result;for(const auto& entry:entries){result.insert(result.end(),entry.begin(),entry.end());result.push_back(0);}result.push_back(0);return result;
}
}
std::wstring ProcessDisplayCommand(const std::filesystem::path& exe,const std::vector<std::wstring>& args){return DisplayCommand(exe,args);}
ProcessResult RunProcess(const std::filesystem::path&exe,const std::vector<std::wstring>&args,const std::filesystem::path&cwd,const std::wstring&input,const ProcessOutputCallback& outputCallback){
 ProcessResult r;r.exitCode=MAXDWORD;r.command=DisplayCommand(exe,args);const auto cmd=ActualCommand(exe,args);
 SECURITY_ATTRIBUTES sa{sizeof(sa),nullptr,TRUE};HANDLE outRead{},outWrite{};if(!CreatePipe(&outRead,&outWrite,&sa,0)){r.error=LastError();return r;}SetHandleInformation(outRead,HANDLE_FLAG_INHERIT,0);
 auto environment=GitEnvironment(exe);
 HANDLE in=CreateFileW(L"NUL",GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_WRITE,&sa,OPEN_EXISTING,0,nullptr);
 STARTUPINFOW si{sizeof(si)};si.dwFlags=STARTF_USESHOWWINDOW|STARTF_USESTDHANDLES;si.wShowWindow=SW_HIDE;si.hStdOutput=outWrite;si.hStdError=outWrite;si.hStdInput=in;
 PROCESS_INFORMATION pi{};std::vector<wchar_t> mutableCmd(cmd.begin(),cmd.end());mutableCmd.push_back(0);
 const bool started=CreateProcessW(exe.c_str(),mutableCmd.data(),nullptr,nullptr,TRUE,CREATE_NO_WINDOW|CREATE_UNICODE_ENVIRONMENT,environment.empty()?nullptr:environment.data(),cwd.empty()?nullptr:cwd.c_str(),&si,&pi)!=FALSE;
 const auto startError=GetLastError();if(in!=INVALID_HANDLE_VALUE)CloseHandle(in);CloseHandle(outWrite);
 if(!started){CloseHandle(outRead);r.error=LastError(startError);return r;}
 std::string bytes;char b[4096];DWORD n;while(ReadFile(outRead,b,sizeof(b),&n,nullptr)&&n){bytes.append(b,n);if(outputCallback){const auto current=Wide(bytes);if(!current.empty())outputCallback(current);}}
 CloseHandle(outRead);WaitForSingleObject(pi.hProcess,INFINITE);GetExitCodeProcess(pi.hProcess,&r.exitCode);CloseHandle(pi.hThread);CloseHandle(pi.hProcess);r.output=Wide(bytes);(void)input;return r;
}
ProcessResult RunProcessToFile(const std::filesystem::path&exe,const std::vector<std::wstring>&args,const std::filesystem::path&cwd,const std::filesystem::path&outputFile){
 ProcessResult r;r.exitCode=MAXDWORD;r.command=DisplayCommand(exe,args);const auto cmd=ActualCommand(exe,args);
 SECURITY_ATTRIBUTES sa{sizeof(sa),nullptr,TRUE};HANDLE output=CreateFileW(outputFile.c_str(),GENERIC_WRITE,FILE_SHARE_READ,&sa,CREATE_ALWAYS,FILE_ATTRIBUTE_TEMPORARY,nullptr);if(output==INVALID_HANDLE_VALUE){r.error=LastError();return r;}
 HANDLE errRead{},errWrite{};if(!CreatePipe(&errRead,&errWrite,&sa,0)){CloseHandle(output);r.error=LastError();return r;}SetHandleInformation(errRead,HANDLE_FLAG_INHERIT,0);
 auto environment=GitEnvironment(exe);HANDLE in=CreateFileW(L"NUL",GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_WRITE,&sa,OPEN_EXISTING,0,nullptr);
 STARTUPINFOW si{sizeof(si)};si.dwFlags=STARTF_USESHOWWINDOW|STARTF_USESTDHANDLES;si.wShowWindow=SW_HIDE;si.hStdOutput=output;si.hStdError=errWrite;si.hStdInput=in;
 PROCESS_INFORMATION pi{};std::vector<wchar_t> mutableCmd(cmd.begin(),cmd.end());mutableCmd.push_back(0);
 const bool started=CreateProcessW(exe.c_str(),mutableCmd.data(),nullptr,nullptr,TRUE,CREATE_NO_WINDOW|CREATE_UNICODE_ENVIRONMENT,environment.empty()?nullptr:environment.data(),cwd.empty()?nullptr:cwd.c_str(),&si,&pi)!=FALSE;
 const auto startError=GetLastError();if(in!=INVALID_HANDLE_VALUE)CloseHandle(in);CloseHandle(output);CloseHandle(errWrite);
 if(!started){CloseHandle(errRead);r.error=LastError(startError);return r;}
 // Drain stderr while Git is running: a full pipe must not prevent its exit.
 std::string error;char b[4096];DWORD n;while(ReadFile(errRead,b,sizeof(b),&n,nullptr)&&n)error.append(b,n);CloseHandle(errRead);
 WaitForSingleObject(pi.hProcess,INFINITE);GetExitCodeProcess(pi.hProcess,&r.exitCode);CloseHandle(pi.hThread);CloseHandle(pi.hProcess);r.error=Wide(error);return r;
}
}
