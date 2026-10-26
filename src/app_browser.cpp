#include "app.h"

namespace rc {
namespace {
constexpr WORD kOpen=106,kFilter=109;
std::wstring ChangeDescription(const std::wstring& code){
 if(code.find(L'U')!=std::wstring::npos||code==L"AA"||code==L"DD")return L"競合（要確認）";
 if(code==L"??")return L"新しいファイル";
 if(code.find(L'D')!=std::wstring::npos)return L"削除";
 if(code.find(L'R')!=std::wstring::npos)return L"名前変更";
 if(code.find(L'U')!=std::wstring::npos)return L"競合";
 if(code.find(L'A')!=std::wstring::npos)return L"追加";
 return L"変更";
}
}

void App::CreateBrowserControls(){
 list_=CreateWindowExW(WS_EX_CLIENTEDGE,WC_LISTVIEWW,L"",WS_CHILD|WS_VISIBLE|WS_TABSTOP|LVS_REPORT|LVS_SINGLESEL|LVS_SHOWSELALWAYS,0,0,0,0,window_,nullptr,instance_,nullptr);
 ListView_SetExtendedListViewStyle(list_,LVS_EX_FULLROWSELECT|LVS_EX_DOUBLEBUFFER|LVS_EX_CHECKBOXES);
 const wchar_t* columns[]={L"項目",L"状態",L"編集中",L"最終編集",L"容量"};
 for(int i=0;i<5;++i){LVCOLUMNW column{};column.mask=LVCF_TEXT|LVCF_WIDTH;column.pszText=const_cast<LPWSTR>(columns[i]);column.cx=100;ListView_InsertColumn(list_,i,&column);}
 status_=CreateWindowW(STATUSCLASSNAMEW,L"「設定」からリポジトリを登録してください。",WS_CHILD|WS_VISIBLE,0,0,0,0,window_,nullptr,instance_,nullptr);
 auto button=[&](WORD id,const wchar_t* text){CreateWindowW(L"BUTTON",text,WS_CHILD|WS_VISIBLE|WS_TABSTOP,0,0,0,0,window_,reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),instance_,nullptr);};
 button(101,L"更新");button(105,L"保存して送信");button(107,L"その他");
 button(kBrowseBack,L"←");button(kBrowseForward,L"→");button(kBrowseUp,L"↑");button(kOpenExplorer,L"エクスプローラーで開く");
 filter_=CreateWindowW(WC_COMBOBOXW,L"",WS_CHILD|WS_VISIBLE|WS_TABSTOP|CBS_DROPDOWNLIST,0,0,0,0,window_,reinterpret_cast<HMENU>(kFilter),instance_,nullptr);
 for(auto text:{L"SolidWorksファイル",L"すべてのファイル",L"パーツ",L"アセンブリ",L"図面"})SendMessageW(filter_,CB_ADDSTRING,0,reinterpret_cast<LPARAM>(text));
 SendMessageW(filter_,CB_SETCURSEL,0,0);
 folderLabel_=CreateWindowW(L"STATIC",L"作業フォルダ",WS_CHILD|WS_VISIBLE|SS_CENTERIMAGE|SS_PATHELLIPSIS,0,0,0,0,window_,nullptr,instance_,nullptr);
 UpdateFonts(GetDpiForWindow(window_));UpdateNavigationBar();
}

void App::LayoutBrowser(UINT dpi){
 if(!list_)return;
 if(!dpi)dpi=GetDpiForWindow(window_);auto scale=[&](int value){return MulDiv(value,dpi,96);};
 RECT client{};GetClientRect(window_,&client);SendMessageW(status_,WM_SIZE,0,0);
 RECT status{};GetWindowRect(status_,&status);const int statusHeight=status.bottom-status.top;
 auto move=[&](HWND control,int x,int y,int width,int height){MoveWindow(control,scale(x),scale(y),scale(width),scale(height),TRUE);};
 move(GetDlgItem(window_,101),10,8,90,28);move(GetDlgItem(window_,105),110,8,120,28);move(GetDlgItem(window_,107),240,8,90,28);
 move(filter_,340,8,220,300);
 move(GetDlgItem(window_,kOpenExplorer),570,8,170,28);
 move(GetDlgItem(window_,kBrowseBack),10,44,34,28);move(GetDlgItem(window_,kBrowseForward),48,44,34,28);move(GetDlgItem(window_,kBrowseUp),86,44,34,28);
 MoveWindow(folderLabel_,scale(132),scale(44),std::max(0,static_cast<int>(client.right)-scale(142)),scale(28),TRUE);
 MoveWindow(list_,scale(10),scale(82),std::max(0,static_cast<int>(client.right)-scale(20)),std::max(0,static_cast<int>(client.bottom)-statusHeight-scale(92)),TRUE);
}

