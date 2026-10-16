#include "observation_ui.hpp"
#ifdef _WIN32
#include "table_sort_win.hpp"
#include <commctrl.h>
#include <algorithm>
#include <charconv>
#include <iomanip>
#include <limits>
#include <memory>
#include <set>
#include <sstream>
#include <stdexcept>
namespace moexwatch {
namespace {
constexpr int RULES=2201,FILTER=2202,HISTORY=2203,ADD=2210,REMOVE=2211,TOGGLE=2212,SAVE=2213;
std::wstring wide(std::string_view s){if(s.empty())return {};const int n=MultiByteToWideChar(CP_UTF8,0,s.data(),static_cast<int>(s.size()),nullptr,0);std::wstring w(static_cast<std::size_t>(n),L'\0');MultiByteToWideChar(CP_UTF8,0,s.data(),static_cast<int>(s.size()),w.data(),n);return w;}
std::string narrow(std::wstring_view s){if(s.empty())return {};const int n=WideCharToMultiByte(CP_UTF8,0,s.data(),static_cast<int>(s.size()),nullptr,0,nullptr,nullptr);std::string out(static_cast<std::size_t>(n),'\0');WideCharToMultiByte(CP_UTF8,0,s.data(),static_cast<int>(s.size()),out.data(),n,nullptr,nullptr);return out;}
std::wstring read(HWND h){const int n=GetWindowTextLengthW(h);std::wstring value(static_cast<std::size_t>(n)+1,L'\0');GetWindowTextW(h,value.data(),n+1);value.resize(static_cast<std::size_t>(n));return value;}
std::string real(double n){std::ostringstream out;out<<std::setprecision(17)<<n;return out.str();}
HWND child(HWND parent,HFONT font,const wchar_t* type,const wchar_t* title,DWORD style,int id,int x,int y,int w,int h){auto control=CreateWindowExW(0,type,title,WS_CHILD|WS_VISIBLE|style,x,y,w,h,parent,reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),GetModuleHandleW(nullptr),nullptr);SendMessageW(control,WM_SETFONT,reinterpret_cast<WPARAM>(font),TRUE);return control;}
void columns(HWND list,const std::vector<std::pair<std::wstring,int>>& cols){ListView_SetExtendedListViewStyle(list,LVS_EX_FULLROWSELECT|LVS_EX_GRIDLINES|LVS_EX_DOUBLEBUFFER);int i=0;for(const auto&[name,width]:cols){LVCOLUMNW c{};c.mask=LVCF_TEXT|LVCF_WIDTH;c.pszText=const_cast<wchar_t*>(name.c_str());c.cx=width;ListView_InsertColumn(list,i++,&c);}}
void row(HWND list,std::size_t model,const std::vector<std::string>& values){auto text=wide(values[0]);LVITEMW item{};item.mask=LVIF_TEXT|LVIF_PARAM;item.iItem=ListView_GetItemCount(list);item.pszText=text.data();item.lParam=static_cast<LPARAM>(model);const int index=ListView_InsertItem(list,&item);for(std::size_t i=1;i<values.size();++i){text=wide(values[i]);ListView_SetItemText(list,index,static_cast<int>(i),text.data());}}
void toggle_sort(TableSort& sort,int column){if(sort.column==column)sort.ascending=!sort.ascending;else{sort.column=column;sort.ascending=true;}}
struct RuleState{ObservationBindings bindings;HFONT font{};HWND ticker{},direction{},level{},list{},save{},cancel{};std::vector<PriceAlert> rules;TableSort sort;};
void fill_rules(RuleState&s){ListView_DeleteAllItems(s.list);for(std::size_t i=0;i<s.rules.size();++i){const auto&r=s.rules[i];row(s.list,i,{std::to_string(r.id),r.ticker,r.at_or_below?"Цена <=":"Цена >=",real(r.level),r.enabled?"Включено":"Выключено"});}sort_report(s.list,s.sort,s.sort.column==0||s.sort.column==3);}
LRESULT CALLBACK rules_proc(HWND window,UINT msg,WPARAM wp,LPARAM lp){auto*s=reinterpret_cast<RuleState*>(GetWindowLongPtrW(window,GWLP_USERDATA));if(msg==WM_NCCREATE){s=static_cast<RuleState*>(reinterpret_cast<CREATESTRUCTW*>(lp)->lpCreateParams);SetWindowLongPtrW(window,GWLP_USERDATA,reinterpret_cast<LONG_PTR>(s));}if(!s)return DefWindowProcW(window,msg,wp,lp);
 switch(msg){
 case WM_CREATE:{
  child(window,s->font,L"STATIC",L"Условия проверяются только для бумаг в наблюдении. Новый/включённый уровень начинает с исходной точки.",0,0,12,8,850,26);
  s->ticker=child(window,s->font,L"COMBOBOX",L"",CBS_DROPDOWNLIST|WS_VSCROLL|WS_TABSTOP,0,12,45,125,240);
  auto tickers=s->bindings.tickers();for(const auto&r:s->rules)tickers.push_back(r.ticker);std::sort(tickers.begin(),tickers.end());tickers.erase(std::unique(tickers.begin(),tickers.end()),tickers.end());for(const auto&t:tickers)SendMessageW(s->ticker,CB_ADDSTRING,0,reinterpret_cast<LPARAM>(wide(t).c_str()));if(!tickers.empty())SendMessageW(s->ticker,CB_SETCURSEL,0,0);
  s->direction=child(window,s->font,L"COMBOBOX",L"",CBS_DROPDOWNLIST|WS_TABSTOP,0,150,45,120,100);SendMessageW(s->direction,CB_ADDSTRING,0,reinterpret_cast<LPARAM>(L"Цена <="));SendMessageW(s->direction,CB_ADDSTRING,0,reinterpret_cast<LPARAM>(L"Цена >="));SendMessageW(s->direction,CB_SETCURSEL,0,0);
  s->level=child(window,s->font,L"EDIT",L"",WS_BORDER|ES_AUTOHSCROLL|WS_TABSTOP,0,282,45,150,28);
  child(window,s->font,L"BUTTON",L"Добавить",BS_PUSHBUTTON|WS_TABSTOP,ADD,445,45,110,28);child(window,s->font,L"BUTTON",L"Вкл./выкл.",BS_PUSHBUTTON|WS_TABSTOP,TOGGLE,566,45,130,28);child(window,s->font,L"BUTTON",L"Удалить",BS_PUSHBUTTON|WS_TABSTOP,REMOVE,708,45,110,28);
  s->list=child(window,s->font,WC_LISTVIEWW,L"",LVS_REPORT|LVS_SINGLESEL|WS_BORDER|WS_TABSTOP,0,12,88,810,250);columns(s->list,{{L"ID",70},{L"Тикер",110},{L"Условие",150},{L"Уровень, ₽",180},{L"Состояние",150}});
  s->save=child(window,s->font,L"BUTTON",L"Сохранить изменения",BS_PUSHBUTTON|WS_TABSTOP,SAVE,450,360,220,30);s->cancel=child(window,s->font,L"BUTTON",L"Отмена",BS_PUSHBUTTON|WS_TABSTOP,IDCANCEL,682,360,140,30);fill_rules(*s);return 0;}
 case WM_SIZE:{const int w=LOWORD(lp),h=HIWORD(lp);if(s->list)MoveWindow(s->list,12,88,w-24,std::max(100,h-145),TRUE);if(s->save)MoveWindow(s->save,w-384,h-43,220,30,TRUE);if(s->cancel)MoveWindow(s->cancel,w-152,h-43,140,30,TRUE);return 0;}
 case WM_COMMAND:try{
  if(LOWORD(wp)==IDCANCEL){DestroyWindow(window);return 0;}
  if(LOWORD(wp)==SAVE){s->bindings.save_rules(s->rules);DestroyWindow(window);return 0;}
  if(LOWORD(wp)==ADD){std::string value=narrow(read(s->level));std::replace(value.begin(),value.end(),',','.');double level{};const auto[p,e]=std::from_chars(value.data(),value.data()+value.size(),level);const auto ticker=narrow(read(s->ticker));const auto side=SendMessageW(s->direction,CB_GETCURSEL,0,0);if(e!=std::errc{}||p!=value.data()+value.size()||side<0)throw std::runtime_error("Выберите бумагу и положительный уровень цены");std::uint64_t id=0;for(const auto&r:s->rules){id=std::max(id,r.id);if(r.ticker==ticker&&r.at_or_below==(side==0)&&r.level==level)throw std::runtime_error("Такой уровень уже есть в списке");}if(id==std::numeric_limits<std::uint64_t>::max())throw std::runtime_error("Исчерпан диапазон ID условий");auto candidate=s->rules;candidate.push_back({id+1,ticker,side==0,level,true});ObservationEngine validator;validator.configure(candidate);s->rules=std::move(candidate);fill_rules(*s);return 0;}
  if(LOWORD(wp)!=REMOVE && LOWORD(wp)!=TOGGLE)return 0;
  const auto selected=selected_row_data(s->list);if(selected&&*selected<s->rules.size()){if(LOWORD(wp)==REMOVE)s->rules.erase(s->rules.begin()+static_cast<std::ptrdiff_t>(*selected));else if(LOWORD(wp)==TOGGLE)s->rules[*selected].enabled=!s->rules[*selected].enabled;fill_rules(*s);}return 0;
 }catch(const std::exception&e){MessageBoxW(window,wide(e.what()).c_str(),L"Условия не сохранены",MB_ICONWARNING);return 0;}
 case WM_NOTIFY:{const auto*n=reinterpret_cast<NMHDR*>(lp);if(n->hwndFrom==s->list&&n->code==LVN_COLUMNCLICK){toggle_sort(s->sort,reinterpret_cast<NMLISTVIEW*>(lp)->iSubItem);fill_rules(*s);return 0;}break;}
 case WM_GETMINMAXINFO:{auto*m=reinterpret_cast<MINMAXINFO*>(lp);m->ptMinTrackSize.x=860;m->ptMinTrackSize.y=440;return 0;}
 case WM_CLOSE:DestroyWindow(window);return 0;
 }return DefWindowProcW(window,msg,wp,lp);
}
struct PageState{ObservationBindings bindings;HFONT font{};HWND filter{},info{},list{};TableSort sort{0,false};std::vector<SignalEvent> snapshot;};
void fill_page(PageState&s){const auto selected=narrow(read(s.filter));auto tickers=s.bindings.tickers();s.snapshot=s.bindings.events();for(const auto&e:s.snapshot)tickers.push_back(e.ticker);std::sort(tickers.begin(),tickers.end());tickers.erase(std::unique(tickers.begin(),tickers.end()),tickers.end());SendMessageW(s.filter,CB_RESETCONTENT,0,0);SendMessageW(s.filter,CB_ADDSTRING,0,reinterpret_cast<LPARAM>(L"Все бумаги"));int index=0;for(std::size_t i=0;i<tickers.size();++i){SendMessageW(s.filter,CB_ADDSTRING,0,reinterpret_cast<LPARAM>(wide(tickers[i]).c_str()));if(tickers[i]==selected)index=static_cast<int>(i+1);}SendMessageW(s.filter,CB_SETCURSEL,static_cast<WPARAM>(index),0);
 const std::string filter=index>0?tickers[static_cast<std::size_t>(index-1)]:"";ListView_DeleteAllItems(s.list);
 for(std::size_t i=0;i<s.snapshot.size();++i){const auto&e=s.snapshot[i];if(!filter.empty()&&e.ticker!=filter)continue;row(s.list,i,{e.detected_at,e.ticker,e.kind,e.condition,real(e.price),e.quote_time,e.source_time});}sort_report(s.list,s.sort,s.sort.column==4);
 const auto text="Последние 2000 событий. Время МСК; двойной щелчок — открыть бумагу. Всплывающие уведомления: настройка на вкладке «Сигналы».\r\n"+s.bindings.error();SetWindowTextW(s.info,wide(text).c_str());
}
void open_rules(HWND page,PageState&s){RuleState state;state.bindings=s.bindings;state.font=s.font;state.rules=s.bindings.rules();const auto instance=GetModuleHandleW(nullptr);WNDCLASSEXW wc{};wc.cbSize=sizeof(wc);wc.hInstance=instance;wc.lpfnWndProc=rules_proc;wc.hCursor=LoadCursorW(nullptr,IDC_ARROW);wc.hbrBackground=reinterpret_cast<HBRUSH>(COLOR_WINDOW+1);wc.lpszClassName=L"MOEXWatchPriceRules";if(!RegisterClassExW(&wc)&&GetLastError()!=ERROR_CLASS_ALREADY_EXISTS)throw std::runtime_error("Не удалось создать окно условий");const HWND parent=GetAncestor(page,GA_ROOT);HWND window=CreateWindowExW(WS_EX_DLGMODALFRAME,wc.lpszClassName,L"Ценовые уведомления — изменения действуют после сохранения",WS_OVERLAPPEDWINDOW,CW_USEDEFAULT,CW_USEDEFAULT,900,480,parent,nullptr,instance,&state);if(!window)throw std::runtime_error("Не удалось открыть условия");EnableWindow(parent,FALSE);ShowWindow(window,SW_SHOW);MSG msg{};while(IsWindow(window)){const auto status=GetMessageW(&msg,nullptr,0,0);if(status<=0){DestroyWindow(window);if(status==0)PostQuitMessage(static_cast<int>(msg.wParam));break;}if(!IsDialogMessageW(window,&msg)){TranslateMessage(&msg);DispatchMessageW(&msg);}}if(IsWindow(parent)){EnableWindow(parent,TRUE);SetForegroundWindow(parent);}if(IsWindow(page))fill_page(s);}
LRESULT CALLBACK page_proc(HWND window,UINT message,WPARAM wp,LPARAM lp){auto*s=reinterpret_cast<PageState*>(GetWindowLongPtrW(window,GWLP_USERDATA));if(message==WM_NCCREATE){s=static_cast<PageState*>(reinterpret_cast<CREATESTRUCTW*>(lp)->lpCreateParams);SetWindowLongPtrW(window,GWLP_USERDATA,reinterpret_cast<LONG_PTR>(s));}if(!s)return DefWindowProcW(window,message,wp,lp);
 switch(message){case WM_CREATE:s->filter=child(window,s->font,L"COMBOBOX",L"",CBS_DROPDOWNLIST|WS_VSCROLL, FILTER,0,0,200,250);child(window,s->font,L"BUTTON",L"Ценовые уведомления",BS_PUSHBUTTON|WS_TABSTOP,RULES,215,0,220,28);s->info=child(window,s->font,L"STATIC",L"",SS_LEFT,0,0,35,1100,44);s->list=child(window,s->font,WC_LISTVIEWW,L"",LVS_REPORT|LVS_SINGLESEL|WS_BORDER|WS_TABSTOP,HISTORY,0,88,1100,450);columns(s->list,{{L"Обнаружено (МСК)",160},{L"Тикер",90},{L"Тип",110},{L"Условие / причина",440},{L"Цена, ₽",140},{L"Время котировки (МСК)",180},{L"SYSTIME MOEX",170}});fill_page(*s);return 0;
 case WM_SIZE:{const int w=LOWORD(lp),h=HIWORD(lp);MoveWindow(s->info,0,35,w,48,TRUE);MoveWindow(s->list,0,88,w,std::max(100,h-88),TRUE);return 0;}
 case WM_COMMAND:if(LOWORD(wp)==RULES){try{open_rules(window,*s);}catch(const std::exception&e){MessageBoxW(window,wide(e.what()).c_str(),L"MOEXWatch",MB_ICONERROR);}return 0;}if(LOWORD(wp)==FILTER&&HIWORD(wp)==CBN_SELCHANGE){fill_page(*s);return 0;}break;
 case WM_NOTIFY:{const auto*n=reinterpret_cast<NMHDR*>(lp);if(n->hwndFrom==s->list&&n->code==LVN_COLUMNCLICK){toggle_sort(s->sort,reinterpret_cast<NMLISTVIEW*>(lp)->iSubItem);fill_page(*s);return 0;}if(n->hwndFrom==s->list&&n->code==NM_DBLCLK){const auto selected=selected_row_data(s->list);if(selected&&*selected<s->snapshot.size())s->bindings.open_ticker(s->snapshot[*selected].ticker);return 0;}break;}
 case WM_NCDESTROY:SetWindowLongPtrW(window,GWLP_USERDATA,0);delete s;break;
 }return DefWindowProcW(window,message,wp,lp);
}
}
HWND create_observation_page(HWND parent,HFONT font,ObservationBindings bindings){const auto instance=GetModuleHandleW(nullptr);WNDCLASSEXW wc{};wc.cbSize=sizeof(wc);wc.hInstance=instance;wc.lpfnWndProc=page_proc;wc.hbrBackground=reinterpret_cast<HBRUSH>(COLOR_WINDOW+1);wc.hCursor=LoadCursorW(nullptr,IDC_ARROW);wc.lpszClassName=L"MOEXWatchObservationPage";if(!RegisterClassExW(&wc)&&GetLastError()!=ERROR_CLASS_ALREADY_EXISTS)return nullptr;auto state=std::make_unique<PageState>();state->bindings=std::move(bindings);state->font=font;auto* raw=state.release();HWND page=CreateWindowExW(0,wc.lpszClassName,L"",WS_CHILD,16,100,1200,600,parent,nullptr,instance,raw);return page;}
void refresh_observation_page(HWND page){if(!page)return;auto*s=reinterpret_cast<PageState*>(GetWindowLongPtrW(page,GWLP_USERDATA));if(s)fill_page(*s);}
}
#endif
