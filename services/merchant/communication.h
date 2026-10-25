#pragma once
namespace merchant {
inline void communicationSchema() {
    Db d(path);
    d.exec(R"SQL(
CREATE TABLE IF NOT EXISTS messages(id INTEGER PRIMARY KEY,stall_id INTEGER NOT NULL,office_id INTEGER NOT NULL REFERENCES offices(id),sender TEXT NOT NULL,body TEXT NOT NULL,idem TEXT NOT NULL,created INTEGER NOT NULL,UNIQUE(office_id,sender,idem));
CREATE INDEX IF NOT EXISTS messages_thread ON messages(office_id,id);
CREATE TABLE IF NOT EXISTS message_reads(office_id INTEGER NOT NULL,reader TEXT NOT NULL,last_id INTEGER NOT NULL,PRIMARY KEY(office_id,reader));
CREATE TABLE IF NOT EXISTS public_profiles(stall_id INTEGER PRIMARY KEY REFERENCES stalls(id),description TEXT NOT NULL DEFAULT '',photo_url TEXT NOT NULL DEFAULT '',terms TEXT NOT NULL DEFAULT '',contact TEXT NOT NULL DEFAULT '');
)SQL");
    column(d,"office_devices","source","TEXT NOT NULL DEFAULT ''");
}
inline std::pair<long long,long long> threadScope(Db &d,const crow::request &r,long long requested) {
    if(!r.get_header_value("X-Office-Token").empty()) {
        auto oid=officeId(d,r); auto info=crow::json::load(officeInfo(d,oid).dump());
        require(info["verified"].b() && info["owner_verified"].b(),403,"Office approval required");
        require(requested==0 || requested==oid,404,"Conversation missing");
        return {info["stall_id"].i(),oid};
    }
    auto sid=stall(d,owner(d,r));
    Stmt s(d,"SELECT id FROM offices WHERE id=? AND stall_id=?");s.bind(1,requested).bind(2,sid);
    require(s.row(),404,"Office missing");return {sid,requested};
}
inline void communication(crow::SimpleApp &app) {
    communicationSchema();
    CROW_ROUTE(app,"/internal/acquisition")([](const crow::request &r){return handle([&]{
        internal(r);Db d(path);auto sid=stall(d,owner(d,r));Json out;out["offices"]=Json::list();int i=0;
        Stmt s(d,"SELECT o.id,CASE WHEN acquisition_source='' THEN 'unattributed' ELSE acquisition_source END FROM offices o WHERE stall_id=? ORDER BY o.id LIMIT 1001");s.bind(1,sid);while(s.row()){require(i<1000,409,"Report exceeds 1000 offices; export by cohort before expanding");out["offices"][i]["office_id"]=s.num(0);out["offices"][i++]["source"]=s.str(1).empty()?"direct":s.str(1);}
        out["enquiries"]=Json::list();i=0;Stmt q(d,"SELECT CASE WHEN source='' THEN 'direct' ELSE source END,COUNT(*) FROM office_devices WHERE stall_id=? GROUP BY source");q.bind(1,sid);while(q.row()){out["enquiries"][i]["source"]=q.str(0);out["enquiries"][i++]["browser_requests"]=q.num(1);}return json(200,std::move(out));
    });});
    CROW_ROUTE(app,"/internal/scheduled-office/<int>")([](const crow::request &r,int oid){return handle([&]{
        internal(r);Db d(path);auto grant=r.get_header_value("X-Office-Grant");require(grant.size()==64,401,"Office grant required");
        Stmt s(d,"SELECT id FROM offices WHERE id=? AND token_hash=? UNION ALL SELECT office_id FROM office_devices WHERE office_id=? AND token_hash=? AND status='APPROVED'");s.bind(1,oid).bind(2,grant).bind(3,oid).bind(4,grant);require(s.row(),401,"Office permission revoked");return json(200,officeInfo(d,oid,true));
    });});
    CROW_ROUTE(app,"/api/offices/<int>/billing-cycle").methods("POST"_method)([](const crow::request &r,int oid){return handle([&]{
        Db d(path);auto [sid,office]=threadScope(d,r,oid);(void)office;require(r.get_header_value("X-Office-Token").empty(),403,"Owner controls settlement terms");auto j=body(r);auto cycle=text(j,"cycle",20),reason=text(j,"reason",300);require(cycle=="MONTHLY"||cycle=="WEEKLY"||cycle=="DAILY"||cycle=="AFTER_DELIVERY",400,"Invalid billing cycle");auto version=integer(j,"version",1,1000000000);Stmt s(d,"UPDATE offices SET billing_cycle=?,version=version+1 WHERE id=? AND stall_id=? AND version=?");s.bind(1,cycle).bind(2,oid).bind(3,sid).bind(4,version).row();require(sqlite3_changes(d.p)==1,409,"Office changed; refresh");audit(d,"BILLING_CYCLE",std::to_string(oid),"owner",cycle+":"+reason);Json out;out["saved"]=true;return json(200,std::move(out));
    });});
    CROW_ROUTE(app,"/api/conversations")([](const crow::request &r){return handle([&]{
        Db d(path); auto sid=stall(d,owner(d,r)); Json out;out["offices"]=Json::list();int i=0;
        Stmt s(d,"SELECT o.id,o.name,(SELECT body FROM messages m WHERE m.office_id=o.id ORDER BY id DESC LIMIT 1),(SELECT COUNT(*) FROM messages m WHERE m.office_id=o.id AND sender='OFFICE' AND id>COALESCE((SELECT last_id FROM message_reads WHERE office_id=o.id AND reader='OWNER'),0)) FROM offices o WHERE stall_id=? ORDER BY o.name,o.id");s.bind(1,sid);
        while(s.row()){out["offices"][i]["office_id"]=s.num(0);out["offices"][i]["name"]=s.str(1);out["offices"][i]["preview"]=s.str(2);out["offices"][i++]["unread"]=s.num(3);}return json(200,std::move(out));
    });});
    CROW_ROUTE(app,"/api/conversations/<int>").methods("GET"_method,"POST"_method)([](const crow::request &r,int requested){return handle([&]{
        Db d(path);auto [sid,oid]=threadScope(d,r,requested);
        auto actor=r.get_header_value("X-Office-Token").empty()?std::string("OWNER"):std::string("OFFICE");
        if(r.method==crow::HTTPMethod::Post){
            auto j=body(r);auto content=text(j,"body",2000),key=text(j,"request_id",64);
            throttle(d,"message:"+actor+":"+std::to_string(oid),60,3600);
            Transaction tx(d);Stmt prior(d,"SELECT body,id FROM messages WHERE office_id=? AND sender=? AND idem=?");prior.bind(1,oid).bind(2,actor).bind(3,key);
            long long id;
            if(prior.row()){require(prior.str(0)==content,409,"Message request reused with different text");id=prior.num(1);}
            else {Stmt s(d,"INSERT INTO messages(stall_id,office_id,sender,body,idem,created) VALUES(?,?,?,?,?,?)");s.bind(1,sid).bind(2,oid).bind(3,actor).bind(4,content).bind(5,key).bind(6,now()).row();id=sqlite3_last_insert_rowid(d.p);audit(d,"MESSAGE_SENT",std::to_string(id),actor+":"+std::to_string(oid),"In-app delivery");}
            tx.commit();Json out;out["id"]=id;out["delivery"]="STORED";return json(200,std::move(out));
        }
        long long after=0;auto value=r.url_params.get("after");
        if(value){std::string v(value);require(!v.empty()&&v.size()<19&&std::all_of(v.begin(),v.end(),[](unsigned char c){return std::isdigit(c);}),400,"Invalid message cursor");after=std::stoll(v);}
        Stmt s(d,"SELECT id,sender,body,created FROM messages WHERE office_id=? AND id>? ORDER BY id LIMIT 101");s.bind(1,oid).bind(2,after);
        Json out;out["messages"]=Json::list();int i=0;long long last=after;bool more=false;
        while(s.row()){if(i==100){more=true;break;}last=s.num(0);out["messages"][i]["id"]=last;out["messages"][i]["sender"]=s.str(1);out["messages"][i]["body"]=s.str(2);out["messages"][i++]["created"]=s.num(3);}
        out["next_cursor"]=last;out["has_more"]=more;
        Stmt read(d,"SELECT COALESCE(MAX(last_id),0) FROM message_reads WHERE office_id=? AND reader=?");read.bind(1,oid).bind(2,actor=="OWNER"?"OFFICE":"OWNER");read.row();out["other_read_through"]=read.num(0);
        return json(200,std::move(out));
    });});
    CROW_ROUTE(app,"/api/conversations/<int>/read").methods("POST"_method)([](const crow::request &r,int requested){return handle([&]{
        Db d(path);auto [sid,oid]=threadScope(d,r,requested);(void)sid;auto j=body(r);auto id=integer(j,"last_id",1,900000000000000000LL);
        auto actor=r.get_header_value("X-Office-Token").empty()?"OWNER":"OFFICE";
        Stmt exists(d,"SELECT id FROM messages WHERE id=? AND office_id=?");exists.bind(1,id).bind(2,oid);require(exists.row(),404,"Message missing");
        Stmt s(d,"INSERT INTO message_reads VALUES(?,?,?) ON CONFLICT(office_id,reader) DO UPDATE SET last_id=MAX(last_id,excluded.last_id)");s.bind(1,oid).bind(2,actor).bind(3,id).row();Json out;out["read_through"]=id;return json(200,std::move(out));
    });});
    CROW_ROUTE(app,"/api/public/stalls/<int>")([](int sid){return handle([&]{
        Db d(path);Stmt s(d,"SELECT s.name,s.area,s.paused,s.opens,s.closes,s.days,p.description,p.photo_url,p.terms,p.contact FROM stalls s JOIN owners o ON o.id=s.owner_id LEFT JOIN public_profiles p ON p.stall_id=s.id WHERE s.id=? AND o.verified=1");s.bind(1,sid);require(s.row(),404,"Stall unavailable");Json out;out["id"]=sid;out["name"]=s.str(0);out["area"]=s.str(1);out["ordering_open"]=!s.num(2)&&!holiday(d,sid)&&open(s.num(3),s.num(4),s.num(5));out["opens"]=s.num(3);out["closes"]=s.num(4);out["days"]=s.num(5);out["description"]=s.str(6);out["photo_url"]=s.str(7);out["terms"]=s.str(8);out["contact"]=s.str(9);out["menu"]=menu(d,sid);return json(200,std::move(out));
    });});
    CROW_ROUTE(app,"/api/public-profile").methods("POST"_method)([](const crow::request &r){return handle([&]{
        auto j=body(r);auto description=optionalText(j,"description",1000),photo=optionalText(j,"photo_url",500),terms=optionalText(j,"terms",1000),contact=optionalText(j,"contact",200);
        require(photo.empty()||photo.rfind("https://",0)==0,400,"Photo must use HTTPS");Db d(path);auto uid=owner(d,r),sid=stall(d,uid);
        Stmt s(d,"INSERT INTO public_profiles VALUES(?,?,?,?,?) ON CONFLICT(stall_id) DO UPDATE SET description=excluded.description,photo_url=excluded.photo_url,terms=excluded.terms,contact=excluded.contact");s.bind(1,sid).bind(2,description).bind(3,photo).bind(4,terms).bind(5,contact).row();Json out;out["saved"]=true;return json(200,std::move(out));
    });});
}
}
