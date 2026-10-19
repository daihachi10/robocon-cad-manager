#include "cad.h"
#include <iostream>

int wmain(int argc,wchar_t** argv){
 if(argc!=2)return 1;
 const auto file=std::filesystem::path(argv[1])/L"読み取り専用 テスト.SLDPRT";
 std::ofstream(file,std::ios::binary)<<"original";
 if(!SetFileAttributesW(file.c_str(),FILE_ATTRIBUTE_READONLY|FILE_ATTRIBUTE_ARCHIVE))return 2;
 std::wstring error;
 if(!rc::MakeCadWritable(file,error)){std::wcerr<<error;return 3;}
 const auto attrs=GetFileAttributesW(file.c_str());
 if((attrs&FILE_ATTRIBUTE_READONLY)||(attrs&FILE_ATTRIBUTE_ARCHIVE)==0)return 4;
 {std::ofstream out(file,std::ios::binary|std::ios::trunc);out<<"saved";if(!out)return 5;}
 std::ifstream input(file,std::ios::binary);std::string text;input>>text;input.close();
 if(text!="saved")return 6;
 if(!rc::MakeCadWritable(file,error))return 7;
 if(!DeleteFileW(file.c_str()))return 8;
 if(rc::MakeCadWritable(file,error)||error.empty())return 9;
 std::cout<<"Read-only CAD can be saved; other attributes preserved; missing files fail.\n";
 return 0;
}
