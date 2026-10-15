#include "table_sort_win.hpp"
#include "observation_ui.hpp"
#include <windows.h>
#include <commctrl.h>
#include <iostream>
#include <stdexcept>
using namespace moexwatch;
namespace {void check(bool b,const char*m){if(!b)throw std::runtime_error(m);}}
int main(){try{
 INITCOMMONCONTROLSEX controls{sizeof(controls),ICC_LISTVIEW_CLASSES};check(InitCommonControlsEx(&controls)!=FALSE,"common controls initialize");
 HWND parent=CreateWindowExW(0,L"STATIC",L"MOEXWatch hidden test",WS_OVERLAPPEDWINDOW,0,0,1200,800,nullptr,nullptr,GetModuleHandleW(nullptr),nullptr);check(parent!=nullptr,"hidden owner created");
 HWND list=CreateWindowExW(0,WC_LISTVIEWW,L"",WS_CHILD|LVS_REPORT|LVS_SINGLESEL,0,0,500,300,parent,nullptr,GetModuleHandleW(nullptr),nullptr);check(list!=nullptr,"native report list created");
 LVCOLUMNW col{};col.mask=LVCF_TEXT|LVCF_WIDTH;col.cx=100;col.pszText=const_cast<wchar_t*>(L"Цена");ListView_InsertColumn(list,0,&col);
 const wchar_t* values[]={L"10.0",L"2.0",L"—"};
 for(int i=0;i<3;++i){LVITEMW row{};row.mask=LVIF_TEXT|LVIF_PARAM;row.iItem=i;row.lParam=i;row.pszText=const_cast<wchar_t*>(values[i]);ListView_InsertItem(list,&row);}
 ListView_SetItemState(list,0,LVIS_SELECTED|LVIS_FOCUSED,LVIS_SELECTED|LVIS_FOCUSED);
 sort_report(list,{0,true},true);
 check(row_data(list,0)==1&&row_data(list,1)==0&&row_data(list,2)==2,"ascending numeric sort keeps original model ids and missing last");
 check(selected_row_data(list)==0&&ListView_GetNextItem(list,-1,LVNI_SELECTED)==1,"selection identity follows row instead of visual index");
 sort_report(list,{0,false},true);check(row_data(list,0)==0&&row_data(list,1)==1&&row_data(list,2)==2,"descending keeps missing last");
 ObservationBindings bindings;std::string opened;
 bindings.events=[] {return std::vector<SignalEvent>{{"2026-10-15 12:00:00","SBER","Цена","<= 270",269,"2026-10-15 11:45:00","2026-10-15 12:00:00"},{"2026-10-15 13:00:00","GAZP","Цена",">= 130",131,"2026-10-15 12:45:00","2026-10-15 13:00:00"}};};
 bindings.rules=[] {return std::vector<PriceAlert>{};};bindings.tickers=[] {return std::vector<std::string>{"SBER","GAZP"};};bindings.error=[] {return std::string{};};bindings.save_rules=[](const auto&){};bindings.open_ticker=[&](const auto&t){opened=t;};
 HWND page=create_observation_page(parent,reinterpret_cast<HFONT>(GetStockObject(DEFAULT_GUI_FONT)),bindings);check(page!=nullptr,"history page creates native controls");
 HWND history=GetDlgItem(page,2203);check(ListView_GetItemCount(history)==2&&row_data(history,0)==1,"history defaults to latest first with preserved ids");
 ListView_SetItemState(history,0,LVIS_SELECTED,LVIS_SELECTED);
 NMHDR notice{};notice.hwndFrom=history;notice.idFrom=2203;notice.code=NM_DBLCLK;SendMessageW(page,WM_NOTIFY,2203,reinterpret_cast<LPARAM>(&notice));check(opened=="GAZP","history opens correct ticker after sorting");
 HWND filter=GetDlgItem(page,2202);SendMessageW(filter,CB_SETCURSEL,2,0);SendMessageW(page,WM_COMMAND,MAKEWPARAM(2202,CBN_SELCHANGE),reinterpret_cast<LPARAM>(filter));check(ListView_GetItemCount(history)==1&&row_data(history,0)==0,"ticker filter preserves source identity");
 refresh_observation_page(page);check(ListView_GetItemCount(history)==1,"history refresh preserves selected ticker filter");
 DestroyWindow(parent);std::cout<<"gui_sort_tests: all checks passed\n";
 }catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
