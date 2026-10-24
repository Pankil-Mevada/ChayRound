#pragma once
#include "core.h"
namespace orders {
using namespace cr;
inline std::string path;
inline void migrate() {
    Db d(path);
    bool upgrade = false;
    {
        Stmt v(d, "PRAGMA user_version");
        v.row();
        upgrade = v.num(0) < 2;
    }
    Transaction tx(d);
    d.exec(R"SQL(
CREATE TABLE IF NOT EXISTS orders(id TEXT PRIMARY KEY,stall_id INTEGER NOT NULL,office_id INTEGER NOT NULL,office_name TEXT NOT NULL,item_id INTEGER NOT NULL,item_name TEXT NOT NULL,quantity INTEGER NOT NULL,total INTEGER NOT NULL,status TEXT NOT NULL,settlement TEXT NOT NULL DEFAULT 'UNPAID',version INTEGER NOT NULL DEFAULT 1,policy TEXT NOT NULL,idem TEXT NOT NULL,request_hash TEXT NOT NULL,created INTEGER NOT NULL,UNIQUE(office_id,idem));
CREATE INDEX IF NOT EXISTS orders_office ON orders(office_id,status,settlement);
CREATE INDEX IF NOT EXISTS orders_stall ON orders(stall_id,created);
CREATE TABLE IF NOT EXISTS events(id INTEGER PRIMARY KEY,order_id TEXT NOT NULL REFERENCES orders(id),kind TEXT NOT NULL,created INTEGER NOT NULL);
CREATE TABLE IF NOT EXISTS order_items(order_id TEXT NOT NULL REFERENCES orders(id),item_id INTEGER NOT NULL,name TEXT NOT NULL,price INTEGER NOT NULL,quantity INTEGER NOT NULL,PRIMARY KEY(order_id,item_id));
CREATE TABLE IF NOT EXISTS ledger(id TEXT PRIMARY KEY,stall_id INTEGER NOT NULL,office_id INTEGER NOT NULL,order_id TEXT,kind TEXT NOT NULL,amount INTEGER NOT NULL,reference TEXT NOT NULL,actor TEXT NOT NULL,created INTEGER NOT NULL);
CREATE INDEX IF NOT EXISTS ledger_office ON ledger(office_id,created);
CREATE TABLE IF NOT EXISTS allocations(id TEXT PRIMARY KEY,credit_id TEXT NOT NULL REFERENCES ledger(id),debit_id TEXT NOT NULL REFERENCES ledger(id),amount INTEGER NOT NULL,reversal_of TEXT UNIQUE REFERENCES allocations(id));
CREATE INDEX IF NOT EXISTS allocations_debit ON allocations(debit_id);
CREATE TABLE IF NOT EXISTS receipts(id TEXT PRIMARY KEY,stall_id INTEGER NOT NULL,office_id INTEGER NOT NULL,method TEXT NOT NULL,reference TEXT NOT NULL,amount INTEGER NOT NULL,idem TEXT NOT NULL,hash TEXT NOT NULL,UNIQUE(stall_id,idem),UNIQUE(stall_id,method,reference));
CREATE TABLE IF NOT EXISTS reversals(receipt_id TEXT PRIMARY KEY REFERENCES receipts(id),entry_id TEXT NOT NULL REFERENCES ledger(id));
CREATE TABLE IF NOT EXISTS invoices(id TEXT PRIMARY KEY,stall_id INTEGER NOT NULL,office_id INTEGER NOT NULL,period TEXT NOT NULL,due_at INTEGER NOT NULL,created INTEGER NOT NULL,UNIQUE(office_id,period));
CREATE TABLE IF NOT EXISTS invoice_lines(invoice_id TEXT NOT NULL REFERENCES invoices(id),debit_id TEXT UNIQUE NOT NULL REFERENCES ledger(id),amount INTEGER NOT NULL,PRIMARY KEY(invoice_id,debit_id));
CREATE TABLE IF NOT EXISTS webhook_events(stall_id INTEGER NOT NULL,event_id TEXT NOT NULL,created INTEGER NOT NULL,PRIMARY KEY(stall_id,event_id));
CREATE TABLE IF NOT EXISTS command_keys(stall_id INTEGER NOT NULL,idem TEXT NOT NULL,hash TEXT NOT NULL,result TEXT NOT NULL,PRIMARY KEY(stall_id,idem));
)SQL");
    column(d, "orders", "fulfilled_at", "INTEGER NOT NULL DEFAULT 0");
    column(d, "orders", "provider_order", "TEXT NOT NULL DEFAULT ''");
    column(d, "orders", "provider_payment", "TEXT NOT NULL DEFAULT ''");
    column(d, "orders", "setup_state", "TEXT NOT NULL DEFAULT 'NONE'");
    column(d, "orders", "refund_id", "TEXT NOT NULL DEFAULT ''");
    column(d, "orders", "refund_key", "TEXT NOT NULL DEFAULT ''");
    column(d, "orders", "address", "TEXT NOT NULL DEFAULT ''");
    column(d, "orders", "instructions", "TEXT NOT NULL DEFAULT ''");
    column(d, "orders", "policy_version", "INTEGER NOT NULL DEFAULT 1");
    column(d, "orders", "next_reconcile", "INTEGER NOT NULL DEFAULT 0");
    column(d, "orders", "reconcile_attempts", "INTEGER NOT NULL DEFAULT 0");
    column(d, "orders", "last_reconcile_error", "TEXT NOT NULL DEFAULT ''");
    column(d, "orders", "reconcile_lease", "TEXT NOT NULL DEFAULT ''");
    d.exec("CREATE UNIQUE INDEX IF NOT EXISTS provider_order_unique ON orders(provider_order) "
           "WHERE provider_order!='';"
           "CREATE UNIQUE INDEX IF NOT EXISTS provider_payment_unique ON orders(provider_payment) "
           "WHERE provider_payment!='';"
           "INSERT OR IGNORE INTO order_items SELECT id,item_id,item_name,total/quantity,quantity "
           "FROM orders;");
    if (upgrade)
        d.exec("INSERT OR IGNORE INTO ledger SELECT "
               "'legacy_'||id,stall_id,office_id,id,'CHARGE',total,'Imported fulfilled "
               "order','migration',created FROM orders WHERE status='FULFILLED' AND "
               "policy='MONTHLY';");
    for (auto table : {"ledger", "allocations", "invoice_lines", "invoices"}) {
        d.exec("CREATE TRIGGER IF NOT EXISTS " + std::string(table) +
               "_no_update BEFORE UPDATE ON " + table +
               " BEGIN SELECT RAISE(ABORT,'immutable financial record'); END;");
        d.exec("CREATE TRIGGER IF NOT EXISTS " + std::string(table) +
               "_no_delete BEFORE DELETE ON " + table +
               " BEGIN SELECT RAISE(ABORT,'immutable financial record'); END;");
    }
    auditSchema(d);
    d.exec("PRAGMA user_version=2");
    tx.commit();
    d.exec("PRAGMA journal_mode=WAL");
}
inline crow::json::rvalue merchant(const std::string &url, const crow::request &r,
                                   const std::string &method = "GET") {
    auto reply = http(env("CHAY_MERCHANT_URL", "http://127.0.0.1:9181") + url, method,
                      method == "GET" ? "" : "{}",
                      {"X-Internal-Key: " + serviceKey(),
                       "Authorization: " + r.get_header_value("Authorization"),
                       "X-Office-Token: " + r.get_header_value("X-Office-Token"),
                       "X-Office-Pin: " + r.get_header_value("X-Office-Pin")});
    require(reply.status == 200, reply.status >= 500 ? 503 : static_cast<int>(reply.status),
            reply.status >= 500 ? "Merchant service unavailable"
                                : "Access denied or contact not verified");
    auto j = crow::json::load(reply.body);
    require(bool(j), 503, "Invalid merchant response");
    return j;
}
inline crow::json::rvalue office(const crow::request &r) {
    return merchant("/internal/office", r, "POST");
}
inline crow::json::rvalue owner(const crow::request &r) { return merchant("/internal/owner", r); }
inline crow::json::rvalue ownedOffice(const crow::request &r, long long oid) {
    return merchant("/internal/office/" + std::to_string(oid), r);
}
inline void event(Db &d, const std::string &id, const std::string &kind) {
    Stmt s(d, "INSERT INTO events(order_id,kind,created) VALUES(?,?,?)");
    s.bind(1, id).bind(2, kind).bind(3, now()).row();
}
inline void entry(Db &d, const std::string &id, long long sid, long long oid,
                  const std::string &order, const std::string &kind, long long amount,
                  const std::string &reference, const std::string &actor) {
    Stmt s(d, "INSERT INTO ledger VALUES(?,?,?,?,?,?,?,?,?)");
    s.bind(1, id)
        .bind(2, sid)
        .bind(3, oid)
        .bind(4, order)
        .bind(5, kind)
        .bind(6, amount)
        .bind(7, reference)
        .bind(8, actor)
        .bind(9, now())
        .row();
}
inline long long outstanding(Db &d, long long oid) {
    Stmt s(d, "SELECT COALESCE(SUM(l.amount-COALESCE((SELECT SUM(a.amount) FROM allocations a "
              "WHERE a.debit_id=l.id),0)),0) FROM ledger l WHERE l.office_id=? AND l.kind IN "
              "('CHARGE','DEBIT_ADJUSTMENT') AND l.order_id IN (SELECT id FROM orders WHERE "
              "policy='MONTHLY')");
    s.bind(1, oid);
    s.row();
    return s.num(0);
}
inline long long reserved(Db &d, long long oid) {
    Stmt s(d, "SELECT COALESCE(SUM(total),0) FROM orders WHERE office_id=? AND policy='MONTHLY' "
              "AND status IN ('PLACED','ACCEPTED','READY')");
    s.bind(1, oid);
    s.row();
    return s.num(0);
}
inline bool overdue(Db &d, long long oid) {
    Stmt s(d, "SELECT 1 FROM invoices i JOIN invoice_lines il ON il.invoice_id=i.id WHERE "
              "i.office_id=? AND i.due_at<? AND il.amount>COALESCE((SELECT SUM(a.amount) FROM "
              "allocations a WHERE a.debit_id=il.debit_id),0) LIMIT 1");
    s.bind(1, oid).bind(2, now());
    return s.row();
}
inline void updateSettlement(Db &d, const std::string &order) {
    Stmt s(d, "SELECT COALESCE(SUM(l.amount),0),COALESCE(SUM((SELECT COALESCE(SUM(a.amount),0) "
              "FROM allocations a WHERE a.debit_id=l.id)),0) FROM ledger l WHERE order_id=? AND "
              "kind IN ('CHARGE','DEBIT_ADJUSTMENT')");
    s.bind(1, order);
    s.row();
    auto charge = s.num(0), paid = s.num(1);
    Stmt u(d, "UPDATE orders SET settlement=?,version=version+1 WHERE id=? AND policy='MONTHLY'");
    u.bind(1, paid >= charge && charge > 0 ? "PAID"
              : paid > 0                   ? "PARTIAL"
                                           : "UNPAID")
        .bind(2, order)
        .row();
}
inline void allocate(Db &d, const std::string &credit, long long oid, long long amount,
                     const std::string &order = "") {
    std::string sql = "SELECT l.id,l.order_id,l.amount-COALESCE((SELECT SUM(a.amount) FROM "
                      "allocations a WHERE a.debit_id=l.id),0) AS remaining FROM ledger l WHERE "
                      "l.office_id=? AND l.kind IN ('CHARGE','DEBIT_ADJUSTMENT') AND l.order_id IN "
                      "(SELECT id FROM orders WHERE policy='MONTHLY')";
    if (!order.empty())
        sql += " AND l.order_id=?";
    sql += " ORDER BY l.created,l.rowid";
    Stmt s(d, sql.c_str());
    s.bind(1, oid);
    if (!order.empty())
        s.bind(2, order);
    while (s.row() && amount > 0) {
        auto part = std::min(amount, s.num(2));
        if (part <= 0)
            continue;
        Stmt a(d, "INSERT INTO allocations(id,credit_id,debit_id,amount) VALUES(?,?,?,?)");
        a.bind(1, token()).bind(2, credit).bind(3, s.str(0)).bind(4, part).row();
        amount -= part;
        updateSettlement(d, s.str(1));
    }
    require(amount == 0, 409, "Payment exceeds outstanding fulfilled charges");
}
inline Json orderJson(Db &d, const std::string &id) {
    Stmt s(
        d,
        "SELECT "
        "rowid,office_id,office_name,total,status,settlement,version,policy,created,provider_order,"
        "provider_payment,setup_state,refund_id,address,instructions FROM orders WHERE id=?");
    s.bind(1, id);
    require(s.row(), 404, "Order missing");
    Json j;
    j["id"] = id;
    j["cursor"] = s.num(0);
    j["office_id"] = s.num(1);
    j["office_name"] = s.str(2);
    j["total_paise"] = s.num(3);
    j["status"] = s.str(4);
    j["settlement"] = s.str(5);
    j["version"] = s.num(6);
    j["policy"] = s.str(7);
    j["created"] = s.num(8);
    j["provider_order"] = s.str(9);
    j["provider_payment"] = s.str(10);
    j["setup_state"] = s.str(11);
    j["refund_id"] = s.str(12);
    j["address"] = s.str(13);
    j["instructions"] = s.str(14);
    j["items"] = Json::list();
    Stmt items(
        d, "SELECT item_id,name,price,quantity FROM order_items WHERE order_id=? ORDER BY item_id");
    items.bind(1, id);
    int n = 0;
    while (items.row()) {
        j["items"][n]["item_id"] = items.num(0);
        j["items"][n]["name"] = items.str(1);
        j["items"][n]["price_paise"] = items.num(2);
        j["items"][n++]["quantity"] = items.num(3);
    }
    return j;
}
inline long long cursor(const crow::request &r) {
    auto value = r.url_params.get("before");
    if (!value)
        return std::numeric_limits<long long>::max();
    std::string s = value;
    require(!s.empty() && s.size() <= 18 &&
                std::all_of(s.begin(), s.end(), [](unsigned char c) { return std::isdigit(c); }),
            400, "Invalid cursor");
    return std::stoll(s);
}
inline std::string idem(const crow::request &r) {
    auto key = r.get_header_value("Idempotency-Key");
    require(key.size() >= 8 && key.size() <= 100, 400,
            "Idempotency-Key required (8-100 characters)");
    return identifier(key);
}
inline std::pair<long long, long long> scopedOrder(Db &d, const crow::request &r,
                                                   const std::string &id) {
    long long sid = 0, oid = 0;
    if (!r.get_header_value("X-Office-Token").empty()) {
        auto o = office(r);
        sid = o["stall_id"].i();
        oid = o["office_id"].i();
    } else
        sid = owner(r)["stall_id"].i();
    Stmt s(
        d,
        "SELECT stall_id,office_id FROM orders WHERE id=? AND stall_id=? AND (?=0 OR office_id=?)");
    s.bind(1, id).bind(2, sid).bind(3, oid).bind(4, oid);
    require(s.row(), 404, "Order missing");
    return {s.num(0), s.num(1)};
}
} // namespace orders
