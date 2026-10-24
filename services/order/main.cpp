#include "core.h"
using namespace cr;
static std::string path;
static crow::json::rvalue office(const crow::request &r) {
    auto key = r.get_header_value("X-Office-Token");
    require(key.size() == 64, 401, "Office link required");
    Json j;
    j["office_token"] = key;
    return remote("/internal/office", "POST", j.dump());
}
static long long owner(const crow::request &r) {
    return remote("/internal/owner", "GET", "", r.get_header_value("Authorization"))["stall_id"]
        .i();
}
static Json record(Stmt &s) {
    Json j;
    j["id"] = s.str(0);
    j["office_id"] = s.num(1);
    j["office_name"] = s.str(2);
    j["item_name"] = s.str(3);
    j["quantity"] = s.num(4);
    j["total_paise"] = s.num(5);
    j["status"] = s.str(6);
    j["settlement"] = s.str(7);
    j["version"] = s.num(8);
    j["policy"] = s.str(9);
    return j;
}
int main() {
    serviceKey();
    curl_global_init(CURL_GLOBAL_DEFAULT);
    path = env("CHAY_DB", "orders.db");
    {
        Db d(path);
        d.exec(R"SQL(
PRAGMA journal_mode=WAL;
CREATE TABLE IF NOT EXISTS orders(id TEXT PRIMARY KEY,stall_id INTEGER NOT NULL,office_id INTEGER NOT NULL,office_name TEXT NOT NULL,item_id INTEGER NOT NULL,item_name TEXT NOT NULL,quantity INTEGER NOT NULL,total INTEGER NOT NULL,status TEXT NOT NULL,settlement TEXT NOT NULL DEFAULT 'UNPAID',version INTEGER NOT NULL DEFAULT 1,policy TEXT NOT NULL,idem TEXT NOT NULL,request_hash TEXT NOT NULL,created INTEGER NOT NULL,UNIQUE(office_id,idem));
CREATE INDEX IF NOT EXISTS orders_office ON orders(office_id,status,settlement);
CREATE INDEX IF NOT EXISTS orders_stall ON orders(stall_id,created);
CREATE TABLE IF NOT EXISTS events(id INTEGER PRIMARY KEY,order_id TEXT NOT NULL REFERENCES orders(id),kind TEXT NOT NULL,created INTEGER NOT NULL);
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
    CROW_ROUTE(app, "/api/office")([](const crow::request &r) {
        return handle([&] {
            auto o = office(r);
            Json out(o);
            Db d(path);
            Stmt s(d, "SELECT COALESCE(SUM(total),0) FROM orders WHERE office_id=? "
                      "AND status!='CANCELLED' AND settlement='UNPAID'");
            s.bind(1, o["office_id"].i());
            s.row();
            out["committed_paise"] = s.num(0);
            return json(200, std::move(out));
        });
    });
    CROW_ROUTE(app, "/api/orders").methods("POST"_method)([](const crow::request &r) {
        return handle([&] {
            auto j = body(r);
            auto item = integer(j, "item_id", 1, 1000000000), qty = integer(j, "quantity", 1, 500);
            auto idem = r.get_header_value("Idempotency-Key");
            require(idem.size() >= 8 && idem.size() <= 100, 400,
                    "Idempotency-Key required (8-100 characters)");
            auto o = office(r);
            auto oid = o["office_id"].i();
            auto requestHash = digest(std::to_string(item) + ":" + std::to_string(qty));
            Db d(path);
            Transaction tx(d);
            Stmt prior(d, "SELECT id,request_hash FROM orders WHERE office_id=? "
                          "AND idem=?");
            prior.bind(1, oid).bind(2, idem);
            if (prior.row()) {
                require(prior.str(1) == requestHash, 409, "Key already used for a different order");
                Json out;
                out["id"] = prior.str(0);
                out["replayed"] = true;
                tx.commit();
                return json(200, std::move(out));
            }
            require(std::string(o["policy"].s()) == "MONTHLY", 409,
                    "Prepaid checkout is not implemented. No order or payment "
                    "created.");
            long long price = 0;
            std::string name;
            for (auto &m : o["menu"]) {
                if (m["id"].i() == item) {
                    price = m["price_paise"].i();
                    name = m["name"].s();
                }
            }
            require(price > 0, 400, "Item does not belong to this stall");
            auto amount = total(price, qty);
            Stmt debt(d, "SELECT COALESCE(SUM(total),0) FROM orders WHERE office_id=? "
                         "AND status!='CANCELLED' AND settlement='UNPAID'");
            debt.bind(1, oid);
            debt.row();
            require(debt.num(0) + amount <= o["credit_limit_paise"].i(), 409,
                    "Office credit limit exceeded");
            auto id = token();
            Stmt insert(d, "INSERT INTO "
                           "orders(id,stall_id,office_id,office_name,item_id,item_name,"
                           "quantity,total,status,policy,idem,request_hash,created) "
                           "VALUES(?,?,?,?,?,?,?,?,'PLACED','MONTHLY',?,?,?)");
            insert.bind(1, id)
                .bind(2, o["stall_id"].i())
                .bind(3, oid)
                .bind(4, std::string(o["office_name"].s()))
                .bind(5, item)
                .bind(6, name)
                .bind(7, qty)
                .bind(8, amount)
                .bind(9, idem)
                .bind(10, requestHash)
                .bind(11, now())
                .row();
            Stmt event(d, "INSERT INTO events(order_id,kind,created) VALUES(?,'PLACED',?)");
            event.bind(1, id).bind(2, now()).row();
            tx.commit();
            Json out;
            out["id"] = id;
            out["total_paise"] = amount;
            out["status"] = "PLACED";
            return json(201, std::move(out));
        });
    });
    CROW_ROUTE(app, "/api/orders")([](const crow::request &r) {
        return handle([&] {
            bool isOffice = !r.get_header_value("X-Office-Token").empty();
            long long scope = isOffice ? office(r)["office_id"].i() : owner(r);
            Db d(path);
            std::string sql = "SELECT "
                              "id,office_id,office_name,item_name,quantity,total,"
                              "status,settlement,version,policy FROM orders WHERE ";
            sql += isOffice ? "office_id=?" : "stall_id=?";
            sql += " ORDER BY created DESC,id DESC LIMIT 200";
            Stmt s(d, sql.c_str());
            s.bind(1, scope);
            Json out = Json::list();
            int i = 0;
            while (s.row())
                out[i++] = record(s);
            return json(200, std::move(out));
        });
    });
    CROW_ROUTE(app, "/api/orders/<string>/status")
        .methods("POST"_method)([](const crow::request &r, std::string id) {
            return handle([&] {
                auto j = body(r);
                auto target = text(j, "status");
                auto version = integer(j, "version", 1, 1000000000);
                auto sid = owner(r);
                Db d(path);
                Transaction tx(d);
                Stmt q(d, "SELECT status,version FROM orders WHERE id=? AND stall_id=?");
                q.bind(1, id).bind(2, sid);
                require(q.row(), 404, "Order missing");
                require(q.num(1) == version, 409, "Order changed; refresh");
                require(transition(q.str(0), target), 409, "Invalid state transition");
                Stmt u(d, "UPDATE orders SET status=?,version=version+1 WHERE id=? "
                          "AND version=?");
                u.bind(1, target).bind(2, id).bind(3, version).row();
                require(sqlite3_changes(d.p) == 1, 409, "Order changed");
                Stmt e(d, "INSERT INTO events(order_id,kind,created) VALUES(?,?,?)");
                e.bind(1, id).bind(2, target).bind(3, now()).row();
                tx.commit();
                Json out;
                out["status"] = target;
                return json(200, std::move(out));
            });
        });
    app.bindaddr("127.0.0.1").port(port(9182)).concurrency(4).run();
}
