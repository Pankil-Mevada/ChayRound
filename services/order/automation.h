#pragma once
namespace orders {
inline void automationSchema(){Db d(path);d.exec(R"SQL(
CREATE TABLE IF NOT EXISTS recurring_rounds(id TEXT PRIMARY KEY,office_id INTEGER NOT NULL,stall_id INTEGER NOT NULL,grant_hash TEXT NOT NULL,basket TEXT NOT NULL,days INTEGER NOT NULL,minute INTEGER NOT NULL,maximum INTEGER NOT NULL,paused INTEGER NOT NULL DEFAULT 0,last_day INTEGER NOT NULL DEFAULT 0,last_result TEXT NOT NULL DEFAULT '',created INTEGER NOT NULL);
CREATE TABLE IF NOT EXISTS recurring_exceptions(round_id TEXT NOT NULL REFERENCES recurring_rounds(id),day INTEGER NOT NULL,PRIMARY KEY(round_id,day));
CREATE TABLE IF NOT EXISTS recurring_attempts(round_id TEXT NOT NULL,day INTEGER NOT NULL,status INTEGER NOT NULL,detail TEXT NOT NULL,PRIMARY KEY(round_id,day));
)SQL");column(d,"recurring_rounds","archived","INTEGER NOT NULL DEFAULT 0");}
inline long long indiaDay(){return (now()+19800)/86400;}
inline void runRecurring(){
    Db d(path);auto day=indiaDay(),minute=((now()+19800)%86400)/60;
    auto stamp=static_cast<time_t>(now()+19800);std::tm local{};gmtime_r(&stamp,&local);
    struct Due {std::string id,grant,basket;long long oid,maximum,minute;};std::vector<Due> pending;
    {Stmt due(d,"SELECT id,office_id,grant_hash,basket,maximum,minute FROM recurring_rounds WHERE paused=0 AND archived=0 AND (days & ?)!=0 AND minute<=? AND last_day<? ORDER BY created LIMIT 50");due.bind(1,1<<local.tm_wday).bind(2,minute).bind(3,day);
    while(due.row())pending.push_back({due.str(0),due.str(2),due.str(3),due.num(1),due.num(4),due.num(5)});}
    for(const auto &due:pending){
        auto id=due.id;int status=409;std::string detail="Round missed its 15-minute window";
        Stmt skip(d,"SELECT 1 FROM recurring_exceptions WHERE round_id=? AND day=?");skip.bind(1,id).bind(2,day);
        if(skip.row())detail="Skipped by office";
        else if(minute-due.minute<=15){
            try{
                auto response=http(env("CHAY_MERCHANT_URL","http://127.0.0.1:9181")+"/internal/scheduled-office/"+std::to_string(due.oid),"GET","",{"X-Internal-Key: "+serviceKey(),"X-Office-Grant: "+due.grant});
                require(response.status==200,409,"Office permission revoked or unavailable");auto identity=crow::json::load(response.body);require(bool(identity),503,"Invalid office response");
                require(std::string(identity["policy"].s())=="MONTHLY",409,"Automatic rounds require approved credit terms");
                auto basket=crow::json::load(due.basket);require(bool(basket),500,"Invalid saved basket");Json order(basket);order["maximum_total_paise"]=due.maximum;order["note"]="Explicitly enabled recurring office round";order["internal_round_id"]=id;order["internal_round_day"]=day;
                crow::request request;request.body=order.dump();request.add_header("Idempotency-Key","round-"+id+"-"+std::to_string(day));
                auto created=placeOrder(request,identity);status=created.code;detail=created.body;
            }catch(const std::exception &e){detail=e.what();status=409;}
        }
        Transaction tx(d);Stmt record(d,"INSERT OR IGNORE INTO recurring_attempts VALUES(?,?,?,?)");record.bind(1,id).bind(2,day).bind(3,status).bind(4,detail).row();
        Stmt finish(d,"UPDATE recurring_rounds SET last_day=?,last_result=? WHERE id=?");finish.bind(1,day).bind(2,(status>=200&&status<300)?"ORDER_CREATED":detail.substr(0,300)).bind(3,id).row();tx.commit();
    }
}
inline void automation(crow::SimpleApp &app){
    automationSchema();reminderSchema();
    CROW_ROUTE(app,"/api/rounds").methods("GET"_method,"POST"_method)([](const crow::request &r){return handle([&]{
        auto o=office(r);auto oid=o["office_id"].i(),sid=o["stall_id"].i();Db d(path);
        if(r.method==crow::HTTPMethod::Post){auto j=body(r);require(flag(j,"consent"),400,"Explicit recurring-order consent required");require(std::string(o["policy"].s())=="MONTHLY",409,"Recurring rounds require owner-approved credit");
            auto days=integer(j,"days",1,127),minute=integer(j,"minute",0,1439),maximum=integer(j,"maximum_total_paise",1,100000000);
            require(j.has("items")&&j["items"].t()==crow::json::type::List&&j["items"].size()>0&&j["items"].size()<=20,400,"Choose 1-20 menu lines");Json basket;basket["items"]=Json(j["items"]);long long cups=0;
            for(auto &line:j["items"]){auto item=integer(line,"item_id",1,1000000000),qty=integer(line,"quantity",1,500);cups+=qty;bool found=false;for(auto &m:o["menu"])if(m["id"].i()==item&&m["available"].b())found=true;require(found,409,"Menu item unavailable");}require(cups<=500,400,"Round exceeds 500 items");
            Transaction tx(d);Stmt count(d,"SELECT COUNT(*) FROM recurring_rounds WHERE office_id=? AND archived=0");count.bind(1,oid);count.row();require(count.num(0)<10,409,"Maximum ten recurring rounds per office");
            auto id=token();Stmt s(d,"INSERT INTO recurring_rounds(id,office_id,stall_id,grant_hash,basket,days,minute,maximum,created) VALUES(?,?,?,?,?,?,?,?,?)");s.bind(1,id).bind(2,oid).bind(3,sid).bind(4,digest(r.get_header_value("X-Office-Token"))).bind(5,basket.dump()).bind(6,days).bind(7,minute).bind(8,maximum).bind(9,now()).row();audit(d,"ROUND_CONSENT",id,"office:"+std::to_string(oid),"Credit, current prices up to approved ceiling, 15-minute window");tx.commit();Json out;out["id"]=id;return json(201,std::move(out));}
        Stmt s(d,"SELECT id,days,minute,maximum,paused,last_result,archived FROM recurring_rounds WHERE office_id=? ORDER BY created");s.bind(1,oid);Json out;out["rounds"]=Json::list();int i=0;while(s.row()){out["rounds"][i]["id"]=s.str(0);out["rounds"][i]["days"]=s.num(1);out["rounds"][i]["minute"]=s.num(2);out["rounds"][i]["maximum_total_paise"]=s.num(3);out["rounds"][i]["paused"]=bool(s.num(4));out["rounds"][i]["last_result"]=s.str(5);out["rounds"][i++]["cancelled"]=bool(s.num(6));}return json(200,std::move(out));
    });});
    CROW_ROUTE(app,"/api/rounds/<string>").methods("POST"_method)([](const crow::request &r,std::string id){return handle([&]{
        auto o=office(r);auto j=body(r);Db d(path);Transaction tx(d);Stmt exists(d,"SELECT archived FROM recurring_rounds WHERE id=? AND office_id=?");exists.bind(1,id).bind(2,o["office_id"].i());require(exists.row(),404,"Round missing");require(!exists.num(0),409,"Cancelled round cannot be resumed");
        if(j.has("cancelled")){require(flag(j,"cancelled"),400,"Use pause/resume for active rounds");Stmt s(d,"UPDATE recurring_rounds SET paused=1,archived=1 WHERE id=?");s.bind(1,id).row();}
        else if(j.has("skip_day")){auto day=integer(j,"skip_day",indiaDay(),indiaDay()+366);Stmt skip(d,"INSERT OR IGNORE INTO recurring_exceptions VALUES(?,?)");skip.bind(1,id).bind(2,day).row();}
        else {auto paused=flag(j,"paused");Stmt s(d,"UPDATE recurring_rounds SET paused=? WHERE id=?");s.bind(1,paused?1:0).bind(2,id).row();}
        audit(d,"ROUND_UPDATED",id,"office:"+std::to_string(o["office_id"].i()),Json(j).dump());tx.commit();Json out;out["saved"]=true;return json(200,std::move(out));
    });});
    CROW_ROUTE(app,"/internal/marketing/tick").methods("POST"_method)([](const crow::request &r){return handle([&]{internal(r);runMarketing();Json out;out["processed"]=true;return json(200,std::move(out));});});
    CROW_ROUTE(app,"/internal/reminders/tick").methods("POST"_method)([](const crow::request &r){return handle([&]{internal(r);runBillingReminders();Json out;out["processed"]=true;return json(200,std::move(out));});});
    CROW_ROUTE(app,"/internal/rounds/tick").methods("POST"_method)([](const crow::request &r){return handle([&]{internal(r);runRecurring();Json out;out["processed"]=true;return json(200,std::move(out));});});
}
inline std::jthread recurringWorker(){
    if(env("CHAY_RECURRING_WORKER","0")!="1"&&env("CHAY_BILLING_REMINDERS","0")!="1"&&env("CHAY_MARKETING_AUTOMATION","0")!="1")return {};
    return std::jthread([](std::stop_token stop){std::mutex mutex;std::condition_variable_any wake;while(!stop.stop_requested()){try{if(env("CHAY_RECURRING_WORKER","0")=="1")runRecurring();if(env("CHAY_BILLING_REMINDERS","0")=="1")runBillingReminders();if(env("CHAY_MARKETING_AUTOMATION","0")=="1")runMarketing();}catch(const std::exception&){}std::unique_lock lock(mutex);wake.wait_for(lock,stop,std::chrono::seconds(30),[]{return false;});}});
}
}
