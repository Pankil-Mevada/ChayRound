#pragma once
namespace orders {
inline long long indiaDateStart(const std::string &date){
    require(date.size()==10&&date[4]=='-'&&date[7]=='-',400,"Use YYYY-MM-DD");auto start=monthStart(date.substr(0,7));auto day=std::stoi(date.substr(8));require(day>=1&&day<=31,400,"Invalid day");auto result=start+(day-1)*86400;require(result<nextMonth(date.substr(0,7)),400,"Invalid calendar day");require(date.substr(8)==(day<10?"0":"")+std::to_string(day),400,"Invalid day format");return result;
}
inline void settlement(crow::SimpleApp &app){
    CROW_ROUTE(app,"/api/accounts/<int>/settlement-statement").methods("POST"_method)([](const crow::request &r,int oid){return handle([&]{
        auto o=ownedOffice(r,oid);auto j=body(r);auto cycle=text(j,"cycle",20),date=text(j,"start_date",10);require(cycle=="DAILY"||cycle=="WEEKLY"||cycle=="AFTER_DELIVERY",400,"Choose daily, weekly or after-delivery settlement");auto start=indiaDateStart(date),end=start+(cycle=="WEEKLY"?7:1)*86400;require(end<=now(),409,"Statement period must be closed");auto period=cycle+":"+date;auto sid=o["stall_id"].i();Db d(path);Transaction tx(d);Stmt old(d,"SELECT id FROM invoices WHERE office_id=? AND period=?");old.bind(1,oid).bind(2,period);Json out;
        if(old.row()){out["invoice_id"]=old.str(0);tx.commit();return json(200,std::move(out));}
        auto id=token();Stmt insert(d,"INSERT INTO invoices VALUES(?,?,?,?,?,?)");insert.bind(1,id).bind(2,sid).bind(3,oid).bind(4,period).bind(5,end+(cycle=="WEEKLY"?7:1)*86400).bind(6,now()).row();
        Stmt lines(d,"INSERT INTO invoice_lines SELECT ?,l.id,l.amount FROM ledger l JOIN orders o ON o.id=l.order_id WHERE l.office_id=? AND l.kind IN ('CHARGE','DEBIT_ADJUSTMENT') AND o.policy='MONTHLY' AND o.billing_cycle=? AND l.created>=? AND l.created<? AND NOT EXISTS(SELECT 1 FROM invoice_lines il WHERE il.debit_id=l.id)");lines.bind(1,id).bind(2,oid).bind(3,cycle).bind(4,start).bind(5,end).row();audit(d,"SETTLEMENT_STATEMENT",id,"owner",period);tx.commit();out["invoice_id"]=id;return json(201,std::move(out));
    });});
}
}
