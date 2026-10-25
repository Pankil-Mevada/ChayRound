#pragma once
namespace orders {
inline void runMarketing(){
    auto base=env("CHAY_MERCHANT_URL","http://127.0.0.1:9181");auto candidates=http(base+"/internal/marketing-candidates","GET","",{"X-Internal-Key: "+serviceKey()});require(candidates.status==200,503,"Marketing candidate service unavailable");auto j=crow::json::load(candidates.body);require(bool(j)&&j.has("candidates"),503,"Invalid marketing candidates");
    for(auto &candidate:j["candidates"]){auto oid=candidate["office_id"].i(),sid=candidate["stall_id"].i();long long last=0;{Db d(path);Stmt s(d,"SELECT COALESCE(MAX(fulfilled_at),0) FROM orders WHERE office_id=? AND stall_id=? AND status='FULFILLED'");s.bind(1,oid).bind(2,sid);s.row();last=s.num(0);}Json message;message["office_id"]=oid;message["stall_id"]=sid;message["kind"]="WELCOME";try{http(base+"/internal/automatic-marketing","POST",message.dump(),{"X-Internal-Key: "+serviceKey()});if(last>0&&last+candidate["inactive_days"].i()*86400<now()){message["kind"]="REACTIVATION";http(base+"/internal/automatic-marketing","POST",message.dump(),{"X-Internal-Key: "+serviceKey()});}}catch(const std::exception&){} }
}
}
