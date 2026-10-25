#include "billing.h"
#include "payments.h"
#include "growth.h"
#include "reporting.h"
#include "settlement.h"
#include "advance.h"
#include "rewards.h"
#include "checkout.h"
#include "reminders.h"
#include "marketing_worker.h"
#include "automation.h"
using namespace cr;
using namespace orders;
int main() {
    serviceKey();
    curl_global_init(CURL_GLOBAL_DEFAULT);
    path = env("CHAY_DB", "orders.db");
    migrate();
    crow::SimpleApp app;
    billing(app);
    growth(app);
    reporting(app);
    settlement(app);
    advances(app);
    rewards(app);
    automation(app);
    CROW_ROUTE(app, "/health")([] {
        return handle([] {
            Db d(path);
            Stmt s(d, "SELECT COUNT(*) FROM ledger");
            s.row();
            Json j;
            j["status"] = "ok";
            j["schema_version"] = 2;
            return json(200, std::move(j));
        });
    });
    CROW_ROUTE(app, "/api/office")([](const crow::request &r) {
        return handle([&] {
            auto o = office(r);
            Db d(path);
            Json out;
            for(const auto *key:{"office_id","stall_id","office_name","policy","credit_limit_paise","email","address","area","instructions","verified","email_verified","owner_approved","service_enabled","credit_enabled","version","stall_name","service_area","ordering_open","owner_verified","menu","billing_cycle"})out[key]=Json(o[key]);
            auto oid = o["office_id"].i();
            out["committed_paise"] = reserved(d, oid) + outstanding(d, oid);
            out["overdue"] = overdue(d, oid);
            return json(200, std::move(out));
        });
    });
    CROW_ROUTE(app, "/api/orders").methods("POST"_method)([](const crow::request &r) {
        return handle([&] {
            return placeOrder(r, office(r));
        });
    });
    CROW_ROUTE(app, "/api/orders")([](const crow::request &r) {
        return handle([&] {
            bool isOffice = !r.get_header_value("X-Office-Token").empty();
            auto identity = isOffice ? office(r) : owner(r);
            auto scope = identity[isOffice ? "office_id" : "stall_id"].i();
            Db d(path);
            std::string sql = "SELECT id,rowid FROM orders WHERE ";
            sql += isOffice ? "office_id=?" : "stall_id=?";
            sql += " AND rowid<? ORDER BY rowid DESC LIMIT 51";
            Stmt s(d, sql.c_str());
            s.bind(1, scope).bind(2, cursor(r));
            Json out;
            out["orders"] = Json::list();
            int i = 0;
            long long next = 0;
            while (s.row()) {
                if (i == 50) {
                    out["next_cursor"] = next;
                    break;
                }
                next = s.num(1);
                out["orders"][i++] = orderJson(d, s.str(0));
            }
            return json(200, std::move(out));
        });
    });
    CROW_ROUTE(app, "/api/orders/<string>/status")
        .methods("POST"_method)([](const crow::request &r, std::string id) {
            return handle([&] {
                auto j = body(r);
                auto target = text(j, "status");
                auto version = integer(j, "version", 1, 1000000000);
                auto reason = optionalText(j, "reason", 300);
                if (target == "CANCELLED")
                    require(!reason.empty(), 400, "Cancellation reason required");
                auto who = owner(r);
                auto sid = who["stall_id"].i();
                auto actor = "owner:" + std::to_string(who["owner_id"].i());
                Db d(path);
                Transaction tx(d);
                Stmt s(d, "SELECT status,version,office_id,total,policy,settlement FROM orders "
                          "WHERE id=? AND stall_id=?");
                s.bind(1, id).bind(2, sid);
                require(s.row(), 404, "Order missing");
                require(s.num(1) == version, 409, "Order changed; refresh");
                require(transition(s.str(0), target) ||
                            (s.str(0) == "PAYMENT_PENDING" && target == "CANCELLED"),
                        409, "Invalid state transition");
                require(target == "CANCELLED" || s.str(4) != "PREPAID" || s.str(5) == "PAID", 409,
                        "Verified capture required before preparation");
                if (target == "FULFILLED" && s.str(4) == "MONTHLY")
                    entry(d, "charge_" + id, sid, s.num(2), id, "CHARGE", s.num(3),
                          "Fulfilled tea order", actor);
                if(target=="FULFILLED" && s.str(4)=="ADVANCE") spendAdvance(d,id,sid,s.num(2),s.num(3),actor);
                Stmt u(d, "UPDATE orders SET status=?,fulfilled_at=CASE WHEN ?='FULFILLED' THEN ? "
                          "ELSE fulfilled_at END,settlement=CASE WHEN ?='CANCELLED' AND "
                          "settlement='PAID' AND policy='PREPAID' THEN 'REFUND_PENDING' WHEN ?='FULFILLED' AND policy='ADVANCE' THEN 'PAID' ELSE "
                          "settlement END,version=version+1 WHERE id=? AND version=?");
                u.bind(1, target)
                    .bind(2, target)
                    .bind(3, now())
                    .bind(4, target)
                    .bind(5, target)
                    .bind(6, id)
                    .bind(7, version)
                    .row();
                require(sqlite3_changes(d.p) == 1, 409, "Order changed");
                event(d, id, target);
                audit(d, "ORDER_" + target, id, actor, reason);
                tx.commit();
                return json(200, orderJson(d, id));
            });
        });
    CROW_ROUTE(app, "/api/orders/<string>/checkout")
        .methods("POST"_method)([](const crow::request &r, std::string id) {
            return handle([&] {
                Db d(path);
                auto [sid, oid] = scopedOrder(d, r, id);
                (void)oid;
                return json(200, checkout(d, id, sid));
            });
        });
    CROW_ROUTE(app, "/api/orders/<string>/verify")
        .methods("POST"_method)([](const crow::request &r, std::string id) {
            return handle([&] {
                auto j = body(r);
                auto payment = identifier(text(j, "payment_id", 100)),
                     signature = text(j, "signature", 64);
                Db d(path);
                auto [sid, oid] = scopedOrder(d, r, id);
                (void)oid;
                auto a = account(sid);
                Stmt s(d, "SELECT provider_order FROM orders WHERE id=?");
                s.bind(1, id);
                s.row();
                require(!s.str(0).empty() &&
                            equal(hmac(a.secret, s.str(0) + "|" + payment), signature),
                        400, "Payment signature mismatch");
                auto verified = provider(a, "/payments/" + payment);
                applyCapture(d, id, sid, verified);
                return json(200, orderJson(d, id));
            });
        });
    CROW_ROUTE(app, "/api/orders/<string>/reconcile")
        .methods("POST"_method)([](const crow::request &r, std::string id) {
            return handle([&] {
                auto who = owner(r);
                auto sid = who["stall_id"].i();
                Db d(path);
                Stmt s(d, "SELECT id FROM orders WHERE id=? AND stall_id=?");
                s.bind(1, id).bind(2, sid);
                require(s.row(), 404, "Order missing");
                return json(200, reconcile(d, id, sid));
            });
        });
    CROW_ROUTE(app, "/api/orders/<string>/refund")
        .methods("POST"_method)([](const crow::request &r, std::string id) {
            return handle([&] {
                auto who = owner(r);
                Db d(path);
                return json(200, refund(d, id, who["stall_id"].i()));
            });
        });
    CROW_ROUTE(app, "/api/webhooks/<int>")
        .methods("POST"_method)([](const crow::request &r, int sid) {
            return handle([&] {
                require(r.body.size() <= 65536, 413, "Webhook too large");
                auto a = account(sid);
                require(!a.webhook.empty() && equal(hmac(a.webhook, r.body),
                                                    r.get_header_value("X-Razorpay-Signature")),
                        401, "Invalid webhook signature");
                auto j = body(r);
                auto eventId = r.get_header_value("X-Razorpay-Event-Id");
                if (eventId.empty())
                    eventId = digest(r.body);
                require(eventId.size() <= 100, 400, "Invalid event id");
                Db d(path);
                Stmt old(d, "SELECT 1 FROM webhook_events WHERE stall_id=? AND event_id=?");
                old.bind(1, sid).bind(2, eventId);
                Json out;
                out["received"] = true;
                if (old.row())
                    return json(200, std::move(out));
                auto kind = text(j, "event", 100);
                if (kind == "payment.captured") {
                    auto payment = identifier(text(j["payload"]["payment"]["entity"], "id", 100));
                    auto p = provider(a, "/payments/" + payment);
                    auto providerOrder = text(p, "order_id", 100);
                    Stmt s(d, "SELECT id FROM orders WHERE stall_id=? AND provider_order=?");
                    s.bind(1, sid).bind(2, providerOrder);
                    require(s.row(), 409, "Unknown provider order; reconcile setup first");
                    applyCapture(d, s.str(0), sid, p);
                } else if (kind == "refund.processed") {
                    auto refundId = identifier(text(j["payload"]["refund"]["entity"], "id", 100));
                    auto verified = provider(a, "/refunds/" + refundId);
                    Stmt s(d, "SELECT id FROM orders WHERE stall_id=? AND provider_payment=?");
                    s.bind(1, sid).bind(2, text(verified, "payment_id", 100));
                    require(s.row(), 409, "Unknown payment; reconcile first");
                    finishRefund(d, s.str(0), sid, verified);
                }
                Stmt save(d, "INSERT OR IGNORE INTO webhook_events VALUES(?,?,?)");
                save.bind(1, sid).bind(2, eventId).bind(3, now()).row();
                return json(200, std::move(out));
            });
        });
    CROW_ROUTE(app, "/api/operations")([](const crow::request &r) {
        return handle([&] {
            auto sid = owner(r)["stall_id"].i();
            Db d(path);
            Json out;
            for (auto state : {"PAYMENT_PENDING", "REFUND_PENDING"}) {
                Stmt s(
                    d,
                    "SELECT COUNT(*) FROM orders WHERE stall_id=? AND (status=? OR settlement=?)");
                s.bind(1, sid).bind(2, std::string(state)).bind(3, std::string(state));
                s.row();
                out[state] = s.num(0);
            }
            Stmt unknown(d, "SELECT COUNT(*) FROM orders WHERE stall_id=? AND setup_state IN "
                            "('UNKNOWN','CREATING')");
            unknown.bind(1, sid);
            unknown.row();
            out["uncertain_setup"] = unknown.num(0);
            return json(200, std::move(out));
        });
    });
    auto worker = reconciliationWorker();
    auto rounds = recurringWorker();
    app.loglevel(crow::LogLevel::Warning)
        .bindaddr("127.0.0.1")
        .port(port(9182))
        .concurrency(4)
        .run();
}
