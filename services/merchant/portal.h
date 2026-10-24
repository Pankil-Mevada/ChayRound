#pragma once
namespace merchant {
inline void portal(crow::SimpleApp &app) {
    CROW_ROUTE(app, "/api/stalls")([] {
        return handle([] {
            Db d(path);
            Stmt s(d, "SELECT s.id,s.name,s.area FROM stalls s JOIN owners o ON o.id=s.owner_id WHERE o.verified=1 ORDER BY s.name,s.id LIMIT 100");
            Json out; out["stalls"] = Json::list(); int i=0;
            while(s.row()) { out["stalls"][i]["id"]=s.num(0); out["stalls"][i]["name"]=s.str(1); out["stalls"][i++]["area"]=s.str(2); }
            return json(200,std::move(out));
        });
    });
    CROW_ROUTE(app, "/api/access/request").methods("POST"_method)([](const crow::request &r) {
        return handle([&] {
            auto j=body(r); auto sid=integer(j,"stall_id",1,1000000000); auto name=text(j,"office_name",120);
            Db d(path); throttle(d,"access:"+r.remote_ip_address,30,3600);
            Stmt stallCheck(d,"SELECT s.id FROM stalls s JOIN owners o ON o.id=s.owner_id WHERE s.id=? AND o.verified=1");
            stallCheck.bind(1,sid); require(stallCheck.row(),404,"Stall unavailable");
            auto key=token(), id=token();
            Stmt s(d,"INSERT INTO office_devices(id,token_hash,stall_id,office_name,created) VALUES(?,?,?,?,?)");
            s.bind(1,id).bind(2,digest(key)).bind(3,sid).bind(4,name).bind(5,now()).row();
            Json out; out["device_token"]=key; out["status"]="PENDING";
            return json(201,std::move(out));
        });
    });
    CROW_ROUTE(app, "/api/access")([](const crow::request &r) {
        return handle([&] {
            auto key=r.get_header_value("X-Office-Token"); require(key.size()==64,401,"Office access required");
            Db d(path); Stmt s(d,"SELECT status FROM office_devices WHERE token_hash=?"); s.bind(1,digest(key));
            require(s.row(),401,"Office access not found"); Json out; out["status"]=s.str(0); return json(200,std::move(out));
        });
    });
    CROW_ROUTE(app, "/api/access/requests")([](const crow::request &r) {
        return handle([&] {
            Db d(path); auto sid=stall(d,owner(d,r));
            Stmt s(d,"SELECT id,office_name,status,office_id FROM office_devices WHERE stall_id=? ORDER BY created DESC,id DESC LIMIT 100"); s.bind(1,sid);
            Json out; out["requests"]=Json::list(); int i=0;
            while(s.row()){out["requests"][i]["id"]=s.str(0);out["requests"][i]["office_name"]=s.str(1);out["requests"][i]["status"]=s.str(2);out["requests"][i++]["office_id"]=s.num(3);}
            return json(200,std::move(out));
        });
    });
    CROW_ROUTE(app, "/api/access/requests/<string>").methods("POST"_method)([](const crow::request &r,std::string id) {
        return handle([&] {
            auto j=body(r); auto status=text(j,"status",20); require(status=="APPROVED"||status=="REVOKED",400,"Invalid access decision");
            Db d(path); auto uid=owner(d,r), sid=stall(d,uid); Transaction tx(d);
            Stmt existing(d,"SELECT status,office_id FROM office_devices WHERE id=? AND stall_id=?"); existing.bind(1,id).bind(2,sid);require(existing.row(),404,"Access request missing");
            long long oid=0;
            if(status=="APPROVED") {
                oid=integer(j,"office_id",1,1000000000);
                require(existing.str(0)=="PENDING"||(existing.str(0)=="APPROVED"&&existing.num(1)==oid),409,"Create a new request to change office access");
                Stmt officeCheck(d,"SELECT id FROM offices WHERE id=? AND stall_id=?");officeCheck.bind(1,oid).bind(2,sid);require(officeCheck.row(),404,"Office missing");
                Stmt approve(d,"UPDATE offices SET owner_approved=1,version=version+1 WHERE id=? AND owner_approved=0");approve.bind(1,oid).row();
            }
            Stmt update(d,"UPDATE office_devices SET status=?,office_id=CASE WHEN ?='APPROVED' THEN ? ELSE office_id END WHERE id=? AND stall_id=?");
            update.bind(1,status).bind(2,status).bind(3,oid).bind(4,id).bind(5,sid).row();
            audit(d,"OFFICE_DEVICE_"+status,id,std::to_string(uid),"office="+std::to_string(oid));tx.commit();
            Json out;out["status"]=status;return json(200,std::move(out));
        });
    });
}
}
