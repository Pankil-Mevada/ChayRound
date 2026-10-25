#pragma once
#include "repository.h"
namespace orders {
inline Json accountView(Db &d, const crow::json::rvalue &o, const crow::request &r) {
    auto oid = o["office_id"].i();
    Json j;
    j["office_id"] = oid;
    j["office_name"] = std::string(o["office_name"].s());
    j["billing_cycle"]=std::string(o["billing_cycle"].s());
    j["outstanding_paise"] = outstanding(d, oid);
    j["reserved_paise"] = reserved(d, oid);
    j["committed_paise"] = outstanding(d, oid) + reserved(d, oid);
    j["credit_limit_paise"] = o["credit_limit_paise"].i();
    j["overdue"] = overdue(d, oid);
    Stmt balance(d, "SELECT COALESCE(SUM(amount),0) FROM ledger WHERE office_id=?");
    balance.bind(1, oid);
    balance.row();
    j["ledger_balance_paise"] = balance.num(0);
    j["entries"] = Json::list();
    Stmt s(d, "SELECT rowid,id,order_id,kind,amount,reference,actor,created FROM ledger WHERE "
              "office_id=? AND rowid<? ORDER BY rowid DESC LIMIT 51");
    s.bind(1, oid).bind(2, cursor(r));
    int i = 0;
    long long next = 0;
    while (s.row()) {
        if (i == 50) {
            j["next_cursor"] = next;
            break;
        }
        next = s.num(0);
        j["entries"][i]["id"] = s.str(1);
        j["entries"][i]["order_id"] = s.str(2);
        j["entries"][i]["kind"] = s.str(3);
        j["entries"][i]["amount_paise"] = s.num(4);
        j["entries"][i]["reference"] = s.str(5);
        j["entries"][i]["actor"] = s.str(6);
        j["entries"][i++]["created"] = s.num(7);
    }
    j["invoices"] = Json::list();
    Stmt inv(d, "SELECT "
                "i.id,i.period,i.due_at,COALESCE(SUM(il.amount),0),COALESCE(SUM(il.amount-COALESCE("
                "(SELECT SUM(a.amount) FROM allocations a WHERE a.debit_id=il.debit_id),0)),0) "
                "FROM invoices i LEFT JOIN invoice_lines il ON il.invoice_id=i.id WHERE "
                "i.office_id=? GROUP BY i.id ORDER BY i.period DESC");
    inv.bind(1, oid);
    i = 0;
    while (inv.row()) {
        j["invoices"][i]["id"] = inv.str(0);
        j["invoices"][i]["period"] = inv.str(1);
        j["invoices"][i]["due_at"] = inv.num(2);
        j["invoices"][i]["total_paise"] = inv.num(3);
        j["invoices"][i++]["outstanding_paise"] = inv.num(4);
    }
    return j;
}
inline void billing(crow::SimpleApp &app) {
    CROW_ROUTE(app, "/api/account")([](const crow::request &r) {
        return handle([&] {
            auto o = office(r);
            Db d(path);
            return json(200, accountView(d, o, r));
        });
    });
    CROW_ROUTE(app, "/api/accounts/<int>")([](const crow::request &r, int oid) {
        return handle([&] {
            auto o = ownedOffice(r, oid);
            Db d(path);
            return json(200, accountView(d, o, r));
        });
    });
    CROW_ROUTE(app, "/api/accounts/<int>/payments")
        .methods("POST"_method)([](const crow::request &r, int oid) {
            return handle([&] {
                auto j = body(r);
                auto o = ownedOffice(r, oid), who = owner(r);
                auto sid = who["stall_id"].i();
                auto actor = "owner:" + std::to_string(who["owner_id"].i());
                auto amount = integer(j, "amount_paise", 1, 100000000);
                auto method = text(j, "method", 20), reference = text(j, "reference", 120),
                     key = idem(r);
                require(method == "CASH" || method == "BANK", 400,
                        "Use CASH or BANK; this action records owner-confirmed receipt, not an "
                        "online capture");
                auto hash = digest(std::to_string(oid) + ":" + std::to_string(amount) + ":" +
                                   method + ":" + reference);
                Db d(path);
                Transaction tx(d);
                Stmt prior(d, "SELECT id,hash FROM receipts WHERE stall_id=? AND idem=?");
                prior.bind(1, sid).bind(2, key);
                if (prior.row()) {
                    require(prior.str(1) == hash, 409, "Receipt key reused for another payment");
                    Json out;
                    out["receipt_id"] = prior.str(0);
                    tx.commit();
                    return json(200, std::move(out));
                }
                Stmt duplicate(
                    d, "SELECT id FROM receipts WHERE stall_id=? AND method=? AND reference=?");
                duplicate.bind(1, sid).bind(2, method).bind(3, reference);
                require(!duplicate.row(), 409, "Payment reference already recorded");
                Stmt advance(d,"SELECT id FROM advance_entries WHERE stall_id=? AND reference=? AND kind='TOPUP'");advance.bind(1,sid).bind(2,reference);require(!advance.row(),409,"Money reference already recorded as advance funds");
                require(amount <= outstanding(d, oid), 409,
                        "Amount exceeds unpaid fulfilled charges");
                auto id = token();
                Stmt s(d, "INSERT INTO receipts VALUES(?,?,?,?,?,?,?,?)");
                s.bind(1, id)
                    .bind(2, sid)
                    .bind(3, oid)
                    .bind(4, method)
                    .bind(5, reference)
                    .bind(6, amount)
                    .bind(7, key)
                    .bind(8, hash)
                    .row();
                entry(d, id, sid, oid, "", "PAYMENT", -amount, method + ": " + reference, actor);
                allocate(d, id, oid, amount);
                audit(d, "RECEIPT_RECORDED", id, actor, reference);
                tx.commit();
                Json out;
                out["receipt_id"] = id;
                return json(201, std::move(out));
            });
        });
    CROW_ROUTE(app, "/api/receipts/<string>/reverse")
        .methods("POST"_method)([](const crow::request &r, std::string receipt) {
            return handle([&] {
                auto j = body(r);
                auto reason = text(j, "reason", 300);
                auto who = owner(r);
                auto sid = who["stall_id"].i();
                auto actor = "owner:" + std::to_string(who["owner_id"].i());
                Db d(path);
                Transaction tx(d);
                Stmt s(d, "SELECT office_id,amount FROM receipts WHERE id=? AND stall_id=?");
                s.bind(1, receipt).bind(2, sid);
                require(s.row(), 404, "Receipt missing");
                auto oid = s.num(0), amount = s.num(1);
                Stmt old(d, "SELECT entry_id FROM reversals WHERE receipt_id=?");
                old.bind(1, receipt);
                if (old.row()) {
                    Json out;
                    out["reversal_id"] = old.str(0);
                    tx.commit();
                    return json(200, std::move(out));
                }
                auto id = token();
                entry(d, id, sid, oid, "", "PAYMENT_REVERSAL", amount, reason, actor);
                Stmt allocations(
                    d, "SELECT a.id,a.debit_id,a.amount,l.order_id FROM allocations a JOIN ledger "
                       "l ON l.id=a.debit_id WHERE a.credit_id=? AND a.reversal_of IS NULL");
                allocations.bind(1, receipt);
                while (allocations.row()) {
                    Stmt reverse(d, "INSERT INTO allocations VALUES(?,?,?,?,?)");
                    reverse.bind(1, token())
                        .bind(2, id)
                        .bind(3, allocations.str(1))
                        .bind(4, -allocations.num(2))
                        .bind(5, allocations.str(0))
                        .row();
                    updateSettlement(d, allocations.str(3));
                }
                Stmt save(d, "INSERT INTO reversals VALUES(?,?)");
                save.bind(1, receipt).bind(2, id).row();
                audit(d, "RECEIPT_REVERSED", receipt, actor, reason);
                tx.commit();
                Json out;
                out["reversal_id"] = id;
                return json(201, std::move(out));
            });
        });
    CROW_ROUTE(app, "/api/orders/<string>/adjustment")
        .methods("POST"_method)([](const crow::request &r, std::string id) {
            return handle([&] {
                auto j = body(r);
                auto amount = integer(j, "amount_paise", -100000000, 100000000);
                require(amount != 0, 400, "Adjustment must be nonzero");
                auto reason = text(j, "reason", 300), key = idem(r);
                auto who = owner(r);
                auto sid = who["stall_id"].i();
                auto actor = "owner:" + std::to_string(who["owner_id"].i());
                auto hash = digest(id + ":" + std::to_string(amount) + ":" + reason);
                Db d(path);
                Transaction tx(d);
                Stmt old(d, "SELECT hash,result FROM command_keys WHERE stall_id=? AND idem=?");
                old.bind(1, sid).bind(2, key);
                if (old.row()) {
                    require(old.str(0) == hash, 409, "Adjustment key conflict");
                    Json out;
                    out["entry_id"] = old.str(1);
                    tx.commit();
                    return json(200, std::move(out));
                }
                Stmt s(d, "SELECT office_id FROM orders WHERE id=? AND stall_id=? AND "
                          "status='FULFILLED' AND policy='MONTHLY'");
                s.bind(1, id).bind(2, sid);
                require(s.row(), 409, "Adjustments require an owned fulfilled monthly order");
                auto oid = s.num(0);
                auto entryId = token();
                entry(d, entryId, sid, oid, id, amount > 0 ? "DEBIT_ADJUSTMENT" : "CREDIT_NOTE",
                      amount, reason, actor);
                if (amount < 0)
                    allocate(d, entryId, oid, -amount, id);
                else
                    updateSettlement(d, id);
                Stmt save(d, "INSERT INTO command_keys VALUES(?,?,?,?)");
                save.bind(1, sid).bind(2, key).bind(3, hash).bind(4, entryId).row();
                audit(d, "CHARGE_ADJUSTED", id, actor, reason);
                tx.commit();
                Json out;
                out["entry_id"] = entryId;
                return json(201, std::move(out));
            });
        });
    CROW_ROUTE(app, "/api/accounts/<int>/invoices")
        .methods("POST"_method)([](const crow::request &r, int oid) {
            return handle([&] {
                auto j = body(r);
                auto period = text(j, "period", 7);
                auto start = monthStart(period), end = nextMonth(period);
                require(end <= now(), 409,
                        "Only closed calendar months can be invoiced; current-month amounts remain "
                        "on the account");
                auto o = ownedOffice(r, oid), who = owner(r);
                auto sid = who["stall_id"].i();
                Db d(path);
                Transaction tx(d);
                Stmt old(d, "SELECT id FROM invoices WHERE office_id=? AND period=?");
                old.bind(1, oid).bind(2, period);
                if (old.row()) {
                    Json out;
                    out["invoice_id"] = old.str(0);
                    tx.commit();
                    return json(200, std::move(out));
                }
                auto id = token();
                Stmt invoice(d, "INSERT INTO invoices VALUES(?,?,?,?,?,?)");
                invoice.bind(1, id)
                    .bind(2, sid)
                    .bind(3, oid)
                    .bind(4, period)
                    .bind(5, end + 7 * 86400)
                    .bind(6, now())
                    .row();
                Stmt lines(d, "INSERT INTO invoice_lines SELECT ?,l.id,l.amount FROM ledger l JOIN "
                              "orders o ON o.id=l.order_id WHERE l.office_id=? AND l.kind IN "
                              "('CHARGE','DEBIT_ADJUSTMENT') AND o.policy='MONTHLY' AND o.billing_cycle='MONTHLY' AND "
                              "l.created>=? AND l.created<?");
                lines.bind(1, id).bind(2, oid).bind(3, start).bind(4, end).row();
                audit(d, "INVOICE_ISSUED", id, "owner:" + std::to_string(who["owner_id"].i()),
                      period);
                tx.commit();
                Json out;
                out["invoice_id"] = id;
                out["due_at"] = end + 7 * 86400;
                return json(201, std::move(out));
            });
        });
    CROW_ROUTE(app, "/api/invoices/<string>")([](const crow::request &r, std::string id) {
        return handle([&] {
            Db d(path);
            bool isOffice = !r.get_header_value("X-Office-Token").empty();
            auto identity = isOffice ? office(r) : owner(r);
            auto sid = identity["stall_id"].i(), oid = isOffice ? identity["office_id"].i() : 0;
            Stmt s(d, "SELECT office_id,period,due_at FROM invoices WHERE id=? AND stall_id=? AND "
                      "(?=0 OR office_id=?)");
            s.bind(1, id).bind(2, sid).bind(3, oid).bind(4, oid);
            require(s.row(), 404, "Invoice missing");
            Json j;
            j["id"] = id;
            j["office_id"] = s.num(0);
            j["period"] = s.str(1);
            j["due_at"] = s.num(2);
            j["currency"] = "INR";
            j["timezone"] = "Asia/Kolkata";
            j["tax_note"] =
                "Account statement; tax invoice fields require merchant tax configuration";
            j["lines"] = Json::list();
            Stmt lines(
                d, "SELECT l.id,l.order_id,l.kind,il.amount,COALESCE((SELECT SUM(a.amount) FROM "
                   "allocations a WHERE a.debit_id=l.id),0) FROM invoice_lines il JOIN ledger l ON "
                   "l.id=il.debit_id WHERE il.invoice_id=? ORDER BY l.created,l.rowid");
            lines.bind(1, id);
            int i = 0;
            long long total = 0, remaining = 0;
            while (lines.row()) {
                j["lines"][i]["entry_id"] = lines.str(0);
                j["lines"][i]["order_id"] = lines.str(1);
                j["lines"][i]["kind"] = lines.str(2);
                j["lines"][i]["amount_paise"] = lines.num(3);
                j["lines"][i++]["outstanding_paise"] = lines.num(3) - lines.num(4);
                total += lines.num(3);
                remaining += lines.num(3) - lines.num(4);
            }
            j["total_paise"] = total;
            j["outstanding_paise"] = remaining;
            return json(200, std::move(j));
        });
    });
}
} // namespace orders
