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
}
std::wstring ProcessDisplayCommand(const std::filesystem::path& exe,const std::vector<std::wstring>& args){return DisplayCommand(exe,args);}
ProcessResult RunProcess(const std::filesystem::path&exe,const std::vector<std::wstring>&args,const std::filesystem::path&cwd,const std::wstring&input,const ProcessOutputCallback& outputCallback){
 ProcessResult r;r.exitCode=MAXDWORD;r.command=DisplayCommand(exe,args);const auto cmd=ActualCommand(exe,args);
 SECURITY_ATTRIBUTES sa{sizeof(sa),nullptr,TRUE};HANDLE outRead{},outWrite{};if(!CreatePipe(&outRead,&outWrite,&sa,0)){r.error=LastError();return r;}SetHandleInformation(outRead,HANDLE_FLAG_INHERIT,0);
 STARTUPINFOW si{sizeof(si)};si.dwFlags=STARTF_USESHOWWINDOW|STARTF_USESTDHANDLES;si.wShowWindow=SW_HIDE;si.hStdOutput=outWrite;si.hStdError=outWrite;si.hStdInput=GetStdHandle(STD_INPUT_HANDLE);PROCESS_INFORMATION pi{};std::vector<wchar_t> mutableCmd(cmd.begin(),cmd.end());mutableCmd.push_back(0);if(!CreateProcessW(exe.c_str(),mutableCmd.data(),nullptr,nullptr,TRUE,CREATE_NO_WINDOW,nullptr,cwd.empty()?nullptr:cwd.c_str(),&si,&pi)){CloseHandle(outRead);CloseHandle(outWrite);r.error=LastError();return r;}CloseHandle(outWrite);std::string bytes;char b[4096];DWORD n;while(ReadFile(outRead,b,sizeof(b),&n,nullptr)&&n){bytes.append(b,n);if(outputCallback){const auto current=Wide(bytes);if(!current.empty())outputCallback(current);}}CloseHandle(outRead);WaitForSingleObject(pi.hProcess,INFINITE);GetExitCodeProcess(pi.hProcess,&r.exitCode);CloseHandle(pi.hThread);CloseHandle(pi.hProcess);r.output=Wide(bytes);(void)input;return r;
}
ProcessResult RunProcessToFile(const std::filesystem::path&exe,const std::vector<std::wstring>&args,const std::filesystem::path&cwd,const std::filesystem::path&outputFile){
 ProcessResult r;r.exitCode=MAXDWORD;r.command=DisplayCommand(exe,args);const auto cmd=ActualCommand(exe,args);
 SECURITY_ATTRIBUTES sa{sizeof(sa),nullptr,TRUE};HANDLE output=CreateFileW(outputFile.c_str(),GENERIC_WRITE,FILE_SHARE_READ,&sa,CREATE_ALWAYS,FILE_ATTRIBUTE_TEMPORARY,nullptr);if(output==INVALID_HANDLE_VALUE){r.error=LastError();return r;}HANDLE errRead{},errWrite{};if(!CreatePipe(&errRead,&errWrite,&sa,0)){CloseHandle(output);r.error=LastError();return r;}SetHandleInformation(errRead,HANDLE_FLAG_INHERIT,0);STARTUPINFOW si{sizeof(si)};si.dwFlags=STARTF_USESHOWWINDOW|STARTF_USESTDHANDLES;si.wShowWindow=SW_HIDE;si.hStdOutput=output;si.hStdError=errWrite;si.hStdInput=GetStdHandle(STD_INPUT_HANDLE);PROCESS_INFORMATION pi{};std::vector<wchar_t> mutableCmd(cmd.begin(),cmd.end());mutableCmd.push_back(0);if(!CreateProcessW(exe.c_str(),mutableCmd.data(),nullptr,nullptr,TRUE,CREATE_NO_WINDOW,nullptr,cwd.empty()?nullptr:cwd.c_str(),&si,&pi)){CloseHandle(output);CloseHandle(errRead);CloseHandle(errWrite);r.error=LastError();return r;}CloseHandle(output);CloseHandle(errWrite);WaitForSingleObject(pi.hProcess,INFINITE);GetExitCodeProcess(pi.hProcess,&r.exitCode);std::string error;char b[4096];DWORD n;while(ReadFile(errRead,b,sizeof(b),&n,nullptr)&&n)error.append(b,n);CloseHandle(errRead);CloseHandle(pi.hThread);CloseHandle(pi.hProcess);r.error=Wide(error);return r;
}
}