void App::UpdateNavigationBar(){
 const auto* repo=Current();
 EnableWindow(GetDlgItem(window_,kBrowseBack),repo&&folderNavigation_.CanBack());
 EnableWindow(GetDlgItem(window_,kBrowseForward),repo&&folderNavigation_.CanForward());
 EnableWindow(GetDlgItem(window_,kBrowseUp),repo&&!folderNavigation_.Current().empty());
 const auto text=repo?(folderNavigation_.Current().empty()?std::wstring(L"作業フォルダ"):L"作業フォルダ / "+folderNavigation_.Current()):L"作業フォルダ";
 SetWindowTextW(folderLabel_,text.c_str());
}

void App::NavigateFolder(const std::wstring& path){if(Current()&&folderNavigation_.Navigate(path))Populate();}

void App::UpdateVisibleCheckStates(){
 const bool previous=populating_;populating_=true;
 for(size_t i=0;i<browserItems_.size();++i){
  const auto& paths=browserItems_[i].changedPaths;
  const bool all=!paths.empty()&&std::all_of(paths.begin(),paths.end(),[this](const auto& path){const auto found=changeChecks_.find(path);return found!=changeChecks_.end()&&found->second;});
  ListView_SetItemState(list_,static_cast<int>(i),paths.empty()?0:INDEXTOSTATEIMAGEMASK(all?2:1),LVIS_STATEIMAGEMASK);
 }
 populating_=previous;
}

LRESULT App::HandleListNotification(LPARAM parameter){
 const auto* notification=reinterpret_cast<NMHDR*>(parameter);if(notification->hwndFrom!=list_)return 0;
 if(notification->code==NM_RCLICK){POINT p;GetCursorPos(&p);ShowListContextMenu(p);return 0;}
 if(notification->code==NM_DBLCLK){if(reinterpret_cast<NMITEMACTIVATE*>(parameter)->iItem>=0)HandleCommand(kOpen);return 0;}
 if(notification->code==LVN_KEYDOWN){
  const auto key=reinterpret_cast<NMLVKEYDOWN*>(parameter)->wVKey;
  if(key==VK_DELETE){DeleteSelectedFile();return 0;}
  if(key==VK_RETURN){HandleCommand(kOpen);return 0;}
  if((GetKeyState(VK_MENU)&0x8000)&&key==VK_LEFT){HandleCommand(kBrowseBack);return 0;}
  if((GetKeyState(VK_MENU)&0x8000)&&key==VK_RIGHT){HandleCommand(kBrowseForward);return 0;}
  if((GetKeyState(VK_MENU)&0x8000)&&key==VK_UP){HandleCommand(kBrowseUp);return 0;}
 }
 if((notification->code==LVN_ITEMCHANGING||notification->code==LVN_ITEMCHANGED)&&!populating_){
  const auto* change=reinterpret_cast<NMLISTVIEW*>(parameter);
  if(change->iItem>=0&&change->iItem<static_cast<int>(browserItems_.size())&&(change->uChanged&LVIF_STATE)){
   const auto before=change->uOldState&LVIS_STATEIMAGEMASK,after=change->uNewState&LVIS_STATEIMAGEMASK;
   if(before!=after){
    const auto& paths=browserItems_[change->iItem].changedPaths;
    if(notification->code==LVN_ITEMCHANGING&&paths.empty())return TRUE;
    if(notification->code==LVN_ITEMCHANGED&&after){
     for(const auto& path:paths)changeChecks_[path]=after==INDEXTOSTATEIMAGEMASK(2);
     UpdateVisibleCheckStates();
    }
   }
  }
 }
 if(notification->code==NM_CUSTOMDRAW){
  auto* draw=reinterpret_cast<NMLVCUSTOMDRAW*>(parameter);
  if(draw->nmcd.dwDrawStage==CDDS_PREPAINT)return CDRF_NOTIFYITEMDRAW;
  if(draw->nmcd.dwDrawStage==CDDS_ITEMPREPAINT)return CDRF_NOTIFYSUBITEMDRAW;
  if(draw->nmcd.dwDrawStage==(CDDS_ITEMPREPAINT|CDDS_SUBITEM)){
   const auto row=static_cast<size_t>(draw->nmcd.dwItemSpec);
   if(row<browserItems_.size()&&!browserItems_[row].changedPaths.empty()){
    const bool removed=!browserItems_[row].directory&&browserItems_[row].changeCode.find(L'D')!=std::wstring::npos;
    draw->clrText=removed?RGB(190,30,30):RGB(25,125,55);
    if(removed&&draw->iSubItem==0&&strikeFont_){SelectObject(draw->nmcd.hdc,strikeFont_);return CDRF_NEWFONT;}
   }
  }
  return CDRF_DODEFAULT;
 }
 return 0;
}

