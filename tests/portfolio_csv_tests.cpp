#include "portfolio_csv.hpp"
#include <cmath>
#include <iostream>
#include <stdexcept>
using namespace moexwatch;
namespace {void check(bool b,const char* m){if(!b)throw std::runtime_error(m);}const std::string header="date;ticker;side;quantity;price;commission\n";}
int main(){try{
 const PortfolioTransaction buy{"2026-10-01","SBER",PortfolioSide::Buy,10,100.123456789,0.4};
 const std::vector<PortfolioTransaction> existing{buy};
 auto csv=header+"2026-10-01;SBER;BUY;10;100.123456789;0.4\n2026-10-02;SBER;SELL;2;110;0.1\n2026-10-02;SBER;SELL;2;110;0.1\n";
 auto preview=preview_portfolio_csv(csv,existing);check(preview.error.empty()&&preview.rows.size()==3,"preview parses all rows");
 check(preview.rows[0].status==ImportStatus::Duplicate&&preview.rows[1].status==ImportStatus::New&&preview.rows[2].status==ImportStatus::Duplicate,"matches existing and duplicate inside input detected");
 std::string error;auto merged=prepare_portfolio_import(preview,existing,false,error);
 check(merged&&merged->size()==2&&calculate_portfolio(*merged,{})->positions[0].quantity==8,"default import skips possible duplicates");
 const auto included=prepare_portfolio_import(preview,existing,true,error);
 check(included&&included->size()==4&&calculate_portfolio(*included,{})->positions[0].quantity==16,"identical genuine trades can explicitly be included");
 check(existing.size()==1&&existing[0].quantity==10,"preview never mutates existing journal");
 auto again=preview_portfolio_csv(csv,*merged);check(!prepare_portfolio_import(again,*merged,false,error),"repeated import is idempotent by default");
 auto bad=preview_portfolio_csv(header+"2026-02-30;SBER;BUY;1;10;0\n2026-10-02;SBER;BUY;1;10;0\n",existing);
 check(bad.rows[0].status==ImportStatus::Error&&!prepare_portfolio_import(bad,existing,false,error),"one invalid row blocks whole file, not partial success");
 const auto oversell=preview_portfolio_csv(header+"2026-10-02;SBER;SELL;11;100;0\n",existing);
 check(!prepare_portfolio_import(oversell,existing,false,error),"sell beyond available quantity blocked");
 const auto early=preview_portfolio_csv(header+"2026-09-30;SBER;SELL;1;100;0\n",existing);
 check(!prepare_portfolio_import(early,existing,false,error),"cannot sell before a historical buy");
 const auto order=preview_portfolio_csv(header+"2026-10-01;GAZP;SELL;1;100;0\n2026-10-01;GAZP;BUY;1;100;0\n",{});
 check(!prepare_portfolio_import(order,{},false,error),"same-day CSV order preserved, no fabricated buy-before-sell sort");
 const auto reordered=preview_portfolio_csv("side;price;date;commission;ticker;quantity\r\nBUY;\"10,25\";2026-10-02;0;sber;2\r\n",{});
 check(reordered.error.empty()&&reordered.rows[0].transaction->ticker=="SBER"&&reordered.rows[0].transaction->price==10.25,"header order, CRLF, quoted decimal comma and lowercase ticker handled");
 const auto comma=preview_portfolio_csv("date,ticker,side,quantity,price,commission\n2026-10-02,SBER,BUY,2,\"10,25\",0\n",{});
 check(comma.rows[0].transaction->price==10.25,"comma delimiter with quoted decimal comma");
 check(!preview_portfolio_csv(header+"2026-10-02;SBER;BUY;1;\"100;0\n",{}).error.empty(),"unclosed quotes reject file");
 check(!preview_portfolio_csv(header+"2026-10-02;SBER;BUY;1;\"100\"oops;0\n",{}).error.empty(),"characters after quotes rejected");
 check(!preview_portfolio_csv("date;ticker;side;quantity;price;price\n",{}).error.empty(),"repeated header rejected");
 const auto nan=preview_portfolio_csv(header+"2026-10-02;SBER;BUY;nan;100;0\n",{});
 check(nan.rows[0].status==ImportStatus::Error,"nonfinite numeric input rejected");
 auto emptyfee=preview_portfolio_csv(header+"2026-10-02;SBER;BUY;1;100;\n",{});
 check(emptyfee.rows[0].status==ImportStatus::Error,"missing commission not silently converted to zero");
 const auto exported=export_portfolio_csv(existing);check(exported.starts_with("\xEF\xBB\xBF"),"export contains UTF-8 BOM for Excel");
 const auto back=preview_portfolio_csv(exported,{});const auto restored=prepare_portfolio_import(back,{},false,error);
 check(restored&&restored->size()==1&&restored->at(0).price==buy.price&&restored->at(0).commission==buy.commission,"export and import preserve exact double values");
 check(!prepare_portfolio_import(preview_portfolio_csv(exported,existing),existing,false,error),"exported own journal is recognized as duplicate");
 const auto portfolio=calculate_portfolio({buy,{"2026-10-01","GAZP",PortfolioSide::Buy,10,100,0}},{{"SBER",100,{},""},{"GAZP",300,{},""}});
 check(portfolio&&portfolio->open_positions==2,"allocation fixture");double total=0;for(const auto&p:portfolio->positions){const auto w=portfolio_weight(*portfolio,p,true);check(w.has_value(),"complete fresh prices produce shares");total+=*w;}
 check(std::abs(total-100)<1e-10,"complete allocation sums to 100 percent");
 check(!portfolio_weight(*portfolio,portfolio->positions[0],false),"stale quotes hide all weights");
 const auto partial=calculate_portfolio({buy,{"2026-10-01","GAZP",PortfolioSide::Buy,10,100,0}},{{"SBER",100,{},""}});
 check(partial&&!portfolio_weight(*partial,partial->positions[0],true),"missing quote does not turn priced subset into misleading 100 percent");
 check(!preview_portfolio_csv(std::string(4U*1024U*1024U+1,'a'),{}).error.empty(),"file size bounded");
 std::cout<<"portfolio_csv_tests: all checks passed\n";
 }catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
