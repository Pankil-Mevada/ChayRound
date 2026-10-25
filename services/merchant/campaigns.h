#pragma once
namespace merchant {
inline void campaigns(crow::SimpleApp &app){
    {Db d(path);d.exec(R"SQL(
CREATE TABLE IF NOT EXISTS marketing_consent(office_id INTEGER PRIMARY KEY REFERENCES offices(id),enabled INTEGER NOT NULL,updated INTEGER NOT NULL);
CREATE TABLE IF NOT EXISTS marketing_controls(stall_id INTEGER PRIMARY KEY REFERENCES stalls(id),paused INTEGER NOT NULL DEFAULT 1,weekly_cap INTEGER NOT NULL DEFAULT 1);
CREATE TABLE IF NOT EXISTS campaigns(id TEXT PRIMARY KEY,stall_id INTEGER NOT NULL,body TEXT NOT NULL,created INTEGER NOT NULL,request_id TEXT NOT NULL,UNIQUE(stall_id,request_id));
CREATE TABLE IF NOT EXISTS campaign_deliveries(campaign_id TEXT NOT NULL,office_id INTEGER NOT NULL,message_id INTEGER NOT NULL,created INTEGER NOT NULL,PRIMARY KEY(campaign_id,office_id));
)SQL");}
    CROW_ROUTE(app,"/api/marketing-consent").methods("GET"_method,"POST"_method)([](const crow::request &r){return handle([&]{
        Db d(path);auto [sid,oid]=threadScope(d,r,0);(void)sid;require(!r.get_header_value("X-Office-Token").empty(),403,"Office controls its consent");
        if(r.method==crow::HTTPMethod::Post){auto j=body(r);auto enabled=flag(j,"enabled");Stmt s(d,"INSERT INTO marketing_consent VALUES(?,?,?) ON CONFLICT(office_id) DO UPDATE SET enabled=excluded.enabled,updated=excluded.updated");s.bind(1,oid).bind(2,enabled?1:0).bind(3,now()).row();audit(d,"MARKETING_CONSENT",std::to_string(oid),"office:"+std::to_string(oid),enabled?"OPT_IN":"UNSUBSCRIBED");}
        Stmt s(d,"SELECT enabled FROM marketing_consent WHERE office_id=?");s.bind(1,oid);Json out;out["enabled"]=s.row()&&s.num(0);return json(200,std::move(out));
    });});
    CROW_ROUTE(app,"/api/marketing-controls").methods("GET"_method,"POST"_method)([](const crow::request &r){return handle([&]{
        Db d(path);auto uid=owner(d,r),sid=stall(d,uid);
        if(r.method==crow::HTTPMethod::Post){auto j=body(r);auto paused=flag(j,"paused");auto cap=integer(j,"weekly_cap",1,3);Stmt s(d,"INSERT INTO marketing_controls VALUES(?,?,?) ON CONFLICT(stall_id) DO UPDATE SET paused=excluded.paused,weekly_cap=excluded.weekly_cap");s.bind(1,sid).bind(2,paused?1:0).bind(3,cap).row();}
        Stmt s(d,"SELECT paused,weekly_cap FROM marketing_controls WHERE stall_id=?");s.bind(1,sid);bool exists=s.row();Json out;out["paused"]=!exists||bool(s.num(0));out["weekly_cap"]=exists?s.num(1):1;out["channel"]="IN_APP";return json(200,std::move(out));
    });});
    CROW_ROUTE(app,"/api/campaigns").methods("POST"_method)([](const crow::request &r){return handle([&]{
        auto j=body(r);auto content=text(j,"body",1500),requestId=text(j,"request_id",64);auto maxRecipients=integer(j,"maximum_recipients",1,100);require(flag(j,"approved"),400,"Owner approval required");Db d(path);auto uid=owner(d,r),sid=stall(d,uid);Transaction tx(d);
        Stmt prior(d,"SELECT id,body FROM campaigns WHERE stall_id=? AND request_id=?");prior.bind(1,sid).bind(2,requestId);std::string id;
        if(prior.row()){require(prior.str(1)==content,409,"Campaign request reused");id=prior.str(0);}
        else {Stmt controls(d,"SELECT paused,weekly_cap FROM marketing_controls WHERE stall_id=?");controls.bind(1,sid);require(controls.row()&&!controls.num(0),409,"Marketing is paused");auto cap=controls.num(1);
            id=token();Stmt save(d,"INSERT INTO campaigns VALUES(?,?,?,?,?)");save.bind(1,id).bind(2,sid).bind(3,content).bind(4,now()).bind(5,requestId).row();
            Stmt eligible(d,"SELECT o.id FROM offices o JOIN marketing_consent c ON c.office_id=o.id WHERE o.stall_id=? AND c.enabled=1 AND o.service_enabled=1 AND (SELECT COUNT(*) FROM campaign_deliveries cd WHERE cd.office_id=o.id AND cd.created>?)<? ORDER BY o.id LIMIT ?");eligible.bind(1,sid).bind(2,now()-7*86400).bind(3,cap).bind(4,maxRecipients);
            while(eligible.row()){auto oid=eligible.num(0);Stmt message(d,"INSERT INTO messages(stall_id,office_id,sender,body,idem,created) VALUES(?,?,'OWNER',?,?,?)");message.bind(1,sid).bind(2,oid).bind(3,"Optional stall update: "+content+"\nManage optional updates in your office portal.").bind(4,"campaign-"+id).bind(5,now()).row();auto mid=sqlite3_last_insert_rowid(d.p);Stmt delivery(d,"INSERT INTO campaign_deliveries VALUES(?,?,?,?)");delivery.bind(1,id).bind(2,oid).bind(3,mid).bind(4,now()).row();}
            audit(d,"CAMPAIGN_APPROVED",id,"owner:"+std::to_string(uid),"In-app only; recipient cap="+std::to_string(maxRecipients));
        }
        Stmt count(d,"SELECT COUNT(*) FROM campaign_deliveries WHERE campaign_id=?");count.bind(1,id);count.row();Json out;out["id"]=id;out["stored_messages"]=count.num(0);out["external_sends"]=0;tx.commit();return json(200,std::move(out));
    });});
    CROW_ROUTE(app,"/internal/billing-reminder").methods("POST"_method)([](const crow::request &r){return handle([&]{
        internal(r);auto j=body(r);auto sid=integer(j,"stall_id",1,1000000000),oid=integer(j,"office_id",1,1000000000);auto content=text(j,"body",1000),key=text(j,"request_id",100);Db d(path);Transaction tx(d);Stmt office(d,"SELECT id FROM offices WHERE id=? AND stall_id=?");office.bind(1,oid).bind(2,sid);require(office.row(),404,"Office missing");
        Stmt prior(d,"SELECT id FROM messages WHERE office_id=? AND sender='SYSTEM' AND idem=?");prior.bind(1,oid).bind(2,key);
        if(!prior.row()){Stmt recent(d,"SELECT COUNT(*) FROM messages WHERE office_id=? AND sender='SYSTEM' AND created>?");recent.bind(1,oid).bind(2,now()-86400);recent.row();require(recent.num(0)<1,429,"One billing reminder per day");Stmt s(d,"INSERT INTO messages(stall_id,office_id,sender,body,idem,created) VALUES(?,?,'SYSTEM',?,?,?)");s.bind(1,sid).bind(2,oid).bind(3,content).bind(4,key).bind(5,now()).row();}tx.commit();Json out;out["stored"]=true;return json(200,std::move(out));
    });});
}
}
