#pragma once
namespace orders {
inline void rewards(crow::SimpleApp &app){
    {Db d(path);d.exec(R"SQL(
CREATE TABLE IF NOT EXISTS reward_rules(stall_id INTEGER PRIMARY KEY,reward INTEGER NOT NULL,budget INTEGER NOT NULL,paused INTEGER NOT NULL DEFAULT 1);
CREATE TABLE IF NOT EXISTS referrals(stall_id INTEGER NOT NULL,referred INTEGER PRIMARY KEY,referrer INTEGER NOT NULL,created INTEGER NOT NULL,CHECK(referred<>referrer));
CREATE TABLE IF NOT EXISTS referral_rewards(referred INTEGER PRIMARY KEY,stall_id INTEGER NOT NULL,referrer INTEGER NOT NULL,qualifying_order TEXT NOT NULL UNIQUE,amount INTEGER NOT NULL,created INTEGER NOT NULL);
CREATE TABLE IF NOT EXISTS loyalty_points(order_id TEXT PRIMARY KEY,stall_id INTEGER NOT NULL,office_id INTEGER NOT NULL,points INTEGER NOT NULL,created INTEGER NOT NULL);
)SQL");}
    CROW_ROUTE(app,"/api/reward-rules").methods("GET"_method,"POST"_method)([](const crow::request &r){return handle([&]{
        auto who=owner(r);auto sid=who["stall_id"].i();Db d(path);
        if(r.method==crow::HTTPMethod::Post){auto j=body(r);auto reward=integer(j,"reward_paise",0,100000),budget=integer(j,"budget_paise",0,10000000);auto paused=flag(j,"paused");require(paused||reward>0,400,"Enabled reward must be positive");Stmt s(d,"INSERT INTO reward_rules VALUES(?,?,?,?) ON CONFLICT(stall_id) DO UPDATE SET reward=excluded.reward,budget=excluded.budget,paused=excluded.paused");s.bind(1,sid).bind(2,reward).bind(3,budget).bind(4,paused?1:0).row();audit(d,"REWARD_RULES",std::to_string(sid),"owner:"+std::to_string(who["owner_id"].i()),Json(j).dump());}
        Stmt rules(d,"SELECT reward,budget,paused FROM reward_rules WHERE stall_id=?");rules.bind(1,sid);bool exists=rules.row();Json out;out["reward_paise"]=exists?rules.num(0):0;out["budget_paise"]=exists?rules.num(1):0;out["paused"]=!exists||bool(rules.num(2));Stmt spent(d,"SELECT COALESCE(SUM(amount),0) FROM referral_rewards WHERE stall_id=?");spent.bind(1,sid);spent.row();out["spent_paise"]=spent.num(0);out["financial_review_recorded"]=!env("CHAY_FINANCIAL_REVIEW_ID").empty();return json(200,std::move(out));
    });});
    CROW_ROUTE(app,"/api/accounts/<int>/referral").methods("POST"_method)([](const crow::request &r,int referred){return handle([&]{
        auto j=body(r);auto referrer=integer(j,"referrer_office_id",1,1000000000);require(referrer!=referred,400,"Self referral is not allowed");require(flag(j,"identity_checked"),400,"Confirm both office identities");auto target=ownedOffice(r,referred),source=ownedOffice(r,referrer);auto sid=target["stall_id"].i();
        auto normalize=[](std::string v){std::transform(v.begin(),v.end(),v.begin(),[](unsigned char c){return std::tolower(c);});v.erase(std::remove_if(v.begin(),v.end(),[](unsigned char c){return std::isspace(c);}),v.end());return v;};
        require(normalize(std::string(target["office_name"].s()))!=normalize(std::string(source["office_name"].s()))&&normalize(std::string(target["address"].s()))!=normalize(std::string(source["address"].s())),409,"Duplicate office identity needs review");
        auto email=std::string(target["email"].s());require(email.empty()||email!=std::string(source["email"].s()),409,"Referral contacts match");Db d(path);Transaction tx(d);Stmt prior(d,"SELECT referrer FROM referrals WHERE referred=?");prior.bind(1,referred);
        if(prior.row())require(prior.num(0)==referrer,409,"Referral already attributed");else{Stmt existing(d,"SELECT 1 FROM orders WHERE office_id=? AND status='FULFILLED'");existing.bind(1,referred);require(!existing.row(),409,"Attribute referrals before first fulfilment");Stmt s(d,"INSERT INTO referrals VALUES(?,?,?,?)");s.bind(1,sid).bind(2,referred).bind(3,referrer).bind(4,now()).row();audit(d,"REFERRAL_CONFIRMED",std::to_string(referred),"owner","referrer="+std::to_string(referrer));}tx.commit();Json out;out["saved"]=true;return json(200,std::move(out));
    });});
    CROW_ROUTE(app,"/api/accounts/<int>/referral-reward").methods("POST"_method)([](const crow::request &r,int referred){return handle([&]{
        requireFinancialReview();auto target=ownedOffice(r,referred);auto sid=target["stall_id"].i();Db d(path);Transaction tx(d);Stmt prior(d,"SELECT amount FROM referral_rewards WHERE referred=? AND stall_id=?");prior.bind(1,referred).bind(2,sid);Json out;
        if(prior.row()){out["amount_paise"]=prior.num(0);tx.commit();return json(200,std::move(out));}
        Stmt referral(d,"SELECT referrer FROM referrals WHERE referred=? AND stall_id=?");referral.bind(1,referred).bind(2,sid);require(referral.row(),404,"Confirmed referral missing");auto referrer=referral.num(0);
        Stmt qualified(d,"SELECT id FROM orders WHERE office_id=? AND status='FULFILLED' AND settlement='PAID' AND policy!='ADVANCE' ORDER BY fulfilled_at,id LIMIT 1");qualified.bind(1,referred);require(qualified.row(),409,"Referral requires paid fulfilment funded outside rewards");auto orderId=qualified.str(0);
        Stmt rules(d,"SELECT reward,budget,paused FROM reward_rules WHERE stall_id=?");rules.bind(1,sid);require(rules.row()&&!rules.num(2)&&rules.num(0)>0,409,"Referral rewards paused");auto amount=rules.num(0),budget=rules.num(1);Stmt total(d,"SELECT COALESCE(SUM(amount),0) FROM referral_rewards WHERE stall_id=?");total.bind(1,sid);total.row();require(total.num(0)+amount<=budget,409,"Referral reward budget exhausted");require(advanceBalance(d,referrer)+amount<=100000000,409,"Advance balance limit exceeded");
        Stmt save(d,"INSERT INTO referral_rewards VALUES(?,?,?,?,?,?)");save.bind(1,referred).bind(2,sid).bind(3,referrer).bind(4,orderId).bind(5,amount).bind(6,now()).row();auto id="reward_"+orderId;
        Stmt credit(d,"INSERT INTO advance_entries VALUES(?,?,?,?,?,?,?,?,?)");credit.bind(1,id).bind(2,sid).bind(3,referrer).bind(4,amount).bind(5,"REFERRAL_REWARD").bind(6,id).bind(7,id).bind(8,digest(id)).bind(9,now()).row();audit(d,"REFERRAL_REWARDED",std::to_string(referred),"owner",std::to_string(amount));tx.commit();out["amount_paise"]=amount;return json(200,std::move(out));
    });});
    CROW_ROUTE(app,"/api/orders/<string>/loyalty").methods("POST"_method)([](const crow::request &r,std::string id){return handle([&]{
        auto o=office(r);auto oid=o["office_id"].i();Db d(path);Transaction tx(d);Stmt order(d,"SELECT total,stall_id FROM orders WHERE id=? AND office_id=? AND status='FULFILLED' AND settlement='PAID'");order.bind(1,id).bind(2,oid);require(order.row(),409,"Loyalty requires your paid fulfilled order");auto points=std::min(100LL,order.num(0)/100);Stmt s(d,"INSERT OR IGNORE INTO loyalty_points VALUES(?,?,?,?,?)");s.bind(1,id).bind(2,order.num(1)).bind(3,oid).bind(4,points).bind(5,now()).row();tx.commit();Json out;out["points"]=points;out["cash_value_paise"]=0;return json(200,std::move(out));
    });});
}
}
