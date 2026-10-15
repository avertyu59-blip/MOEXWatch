#include "portfolio_import_ui.hpp"
#ifdef _WIN32
#include <commctrl.h>
#include <algorithm>
#include <memory>
#include <iomanip>
#include <sstream>
#include <stdexcept>
namespace moexwatch {
namespace {
constexpr int ID_INCLUDE=2101, ID_APPLY=2102, ID_CANCEL_IMPORT=IDCANCEL;
std::wstring wide(std::string_view s){if(s.empty())return {};const auto n=MultiByteToWideChar(CP_UTF8,0,s.data(),static_cast<int>(s.size()),nullptr,0);std::wstring w(static_cast<std::size_t>(n),L'\0');MultiByteToWideChar(CP_UTF8,0,s.data(),static_cast<int>(s.size()),w.data(),n);return w;}
struct State{
 ImportPreview preview;
 std::vector<PortfolioTransaction> existing;
 std::function<void(const std::vector<PortfolioTransaction>&)> commit;
 HWND info{},list{},include{},apply{},cancel{};
 HFONT font{};
 bool accepted{};
};
std::string precise(double value){std::ostringstream out;out<<std::setprecision(17)<<value;return out.str();}
void refresh(State& s){
 const bool include=SendMessageW(s.include,BM_GETCHECK,0,0)==BST_CHECKED;
 std::string error;const auto candidate=prepare_portfolio_import(s.preview,s.existing,include,error);
 std::size_t fresh=0,duplicates=0,invalid=0;
 for(const auto&r:s.preview.rows){if(r.status==ImportStatus::New)++fresh;else if(r.status==ImportStatus::Duplicate)++duplicates;else ++invalid;}
 std::string text="Новых: "+std::to_string(fresh)+" | Совпадений: "+std::to_string(duplicates)+" | Ошибок: "+std::to_string(invalid)+
   "\r\n"+(candidate?"К добавлению: "+std::to_string(candidate->size()-s.existing.size())+". Перед записью будет создана резервная копия.":error)+
   "\r\nРежим: добавить к журналу. Одинаковая дата: сначала текущий журнал, затем CSV в порядке строк.\r\nЦены/комиссия — ₽, количество — акции. Совпадение — все 6 полей. Отчёт брокера нужно привести к формату MOEXWatch.";
 SetWindowTextW(s.info,wide(text).c_str());EnableWindow(s.apply,candidate.has_value());
 ListView_DeleteAllItems(s.list);
 for(std::size_t i=0;i<s.preview.rows.size();++i){const auto&r=s.preview.rows[i];std::wstring line=std::to_wstring(r.line);LVITEMW item{};item.mask=LVIF_TEXT;item.iItem=static_cast<int>(i);item.pszText=line.data();const int index=ListView_InsertItem(s.list,&item);
  const auto set=[&](int col,std::string value){auto w=wide(value);ListView_SetItemText(s.list,index,col,w.data());};
  if(r.transaction){const auto&t=*r.transaction;set(1,t.date);set(2,t.ticker);set(3,t.side==PortfolioSide::Buy?"BUY":"SELL");set(4,precise(t.quantity));set(5,precise(t.price));set(6,precise(t.commission));}
  set(7,r.status==ImportStatus::Error?"ОШИБКА":r.status==ImportStatus::Duplicate?(include?"Совпадение: включено":"Совпадение: пропуск"):"Добавить");set(8,r.message);
 }
}
LRESULT CALLBACK proc(HWND window,UINT message,WPARAM wp,LPARAM lp){
 auto* state=reinterpret_cast<State*>(GetWindowLongPtrW(window,GWLP_USERDATA));
 if(message==WM_NCCREATE){state=static_cast<State*>(reinterpret_cast<CREATESTRUCTW*>(lp)->lpCreateParams);SetWindowLongPtrW(window,GWLP_USERDATA,reinterpret_cast<LONG_PTR>(state));}
 if(!state)return DefWindowProcW(window,message,wp,lp);
 auto&s=*state;
 switch(message){
 case WM_CREATE:{
  const auto child=[&](const wchar_t* type,const wchar_t* title,DWORD style,int id){HWND h=CreateWindowExW(0,type,title,WS_CHILD|WS_VISIBLE|style,0,0,100,28,window,reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),GetModuleHandleW(nullptr),nullptr);SendMessageW(h,WM_SETFONT,reinterpret_cast<WPARAM>(s.font),TRUE);return h;};
  s.info=child(L"STATIC",L"",SS_LEFT,0);s.include=child(L"BUTTON",L"Включить совпадающие операции (по умолчанию пропускаются)",BS_AUTOCHECKBOX|WS_TABSTOP,ID_INCLUDE);
  s.list=child(WC_LISTVIEWW,L"",LVS_REPORT|LVS_SINGLESEL|WS_BORDER|WS_TABSTOP,0);
  ListView_SetExtendedListViewStyle(s.list,LVS_EX_FULLROWSELECT|LVS_EX_GRIDLINES|LVS_EX_DOUBLEBUFFER);
  const wchar_t* names[]={L"Строка",L"Дата",L"Тикер",L"Тип",L"Акций",L"Цена",L"Комиссия",L"Результат",L"Пояснение"};const int widths[]={60,100,80,65,95,95,95,200,470};
  for(int i=0;i<9;++i){LVCOLUMNW col{};col.mask=LVCF_TEXT|LVCF_WIDTH;col.pszText=const_cast<wchar_t*>(names[i]);col.cx=widths[i];ListView_InsertColumn(s.list,i,&col);}
  s.apply=child(L"BUTTON",L"Подтвердить импорт",BS_PUSHBUTTON|WS_TABSTOP,ID_APPLY);s.cancel=child(L"BUTTON",L"Отмена",BS_PUSHBUTTON|WS_TABSTOP,ID_CANCEL_IMPORT);refresh(s);return 0;
 }
 case WM_SIZE:{const int width=LOWORD(lp),height=HIWORD(lp);MoveWindow(s.info,12,10,width-24,88,TRUE);MoveWindow(s.include,12,102,width-24,30,TRUE);MoveWindow(s.list,12,142,width-24,std::max(100,height-202),TRUE);MoveWindow(s.apply,width-340,height-44,190,30,TRUE);MoveWindow(s.cancel,width-138,height-44,126,30,TRUE);return 0;}
 case WM_COMMAND:
  if(LOWORD(wp)==ID_INCLUDE){refresh(s);return 0;}
  if(LOWORD(wp)==ID_CANCEL_IMPORT){DestroyWindow(window);return 0;}
  if(LOWORD(wp)==ID_APPLY){
   std::string error;const auto candidate=prepare_portfolio_import(s.preview,s.existing,SendMessageW(s.include,BM_GETCHECK,0,0)==BST_CHECKED,error);
   if(!candidate){MessageBoxW(window,wide(error).c_str(),L"Импорт заблокирован",MB_ICONWARNING);return 0;}
   try{s.commit(*candidate);s.accepted=true;DestroyWindow(window);}
   catch(const std::exception& ex){MessageBoxW(window,wide(std::string("Импорт не записан: ")+ex.what()).c_str(),L"MOEXWatch",MB_ICONERROR);}return 0;
  }break;
 case WM_GETMINMAXINFO:{auto*m=reinterpret_cast<MINMAXINFO*>(lp);m->ptMinTrackSize.x=980;m->ptMinTrackSize.y=520;return 0;}
 case WM_CLOSE:DestroyWindow(window);return 0;
 }
 return DefWindowProcW(window,message,wp,lp);
}
}
bool review_portfolio_import(HWND parent,HFONT font,std::string csv,const std::vector<PortfolioTransaction>& existing,std::function<void(const std::vector<PortfolioTransaction>&)> commit){
 const auto instance=GetModuleHandleW(nullptr);WNDCLASSEXW wc{};wc.cbSize=sizeof(wc);wc.hInstance=instance;wc.lpfnWndProc=proc;wc.hCursor=LoadCursorW(nullptr,IDC_ARROW);wc.hbrBackground=reinterpret_cast<HBRUSH>(COLOR_WINDOW+1);wc.lpszClassName=L"MOEXWatchImportPreview";
 if(!RegisterClassExW(&wc)&&GetLastError()!=ERROR_CLASS_ALREADY_EXISTS)throw std::runtime_error("Не удалось создать окно просмотра CSV");
 State state;state.preview=preview_portfolio_csv(csv,existing);state.existing=existing;state.commit=std::move(commit);state.font=font;
 HWND window=CreateWindowExW(WS_EX_DLGMODALFRAME,wc.lpszClassName,L"Импорт операций — предварительный просмотр",WS_OVERLAPPEDWINDOW,CW_USEDEFAULT,CW_USEDEFAULT,1180,650,parent,nullptr,instance,&state);
 if(!window)throw std::runtime_error("Не удалось открыть предварительный просмотр");
 const bool was_enabled=IsWindowEnabled(parent)!=FALSE;EnableWindow(parent,FALSE);ShowWindow(window,SW_SHOW);SetForegroundWindow(window);
 MSG msg{};while(IsWindow(window)){const int status=static_cast<int>(GetMessageW(&msg,nullptr,0,0));if(status<=0){if(IsWindow(window))DestroyWindow(window);if(status==0)PostQuitMessage(static_cast<int>(msg.wParam));break;}if(!IsDialogMessageW(window,&msg)){TranslateMessage(&msg);DispatchMessageW(&msg);}}
 if(IsWindow(parent)){EnableWindow(parent,was_enabled);SetForegroundWindow(parent);}return state.accepted;
}
}
#endif
