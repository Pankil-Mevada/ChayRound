#include "merchant.h"
#include "portal.h"
#include "communication.h"
#include "campaigns.h"
#include "workspaces.h"
#include "promotions.h"
#include "marketing_automation.h"
using namespace cr;
using namespace merchant;
int main() {
    serviceKey();
    curl_global_init(CURL_GLOBAL_DEFAULT);
    path = env("CHAY_DB", "merchant.db");
    migrate();
    crow::SimpleApp app;
    verification(app);
    portal(app);
    communication(app);
    campaigns(app);
    workspaces(app);
    promotions(app);
    marketingAutomation(app);
    CROW_ROUTE(app, "/health")([] {
        return handle([] {
            Db d(path);
            Stmt s(d, "SELECT COUNT(*) FROM owners");
            s.row();
            Json j;
            j["status"] = "ok";
            j["schema_version"] = 2;
            return json(200, std::move(j));
        });
    });
    CROW_ROUTE(app, "/api/register").methods("POST"_method)([](const crow::request &r) {
        return handle([&] {
            auto j = body(r);
            auto email = emailAddress(text(j, "email", 200)), pass = text(j, "password", 200),
                 name = text(j, "stall_name");
            require(pass.size() >= 12, 400, "Use a password of at least 12 characters");
            Db d(path);
            throttle(d, "register:" + r.remote_ip_address, 10, 3600);
            auto salt = token(), hash = password(pass, salt);
            Transaction tx(d);
            Stmt old(d, "SELECT id FROM owners WHERE email=?");
            old.bind(1, email);
            require(!old.row(), 409, "Email already registered");
            Stmt s(d, "INSERT INTO owners(email,salt,password) VALUES(?,?,?)");
            s.bind(1, email).bind(2, salt).bind(3, hash).row();
            auto uid = sqlite3_last_insert_rowid(d.p);
            Stmt member(d,"INSERT INTO workspace_members VALUES(?,?,'OWNER')");member.bind(1,uid).bind(2,uid).row();
            Stmt st(d, "INSERT INTO stalls(owner_id,name) VALUES(?,?)");
            st.bind(1, uid).bind(2, name).row();
            audit(d, "OWNER_CREATED", std::to_string(uid), std::to_string(uid),
                  "Email verification pending");
            tx.commit();
            Json out;
            out["message"] = "Registered. Sign in and verify your email.";
            return json(201, std::move(out));
        });
    });
    CROW_ROUTE(app, "/api/login").methods("POST"_method)([](const crow::request &r) {
        return handle([&] {
            auto j = body(r);
            auto email = emailAddress(text(j, "email", 200)), pass = text(j, "password", 200);
            Db d(path);
            throttle(d, "login:" + digest(email), 15, 900);
            Stmt q(d, "SELECT id,salt,password FROM owners WHERE email=?");
            q.bind(1, email);
            bool exists = q.row();
            auto hash = password(pass, exists ? q.str(1) : std::string(64, '0'));
            require(exists && equal(hash, q.str(2)), 401, "Invalid credentials");
            auto key = token();
            Stmt s(d, "INSERT INTO sessions(hash,owner_id,expires,principal_id) VALUES(?,?,?,?)");
            s.bind(1, digest(key)).bind(2, q.num(0)).bind(3, now() + 86400).bind(4,q.num(0)).row();
            Json out;
            out["token"] = key;
            return json(200, std::move(out));
        });
    });
    CROW_ROUTE(app, "/api/logout").methods("POST"_method)([](const crow::request &r) {
        return handle([&] {
            Db d(path);
            owner(d, r, false);
            Stmt s(d, "DELETE FROM sessions WHERE hash=?");
            s.bind(1, digest(r.get_header_value("Authorization").substr(7))).row();
            Json j;
            j["message"] = "Signed out";
            return json(200, std::move(j));
        });
    });
    CROW_ROUTE(app, "/api/owner")([](const crow::request &r) {
        return handle([&] {
            Db d(path);
            auto uid = owner(d, r, false), sid = stall(d, uid);
            Stmt s(d, "SELECT "
                      "s.name,s.area,s.paused,s.opens,s.closes,s.days,s.version,o.verified,o.email "
                      "FROM stalls s JOIN owners o ON o.id=s.owner_id WHERE s.id=?");
            s.bind(1, sid);
            s.row();
            Json out;
            out["stall_id"] = sid;
            out["stall_name"] = s.str(0);
            out["area"] = s.str(1);
            out["paused"] = bool(s.num(2));
            out["opens"] = s.num(3);
            out["closes"] = s.num(4);
            out["days"] = s.num(5);
            out["version"] = s.num(6);
            out["verified"] = bool(s.num(7));
            Stmt contact(d,"SELECT email FROM owners WHERE id=?");contact.bind(1,principal(d,r));contact.row();out["email"]=contact.str(0);
            out["menu"] = menu(d, sid);
            out["offices"] = Json::list();
            Stmt o(d, "SELECT id FROM offices WHERE stall_id=? ORDER BY id");
            o.bind(1, sid);
            int i = 0;
            while (o.row())
                out["offices"][i++] = officeInfo(d, o.num(0));
            return json(200, std::move(out));
        });
    });
    CROW_ROUTE(app, "/api/stall").methods("POST"_method)([](const crow::request &r) {
        return handle([&] {
            auto j = body(r);
            auto name = text(j, "name"), area = text(j, "area");
            auto opens = integer(j, "opens", 0, 1439), closes = integer(j, "closes", 0, 1439),
                 days = integer(j, "days", 1, 127), version = integer(j, "version", 1, 1000000);
            auto paused = flag(j, "paused");
            Db d(path);
            auto uid = owner(d, r), sid = stall(d, uid);
            Transaction tx(d);
            Stmt s(d, "UPDATE stalls SET "
                      "name=?,area=?,paused=?,opens=?,closes=?,days=?,version=version+1 WHERE id=? "
                      "AND version=?");
            s.bind(1, name)
                .bind(2, area)
                .bind(3, paused ? 1 : 0)
                .bind(4, opens)
                .bind(5, closes)
                .bind(6, days)
                .bind(7, sid)
                .bind(8, version)
                .row();
            require(sqlite3_changes(d.p) == 1, 409, "Settings changed; refresh");
            audit(d, "STALL_UPDATED", std::to_string(sid), std::to_string(uid), Json(j).dump());
            tx.commit();
            Json out;
            out["version"] = version + 1;
            return json(200, std::move(out));
        });
    });
    CROW_ROUTE(app, "/api/menu").methods("POST"_method)([](const crow::request &r) {
        return handle([&] {
            auto j = body(r);
            auto name = text(j, "name");
            auto price = integer(j, "price_paise", 1, 1000000);
            Db d(path);
            auto uid = owner(d, r), sid = stall(d, uid);
            Transaction tx(d);
            Stmt s(d, "INSERT INTO menu(stall_id,name,price) VALUES(?,?,?)");
            s.bind(1, sid).bind(2, name).bind(3, price).row();
            auto id = sqlite3_last_insert_rowid(d.p);
            audit(d, "MENU_CREATED", std::to_string(id), std::to_string(uid), name);
            tx.commit();
            Json out;
            out["id"] = id;
            return json(201, std::move(out));
        });
    });
    CROW_ROUTE(app, "/api/menu/<int>").methods("POST"_method)([](const crow::request &r, int id) {
        return handle([&] {
            auto j = body(r);
            auto name = text(j, "name");
            auto price = integer(j, "price_paise", 1, 1000000),
                 version = integer(j, "version", 1, 1000000);
            bool available = flag(j, "available");
            Db d(path);
            auto uid = owner(d, r), sid = stall(d, uid);
            Transaction tx(d);
            Stmt s(d, "UPDATE menu SET name=?,price=?,available=?,version=version+1 WHERE id=? AND "
                      "stall_id=? AND version=?");
            s.bind(1, name)
                .bind(2, price)
                .bind(3, available ? 1 : 0)
                .bind(4, id)
                .bind(5, sid)
                .bind(6, version)
                .row();
            require(sqlite3_changes(d.p) == 1, 409, "Item missing or changed; refresh");
            audit(d, "MENU_UPDATED", std::to_string(id), std::to_string(uid), Json(j).dump());
            tx.commit();
            Json out;
            out["version"] = version + 1;
            return json(200, std::move(out));
        });
    });
    CROW_ROUTE(app, "/api/offices").methods("POST"_method)([](const crow::request &r) {
        return handle([&] {
            auto j = body(r);
            auto name = text(j, "name"), email = optionalText(j, "email", 200),
                 address = text(j, "address", 400), area = text(j, "area"),
                 instructions = optionalText(j, "instructions"),
                 policy = j.has("policy") ? text(j, "policy") : "PREPAID";
            if (!email.empty()) email = emailAddress(email);
            require(policy == "PREPAID" || policy == "MONTHLY", 400, "Invalid policy");
            auto limit =
                j.has("credit_limit_paise") ? integer(j, "credit_limit_paise", 0, 100000000) : 0;
            auto key = token(), salt = token(), hash = pinHash(optionalText(j, "pin", 8), salt);
            Db d(path);
            auto uid = owner(d, r), sid = stall(d, uid);
            Transaction tx(d);
            Stmt s(d, "INSERT INTO "
                      "offices(stall_id,name,token_hash,policy,credit_limit,email,address,area,"
                      "instructions,pin_hash,pin_salt) VALUES(?,?,?,?,?,?,?,?,?,?,?)");
            s.bind(1, sid)
                .bind(2, name)
                .bind(3, digest(key))
                .bind(4, policy)
                .bind(5, limit)
                .bind(6, email)
                .bind(7, address)
                .bind(8, area)
                .bind(9, instructions)
                .bind(10, hash)
                .bind(11, salt)
                .row();
            auto id = sqlite3_last_insert_rowid(d.p);
            audit(d, "OFFICE_CREATED", std::to_string(id), std::to_string(uid),
                  "Contact verification pending; " + policy);
            tx.commit();
            Json out;
            out["id"] = id;
            out["office_token"] = key;
            return json(201, std::move(out));
        });
    });
    CROW_ROUTE(app, "/api/offices/<int>")
        .methods("POST"_method)([](const crow::request &r, int id) {
            return handle([&] {
                auto j = body(r);
                auto name = text(j, "name"), email = optionalText(j, "email", 200),
                     address = text(j, "address", 400), area = text(j, "area"),
                     instructions = optionalText(j, "instructions"), policy = text(j, "policy"),
                     reason = text(j, "reason", 300);
                if (!email.empty()) email = emailAddress(email);
                require(policy == "PREPAID" || policy == "MONTHLY", 400, "Invalid policy");
                auto limit = integer(j, "credit_limit_paise", 0, 100000000),
                     version = integer(j, "version", 1, 1000000);
                bool enabled = flag(j, "service_enabled"), credit = flag(j, "credit_enabled");
                Db d(path);
                auto uid = owner(d, r), sid = stall(d, uid);
                Transaction tx(d);
                Stmt old(d,
                         "SELECT email,pin_hash,pin_salt FROM offices WHERE id=? AND stall_id=?");
                old.bind(1, id).bind(2, sid);
                require(old.row(), 404, "Office missing");
                auto salt = old.str(2), hash = old.str(1);
                if (j.has("pin")) {
                    salt = token();
                    hash = pinHash(optionalText(j, "pin", 8), salt);
                }
                Stmt s(d, "UPDATE offices SET "
                          "name=?,email=?,address=?,area=?,instructions=?,policy=?,credit_limit=?,"
                          "service_enabled=?,credit_enabled=?,verified=CASE WHEN email=? THEN "
                          "verified ELSE 0 END,pin_hash=?,pin_salt=?,version=version+1 WHERE id=? "
                          "AND stall_id=? AND version=?");
                s.bind(1, name)
                    .bind(2, email)
                    .bind(3, address)
                    .bind(4, area)
                    .bind(5, instructions)
                    .bind(6, policy)
                    .bind(7, limit)
                    .bind(8, enabled ? 1 : 0)
                    .bind(9, credit ? 1 : 0)
                    .bind(10, email)
                    .bind(11, hash)
                    .bind(12, salt)
                    .bind(13, id)
                    .bind(14, sid)
                    .bind(15, version)
                    .row();
                require(sqlite3_changes(d.p) == 1, 409, "Office changed; refresh");
                audit(d, "OFFICE_POLICY_UPDATED", std::to_string(id), std::to_string(uid),
                      "v" + std::to_string(version) + " -> " + policy +
                          "; limit=" + std::to_string(limit) + "; " + reason);
                tx.commit();
                Json out;
                out["version"] = version + 1;
                return json(200, std::move(out));
            });
        });
    CROW_ROUTE(app, "/api/offices/<int>/rotate")
        .methods("POST"_method)([](const crow::request &r, int id) {
            return handle([&] {
                Db d(path);
                auto uid = owner(d, r), sid = stall(d, uid);
                auto key = token();
                Transaction tx(d);
                Stmt s(
                    d,
                    "UPDATE offices SET token_hash=?,version=version+1 WHERE id=? AND stall_id=?");
                s.bind(1, digest(key)).bind(2, id).bind(3, sid).row();
                require(sqlite3_changes(d.p) == 1, 404, "Office missing");
                audit(d, "OFFICE_LINK_ROTATED", std::to_string(id), std::to_string(uid),
                      "Previous link revoked");
                tx.commit();
                Json out;
                out["office_token"] = key;
                return json(200, std::move(out));
            });
        });
    CROW_ROUTE(app, "/internal/owner")([](const crow::request &r) {
        return handle([&] {
            internal(r);
            Db d(path);
            auto uid = owner(d, r);
            Json out;
            out["owner_id"] = principal(d,r);
            out["stall_id"] = stall(d, uid);
            return json(200, std::move(out));
        });
    });
    CROW_ROUTE(app, "/internal/office").methods("POST"_method)([](const crow::request &r) {
        return handle([&] {
            internal(r);
            Db d(path);
            return json(200, officeInfo(d, officeId(d, r),true));
        });
    });
    CROW_ROUTE(app, "/internal/office/<int>")([](const crow::request &r, int id) {
        return handle([&] {
            internal(r);
            Db d(path);
            auto sid = stall(d, owner(d, r));
            Stmt s(d, "SELECT id FROM offices WHERE id=? AND stall_id=?");
            s.bind(1, id).bind(2, sid);
            require(s.row(), 404, "Office missing");
            return json(200, officeInfo(d, id,true));
        });
    });
    app.loglevel(crow::LogLevel::Warning)
        .bindaddr("127.0.0.1")
        .port(port(9181))
        .concurrency(4)
        .run();
}
