#include "app.h"
#include <shobjidl.h>
int WINAPI wWinMain(HINSTANCE h,HINSTANCE,LPWSTR,int){
  SetCurrentProcessExplicitAppUserModelID(L"RoboconCadManager.Desktop");
  rc::App app;return app.Run(h);
}