void App::Populate(){
 if(!list_)return;
 std::wstring selectedPath;const int selected=ListView_GetNextItem(list_,-1,LVNI_SELECTED);
 if(selected>=0&&selected<static_cast<int>(listPaths_.size()))selectedPath=listPaths_[selected];
 populating_=true;SendMessageW(list_,WM_SETREDRAW,FALSE,0);ListView_DeleteAllItems(list_);listPaths_.clear();localFiles_.clear();browserItems_.clear();
 if(const auto* repository=Current())browserItems_=BuildFolderItems(*repository,folderNavigation_.Current(),cad_,changes_,filterMode_);
 for(const auto& entry:browserItems_){
  auto locked=locks_.find(entry.path);
  const bool lockingEnabled=Current()&&Current()->lfsLockEnabled;
  LVITEMW item{};item.mask=LVIF_TEXT|LVIF_IMAGE;item.iItem=ListView_GetItemCount(list_);item.pszText=const_cast<LPWSTR>(entry.name.c_str());
  item.iImage=entry.directory?2:(!lockingEnabled?-1:(!locksKnown_?-1:(locked==locks_.end()?0:1)));ListView_InsertItem(list_,&item);
  auto state=entry.directory?(entry.changedPaths.empty()?std::wstring(L"フォルダ"):L"変更あり（"+std::to_wstring(entry.changedPaths.size())+L"件）"):
   (entry.changeCode.empty()?std::wstring(L"変更なし"):ChangeDescription(entry.changeCode));
  if(entry.lfsPointer)state+=L" / 大容量ファイルの本体未取得";
  if(entry.suspiciousName)state+=L" / 版名を見直してください";
  std::wstring editing,last,size;
  const auto commit=latestCommits_.find(entry.path);last=commit==latestCommits_.end()?L"未保存":commit->second.author+L" ("+commit->second.message+L")";
  if(!entry.directory){
   if(!lockingEnabled)editing=L"ロック機能は一時的に無効です";
   else if(!locksKnown_)editing=L"ロック状態を確認できません";
   else if(locked==locks_.end())editing=L"-";
   else if(locked->second.ours)editing=opened_.contains(entry.path)?L"自分が編集中":(pendingUnlock_.contains(entry.path)?L"自分のロック（送信待ち）":L"自分のロック（閉じています）");
   else editing=locked->second.owner.empty()?L"他の利用者が編集中":L"["+locked->second.owner+L"] が編集中";
   // An LFS pointer is only a tiny text file. Do not misleadingly show it as a
   // 0 KB CAD file: the remote object has not been downloaded yet.
   size=!entry.local?L"-":(entry.lfsPointer?L"LFS未取得":std::to_wstring(entry.bytes/1024)+L" KB");
   if(entry.local)localFiles_[entry.path]=std::filesystem::path(Current()->folder)/std::filesystem::path(entry.path);
  }
  ListView_SetItemText(list_,item.iItem,1,const_cast<LPWSTR>(state.c_str()));ListView_SetItemText(list_,item.iItem,2,const_cast<LPWSTR>(editing.c_str()));
  ListView_SetItemText(list_,item.iItem,3,const_cast<LPWSTR>(last.c_str()));ListView_SetItemText(list_,item.iItem,4,const_cast<LPWSTR>(size.c_str()));
  listPaths_.push_back(entry.path);
  if(entry.path==selectedPath)ListView_SetItemState(list_,item.iItem,LVIS_SELECTED|LVIS_FOCUSED,LVIS_SELECTED|LVIS_FOCUSED);
 }
 UpdateVisibleCheckStates();populating_=false;SendMessageW(list_,WM_SETREDRAW,TRUE,0);InvalidateRect(list_,nullptr,FALSE);UpdateNavigationBar();
}
}
