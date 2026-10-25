#pragma once
namespace orders {
inline void advanceSchema(){Db d(path);d.exec(R"SQL(
CREATE TABLE IF NOT EXISTS advance_entries(id TEXT PRIMARY KEY,stall_id INTEGER NOT NULL,office_id INTEGER NOT NULL,amount INTEGER NOT NULL,kind TEXT NOT NULL,reference TEXT NOT NULL,idem TEXT NOT NULL,request_hash TEXT NOT NULL,created INTEGER NOT NULL,UNIQUE(stall_id,idem),UNIQUE(stall_id,reference));
CREATE TRIGGER IF NOT EXISTS advance_no_update BEFORE UPDATE ON advance_entries BEGIN SELECT RAISE(ABORT,'immutable advance record'); END;
CREATE TRIGGER IF NOT EXISTS advance_no_delete BEFORE DELETE ON advance_entries BEGIN SELECT RAISE(ABORT,'immutable advance record'); END;
)SQL");}
inline void requireFinancialReview(){require(!env("CHAY_FINANCIAL_REVIEW_ID").empty(),503,"Advance money operations require a recorded provider and financial review");}
inline long long advanceBalance(Db &d,long long oid){Stmt s(d,"SELECT COALESCE(SUM(amount),0) FROM advance_entries WHERE office_id=?");s.bind(1,oid);s.row();return s.num(0);}
inline long long advanceReserved(Db &d,long long oid){Stmt s(d,"SELECT COALESCE(SUM(total),0) FROM orders WHERE office_id=? AND policy='ADVANCE' AND status IN ('PLACED','ACCEPTED','READY')");s.bind(1,oid);s.row();return s.num(0);}
inline void spendAdvance(Db &d,const std::string &id,long long sid,long long oid,long long amount,const std::string &actor){
    requireFinancialReview();require(advanceBalance(d,oid)>=amount,409,"Advance balance is insufficient");
    Stmt s(d,"INSERT INTO advance_entries VALUES(?,?,?,?,?,?,?,?,?)");s.bind(1,"spend_"+id).bind(2,sid).bind(3,oid).bind(4,-amount).bind(5,"ORDER_DEDUCTION").bind(6,"order:"+id).bind(7,"spend-"+id).bind(8,digest(id)).bind(9,now()).row();
    entry(d,"charge_"+id,sid,oid,id,"CHARGE",amount,"Fulfilled advance order",actor);
    entry(d,"advance_"+id,sid,oid,id,"PAYMENT",-amount,"Advance balance deduction",actor);
    Stmt allocation(d,"INSERT INTO allocations(id,credit_id,debit_id,amount) VALUES(?,?,?,?)");allocation.bind(1,token()).bind(2,"advance_"+id).bind(3,"charge_"+id).bind(4,amount).row();
}
inline void advances(crow::SimpleApp &app){
    advanceSchema();
    CROW_ROUTE(app,"/api/advance")([](const crow::request &r){return handle([&]{auto o=office(r);Db d(path);auto oid=o["office_id"].i();Json out;out["balance_paise"]=advanceBalance(d,oid);out["reserved_paise"]=advanceReserved(d,oid);out["available_paise"]=advanceBalance(d,oid)-advanceReserved(d,oid);out["enabled"]=!env("CHAY_FINANCIAL_REVIEW_ID").empty();Stmt points(d,"SELECT COALESCE(SUM(points),0) FROM loyalty_points WHERE office_id=?");points.bind(1,oid);points.row();out["loyalty_points"]=points.num(0);return json(200,std::move(out));});});
    CROW_ROUTE(app,"/api/accounts/<int>/advance").methods("GET"_method,"POST"_method)([](const crow::request &r,int oid){return handle([&]{
        auto o=ownedOffice(r,oid);auto who=owner(r);auto sid=who["stall_id"].i();Db d(path);
        if(r.method==crow::HTTPMethod::Post){requireFinancialReview();auto j=body(r);auto amount=integer(j,"amount_paise",1,100000000);auto kind=text(j,"kind",20),ref=text(j,"reference",120),method=text(j,"method",10),key=idem(r);require(kind=="TOPUP"||kind=="REFUND",400,"Use TOPUP or REFUND");require(method=="CASH"||method=="BANK",400,"Record confirmed cash or bank receipt only");require(flag(j,"confirmed"),400,"Confirm the actual money receipt or refund");auto hash=digest(std::to_string(oid)+":"+std::to_string(amount)+":"+kind+":"+method+":"+ref);
            Transaction tx(d);Stmt prior(d,"SELECT id,request_hash FROM advance_entries WHERE stall_id=? AND idem=?");prior.bind(1,sid).bind(2,key);std::string id;
            if(prior.row()){require(prior.str(1)==hash,409,"Advance request reused");id=prior.str(0);}
            else {if(kind=="TOPUP"){Stmt receipt(d,"SELECT id FROM receipts WHERE stall_id=? AND reference=?");receipt.bind(1,sid).bind(2,ref);require(!receipt.row(),409,"Money reference already recorded as a credit-account receipt");}Stmt dup(d,"SELECT id FROM advance_entries WHERE stall_id=? AND reference=?");dup.bind(1,sid).bind(2,ref);require(!dup.row(),409,"Advance reference already recorded");if(kind=="REFUND"){Stmt cash(d,"SELECT COALESCE(SUM(CASE WHEN kind='REFERRAL_REWARD' THEN 0 ELSE amount END),0) FROM advance_entries WHERE office_id=?");cash.bind(1,oid);cash.row();require(amount<=std::min(cash.num(0),advanceBalance(d,oid)-advanceReserved(d,oid)),409,"Refund exceeds unreserved cash-funded balance");}else require(advanceBalance(d,oid)+amount<=100000000,409,"Advance balance limit exceeded");id=token();Stmt save(d,"INSERT INTO advance_entries VALUES(?,?,?,?,?,?,?,?,?)");save.bind(1,id).bind(2,sid).bind(3,oid).bind(4,kind=="TOPUP"?amount:-amount).bind(5,kind).bind(6,ref).bind(7,key).bind(8,hash).bind(9,now()).row();audit(d,"ADVANCE_"+kind,id,"owner:"+std::to_string(who["owner_id"].i()),method+":"+ref+"; review="+env("CHAY_FINANCIAL_REVIEW_ID"));}tx.commit();Json out;out["id"]=id;return json(200,std::move(out));
        }
        Json out;out["balance_paise"]=advanceBalance(d,oid);out["reserved_paise"]=advanceReserved(d,oid);out["enabled"]=!env("CHAY_FINANCIAL_REVIEW_ID").empty();out["entries"]=Json::list();Stmt s(d,"SELECT id,amount,kind,reference,created FROM advance_entries WHERE office_id=? ORDER BY created DESC,rowid DESC LIMIT 100");s.bind(1,oid);int i=0;while(s.row()){out["entries"][i]["id"]=s.str(0);out["entries"][i]["amount_paise"]=s.num(1);out["entries"][i]["kind"]=s.str(2);out["entries"][i]["reference"]=s.str(3);out["entries"][i++]["created"]=s.num(4);}return json(200,std::move(out));
    });});
}
}
