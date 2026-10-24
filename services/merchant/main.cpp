#include "core.h"
using namespace cr;
static std::string path;
static long long owner(Db &d, const crow::request &r) {
    auto auth = r.get_header_value("Authorization");
    require(auth.rfind("Bearer ", 0) == 0, 401, "Sign in required");
    Stmt s(d, "SELECT owner_id FROM sessions WHERE hash=? AND expires>?");
    s.bind(1, digest(auth.substr(7))).bind(2, now());
    require(s.row(), 401, "Session expired");
    return s.num(0);
}
static long long stall(Db &d, long long uid) {
    Stmt s(d, "SELECT id FROM stalls WHERE owner_id=?");
    s.bind(1, uid);
    require(s.row(), 404, "Stall missing");
    return s.num(0);
}
int main() {
    serviceKey();
    path = env("CHAY_DB", "merchant.db");
    {
        Db d(path);
        d.exec(R"SQL(
PRAGMA journal_mode=WAL;
CREATE TABLE IF NOT EXISTS owners(id INTEGER PRIMARY KEY,email TEXT UNIQUE NOT NULL,salt TEXT NOT NULL,password TEXT NOT NULL);
CREATE TABLE IF NOT EXISTS sessions(hash TEXT PRIMARY KEY,owner_id INTEGER NOT NULL REFERENCES owners(id),expires INTEGER NOT NULL);
CREATE TABLE IF NOT EXISTS stalls(id INTEGER PRIMARY KEY,owner_id INTEGER UNIQUE NOT NULL REFERENCES owners(id),name TEXT NOT NULL);
CREATE TABLE IF NOT EXISTS menu(id INTEGER PRIMARY KEY,stall_id INTEGER NOT NULL REFERENCES stalls(id),name TEXT NOT NULL,price INTEGER NOT NULL CHECK(price>0));
CREATE TABLE IF NOT EXISTS offices(id INTEGER PRIMARY KEY,stall_id INTEGER NOT NULL REFERENCES stalls(id),name TEXT NOT NULL,token_hash TEXT UNIQUE NOT NULL,policy TEXT NOT NULL CHECK(policy IN ('PREPAID','MONTHLY')),credit_limit INTEGER NOT NULL CHECK(credit_limit>=0));
)SQL");
    }
    crow::SimpleApp app;
    CROW_ROUTE(app, "/health")([] {
        return handle([] {
            Db d(path);
            Stmt s(d, "SELECT 1");
            s.row();
            Json j;
            j["status"] = "ok";
            return json(200, std::move(j));
        });
    });
    CROW_ROUTE(app, "/api/register").methods("POST"_method)([](const crow::request &r) {
        return handle([&] {
            auto j = body(r);
            auto email = text(j, "email", 200), pass = text(j, "password", 200),
                 name = text(j, "stall_name");
            require(pass.size() >= 12, 400, "Use a password of at least 12 characters");
            require(email.find('@') != std::string::npos, 400, "Invalid email");
            auto salt = token(), hash = password(pass, salt);
            Db d(path);
            Transaction tx(d);
            Stmt existing(d, "SELECT id FROM owners WHERE email=?");
            existing.bind(1, email);
            require(!existing.row(), 409, "Email already registered");
            Stmt q(d, "INSERT INTO owners(email,salt,password) VALUES(?,?,?)");
            q.bind(1, email).bind(2, salt).bind(3, hash).row();
            auto uid = sqlite3_last_insert_rowid(d.p);
            Stmt s(d, "INSERT INTO stalls(owner_id,name) VALUES(?,?)");
            s.bind(1, uid).bind(2, name).row();
            tx.commit();
            Json out;
            out["message"] = "Registered. Sign in to continue.";
            return json(201, std::move(out));
        });
    });
    CROW_ROUTE(app, "/api/login").methods("POST"_method)([](const crow::request &r) {
        return handle([&] {
            auto j = body(r);
            auto email = text(j, "email", 200), pass = text(j, "password", 200);
            Db d(path);
            Stmt q(d, "SELECT id,salt,password FROM owners WHERE email=?");
            q.bind(1, email);
            bool exists = q.row();
            auto hash = password(pass, exists ? q.str(1) : std::string(64, '0'));
            require(exists && equal(hash, q.str(2)), 401, "Invalid credentials");
            auto key = token();
            Stmt s(d, "INSERT INTO sessions VALUES(?,?,?)");
            s.bind(1, digest(key)).bind(2, q.num(0)).bind(3, now() + 86400).row();
            Json out;
            out["token"] = key;
            return json(200, std::move(out));
        });
    });
    CROW_ROUTE(app, "/api/logout").methods("POST"_method)([](const crow::request &r) {
        return handle([&] {
            Db d(path);
            owner(d, r);
            Stmt s(d, "DELETE FROM sessions WHERE hash=?");
            s.bind(1, digest(r.get_header_value("Authorization").substr(7))).row();
            Json out;
            out["message"] = "Signed out";
            return json(200, std::move(out));
        });
    });
    CROW_ROUTE(app, "/api/owner")([](const crow::request &r) {
        return handle([&] {
            Db d(path);
            auto sid = stall(d, owner(d, r));
            Json out;
            out["stall_id"] = sid;
            Stmt s(d, "SELECT name FROM stalls WHERE id=?");
            s.bind(1, sid);
            s.row();
            out["stall_name"] = s.str(0);
            out["menu"] = Json::list();
            Stmt m(d, "SELECT id,name,price FROM menu WHERE stall_id=? ORDER BY id");
            m.bind(1, sid);
            int i = 0;
            while (m.row()) {
                out["menu"][i]["id"] = m.num(0);
                out["menu"][i]["name"] = m.str(1);
                out["menu"][i++]["price_paise"] = m.num(2);
            }
            out["offices"] = Json::list();
            Stmt o(d, "SELECT id,name,policy,credit_limit FROM offices WHERE "
                      "stall_id=? ORDER BY id");
            o.bind(1, sid);
            i = 0;
            while (o.row()) {
                out["offices"][i]["id"] = o.num(0);
                out["offices"][i]["name"] = o.str(1);
                out["offices"][i]["policy"] = o.str(2);
                out["offices"][i++]["credit_limit_paise"] = o.num(3);
            }
            return json(200, std::move(out));
        });
    });
    CROW_ROUTE(app, "/api/menu").methods("POST"_method)([](const crow::request &r) {
        return handle([&] {
            auto j = body(r);
            auto name = text(j, "name");
            auto price = integer(j, "price_paise", 1, 1000000);
            Db d(path);
            auto sid = stall(d, owner(d, r));
            Stmt s(d, "INSERT INTO menu(stall_id,name,price) VALUES(?,?,?)");
            s.bind(1, sid).bind(2, name).bind(3, price).row();
            Json out;
            out["id"] = sqlite3_last_insert_rowid(d.p);
            return json(201, std::move(out));
        });
    });
    CROW_ROUTE(app, "/api/offices").methods("POST"_method)([](const crow::request &r) {
        return handle([&] {
            auto j = body(r);
            auto name = text(j, "name"), policy = text(j, "policy");
            require(policy == "PREPAID" || policy == "MONTHLY", 400, "Invalid policy");
            auto limit = integer(j, "credit_limit_paise", 0, 100000000);
            auto key = token();
            Db d(path);
            auto sid = stall(d, owner(d, r));
            Stmt s(d, "INSERT INTO "
                      "offices(stall_id,name,token_hash,policy,credit_limit) "
                      "VALUES(?,?,?,?,?)");
            s.bind(1, sid).bind(2, name).bind(3, digest(key)).bind(4, policy).bind(5, limit).row();
            Json out;
            out["id"] = sqlite3_last_insert_rowid(d.p);
            out["office_token"] = key;
            return json(201, std::move(out));
        });
    });
    CROW_ROUTE(app, "/api/offices/<int>/rotate")
        .methods("POST"_method)([](const crow::request &r, int id) {
            return handle([&] {
                Db d(path);
                auto sid = stall(d, owner(d, r));
                auto key = token();
                Stmt s(d, "UPDATE offices SET token_hash=? WHERE id=? AND stall_id=?");
                s.bind(1, digest(key)).bind(2, id).bind(3, sid).row();
                require(sqlite3_changes(d.p) == 1, 404, "Office missing");
                Json out;
                out["office_token"] = key;
                return json(200, std::move(out));
            });
        });
    CROW_ROUTE(app, "/internal/owner")([](const crow::request &r) {
        return handle([&] {
            internal(r);
            Db d(path);
            Json out;
            out["stall_id"] = stall(d, owner(d, r));
            return json(200, std::move(out));
        });
    });
    CROW_ROUTE(app, "/internal/office").methods("POST"_method)([](const crow::request &r) {
        return handle([&] {
            internal(r);
            auto j = body(r);
            auto key = text(j, "office_token", 64);
            Db d(path);
            Stmt o(d, "SELECT o.id,o.stall_id,o.name,o.policy,o.credit_limit,s.name "
                      "FROM offices o JOIN stalls s ON s.id=o.stall_id WHERE "
                      "token_hash=?");
            o.bind(1, digest(key));
            require(o.row(), 401, "Office link invalid or revoked");
            Json out;
            out["office_id"] = o.num(0);
            out["stall_id"] = o.num(1);
            out["office_name"] = o.str(2);
            out["policy"] = o.str(3);
            out["credit_limit_paise"] = o.num(4);
            out["stall_name"] = o.str(5);
            out["menu"] = Json::list();
            Stmt m(d, "SELECT id,name,price FROM menu WHERE stall_id=? ORDER BY id");
            m.bind(1, o.num(1));
            int i = 0;
            while (m.row()) {
                out["menu"][i]["id"] = m.num(0);
                out["menu"][i]["name"] = m.str(1);
                out["menu"][i++]["price_paise"] = m.num(2);
            }
            return json(200, std::move(out));
        });
    });
    app.bindaddr("127.0.0.1").port(port(9181)).concurrency(4).run();
}
