#include "table_sort_win.hpp"
#ifdef _WIN32
#include "observation.hpp"
#include <commctrl.h>
#include <algorithm>
#include <cwctype>
#include <string>
#include <vector>
namespace moexwatch {
std::optional<std::size_t> row_data(HWND list,int row){if(row<0)return {};LVITEMW item{};item.mask=LVIF_PARAM;item.iItem=row;if(!ListView_GetItem(list,&item)||item.lParam<0)return {};return static_cast<std::size_t>(item.lParam);}
std::optional<std::size_t> selected_row_data(HWND list){return row_data(list,ListView_GetNextItem(list,-1,LVNI_SELECTED));}
void sort_report(HWND list,const TableSort& sort,bool numeric){
 if(!list||sort.column<0)return;
 const HWND header=ListView_GetHeader(list);const int columns=Header_GetItemCount(header);if(sort.column>=columns||columns>32)return;
 struct Row{std::vector<std::wstring> cells;LPARAM data{};UINT state{};std::optional<double> number;std::wstring key;bool missing{};};
 std::vector<Row> rows;const int count=ListView_GetItemCount(list);rows.reserve(static_cast<std::size_t>(count));
 for(int i=0;i<count;++i){Row row;LVITEMW item{};item.mask=LVIF_PARAM;item.iItem=i;ListView_GetItem(list,&item);row.data=item.lParam;row.state=ListView_GetItemState(list,i,LVIS_SELECTED|LVIS_FOCUSED);
  for(int col=0;col<columns;++col){wchar_t buffer[4096]{};ListView_GetItemText(list,i,col,buffer,4096);row.cells.emplace_back(buffer);}
  row.key=row.cells[static_cast<std::size_t>(sort.column)];row.missing=row.key.empty()||row.key==L"—"||row.key==L"-"||row.key.starts_with(L"н/д");
  if(numeric){std::string ascii;for(wchar_t c:row.key)ascii+=c<128?static_cast<char>(c):' ';row.number=table_number(ascii);}
  else for(auto& c:row.key)c=static_cast<wchar_t>(std::towlower(c));
  rows.push_back(std::move(row));
 }
 std::stable_sort(rows.begin(),rows.end(),[&](const auto&a,const auto&b){if(numeric)return optional_number_less(a.number,b.number,sort.ascending);if(a.missing!=b.missing)return !a.missing;return sort.ascending?a.key<b.key:a.key>b.key;});
 SendMessageW(list,WM_SETREDRAW,FALSE,0);ListView_DeleteAllItems(list);
 for(std::size_t i=0;i<rows.size();++i){auto&row=rows[i];LVITEMW item{};item.mask=LVIF_TEXT|LVIF_PARAM|LVIF_STATE;item.iItem=static_cast<int>(i);item.pszText=row.cells[0].data();item.lParam=row.data;item.stateMask=LVIS_SELECTED|LVIS_FOCUSED;item.state=row.state;const int index=ListView_InsertItem(list,&item);for(int col=1;col<columns;++col)ListView_SetItemText(list,index,col,row.cells[static_cast<std::size_t>(col)].data());}
 for(int col=0;col<columns;++col){HDITEMW item{};item.mask=HDI_FORMAT;Header_GetItem(header,col,&item);item.fmt&=~(HDF_SORTUP|HDF_SORTDOWN);if(col==sort.column)item.fmt|=sort.ascending?HDF_SORTUP:HDF_SORTDOWN;Header_SetItem(header,col,&item);}
 SendMessageW(list,WM_SETREDRAW,TRUE,0);InvalidateRect(list,nullptr,TRUE);
}
}
#endif
