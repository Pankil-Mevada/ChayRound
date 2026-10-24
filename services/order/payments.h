#pragma once
#include "repository.h"
#include <condition_variable>
#include <mutex>
#include <thread>
namespace orders {
struct Account {
    std::string key, secret, webhook;
};
inline Account account(long long sid) {
    auto file = env("CHAY_RAZORPAY_ACCOUNTS_FILE");
    require(!file.empty(), 503, "Razorpay Test Mode is not configured for this stall");
    std::ifstream input(file);
    require(bool(input), 503, "Payment account configuration unavailable");
    std::string raw((std::istreambuf_iterator<char>(input)), {});
    auto config = crow::json::load(raw);
    auto name = std::to_string(sid);
    require(config && config.has(name), 503, "Razorpay Test Mode is not configured for this stall");
    Account a{text(config[name], "key_id", 100), text(config[name], "key_secret", 200),
              optionalText(config[name], "webhook_secret", 200)};
    require(a.key.rfind("rzp_test_", 0) == 0, 503,
            "Only Razorpay Test Mode is enabled; live collections are blocked");
    return a;
}
inline crow::json::rvalue provider(const Account &a, const std::string &path,
                                   const std::string &method = "GET", const std::string &data = "",
                                   const std::vector<std::string> &headers = {}) {
    auto r = http(endpoint("CHAY_RAZORPAY_TEST_URL", "https://api.razorpay.com/v1") + path, method,
                  data, headers, a.key + ":" + a.secret);
    require(
        r.status >= 200 && r.status < 300, 503,
        "Payment provider rejected or could not complete the request; reconcile before retrying");
    auto j = crow::json::load(r.body);
    require(bool(j), 503, "Invalid payment provider response");
    return j;
}
inline void validateProviderOrder(const crow::json::rvalue &j, const std::string &receipt,
                                  long long amount) {
    require(j.has("id") && j.has("amount") && j.has("currency") && j.has("receipt") &&
                j["amount"].i() == amount && std::string(j["currency"].s()) == "INR" &&
                std::string(j["receipt"].s()) == receipt,
            503, "Provider order mismatch");
    identifier(std::string(j["id"].s()));
}
inline std::string receipt(const std::string &id) { return "cr_" + id.substr(0, 32); }
inline Json checkout(Db &d, const std::string &id, long long sid) {
    auto a = account(sid);
    long long amount;
    std::string providerId, state, status;
    {
        Transaction tx(d);
        Stmt s(d, "SELECT total,provider_order,setup_state,status,policy FROM orders WHERE id=? "
                  "AND stall_id=?");
        s.bind(1, id).bind(2, sid);
        require(s.row(), 404, "Order missing");
        amount = s.num(0);
        providerId = s.str(1);
        state = s.str(2);
        status = s.str(3);
        require(s.str(4) == "PREPAID" &&
                    (status == "PAYMENT_PENDING" || (status == "CANCELLED" && state != "NONE")),
                409, "Order does not await prepayment");
        if (providerId.empty() && state == "NONE") {
            Stmt u(d, "UPDATE orders SET setup_state='CREATING' WHERE id=?");
            u.bind(1, id).row();
        }
        tx.commit();
    }
    if (providerId.empty()) {
        try {
            crow::json::rvalue result;
            if (state == "NONE") {
                Json payload;
                payload["amount"] = amount;
                payload["currency"] = "INR";
                payload["receipt"] = receipt(id);
                result = provider(a, "/orders", "POST", payload.dump());
            } else {
                auto found = provider(a, "/orders?receipt=" + receipt(id) + "&count=100");
                require(found.has("items") && found["items"].size() == 1, 409,
                        "Payment setup outcome is uncertain; reconcile the receipt with provider "
                        "support before creating another attempt");
                // A nested Crow rvalue borrows its root's parse buffer. Own a new
                // root before the collection leaves scope.
                result = crow::json::load(Json(found["items"][0]).dump());
            }
            validateProviderOrder(result, receipt(id), amount);
            providerId = result["id"].s();
            Transaction tx(d);
            Stmt u(d, "UPDATE orders SET provider_order=?,setup_state='READY' WHERE id=? AND "
                      "provider_order=''");
            u.bind(1, providerId).bind(2, id).row();
            tx.commit();
        } catch (...) {
            Stmt u(d, "UPDATE orders SET setup_state='UNKNOWN' WHERE id=? AND provider_order=''");
            u.bind(1, id).row();
            throw;
        }
    }
    Json out;
    out["key_id"] = a.key;
    out["provider_order_id"] = providerId;
    out["amount_paise"] = amount;
    out["currency"] = "INR";
    out["test_mode"] = true;
    return out;
}
inline void applyCapture(Db &d, const std::string &id, long long sid, const crow::json::rvalue &p) {
    auto paymentId = identifier(text(p, "id", 100)), providerOrder = text(p, "order_id", 100),
         currency = text(p, "currency", 10), status = text(p, "status", 30);
    auto amount = integer(p, "amount", 1, 100000000);
    require(currency == "INR" && status == "captured", 409, "Payment is not captured");
    Transaction tx(d);
    Stmt s(d, "SELECT office_id,total,status,provider_order,provider_payment FROM orders WHERE "
              "id=? AND stall_id=? AND policy='PREPAID'");
    s.bind(1, id).bind(2, sid);
    require(s.row(), 404, "Prepaid order missing");
    require(s.num(1) == amount && s.str(3) == providerOrder, 409,
            "Payment does not match the stored order");
    if (!s.str(4).empty()) {
        require(s.str(4) == paymentId, 409, "Another payment is already recorded");
        tx.commit();
        return;
    }
    auto oid = s.num(0);
    auto charge = "charge_" + id, credit = "payment_" + paymentId;
    entry(d, charge, sid, oid, id, "CHARGE", amount, "Verified prepaid capture", "provider");
    entry(d, credit, sid, oid, id, "PAYMENT", -amount, paymentId, "provider");
    Stmt alloc(d, "INSERT INTO allocations(id,credit_id,debit_id,amount) VALUES(?,?,?,?)");
    alloc.bind(1, token()).bind(2, credit).bind(3, charge).bind(4, amount).row();
    bool cancelled = s.str(2) == "CANCELLED";
    Stmt u(d, "UPDATE orders SET provider_payment=?,settlement=?,status=CASE WHEN "
              "status='PAYMENT_PENDING' THEN 'PLACED' ELSE status END,refund_key=CASE WHEN "
              "refund_key='' THEN ? ELSE refund_key END,version=version+1 WHERE id=?");
    u.bind(1, paymentId)
        .bind(2, cancelled ? "REFUND_PENDING" : "PAID")
        .bind(3, token())
        .bind(4, id)
        .row();
    event(d, id, cancelled ? "LATE_CAPTURE_REFUND_REQUIRED" : "PAYMENT_CAPTURED");
    audit(d, "CAPTURE_VERIFIED", id, "provider", paymentId);
    tx.commit();
}
inline void finishRefund(Db &d, const std::string &id, long long sid,
                         const crow::json::rvalue &refund) {
    auto refundId = identifier(text(refund, "id", 100)),
         paymentId = text(refund, "payment_id", 100), status = text(refund, "status", 30);
    auto amount = integer(refund, "amount", 1, 100000000);
    Transaction tx(d);
    Stmt s(d, "SELECT office_id,total,provider_payment,status,settlement,refund_id FROM orders "
              "WHERE id=? AND stall_id=?");
    s.bind(1, id).bind(2, sid);
    require(s.row() && s.str(2) == paymentId && s.num(1) == amount && s.str(3) == "CANCELLED", 409,
            "Refund does not match a cancelled paid order");
    require(s.str(5).empty() || s.str(5) == refundId, 409, "Different refund already recorded");
    Stmt u(d, "UPDATE orders SET refund_id=? WHERE id=?");
    u.bind(1, refundId).bind(2, id).row();
    if (status == "processed" && s.str(4) != "REFUNDED") {
        entry(d, "refund_" + id, sid, s.num(0), id, "REFUND", amount, refundId, "provider");
        entry(d, "cancel_" + id, sid, s.num(0), id, "CREDIT_NOTE", -amount,
              "Cancelled prepaid order", "provider");
        Stmt done(d, "UPDATE orders SET settlement='REFUNDED',version=version+1 WHERE id=?");
        done.bind(1, id).row();
        event(d, id, "REFUND_PROCESSED");
        audit(d, "REFUND_VERIFIED", id, "provider", refundId);
    }
    tx.commit();
}
inline Json reconcile(Db &d, const std::string &id, long long sid) {
    auto a = account(sid);
    Stmt s(d, "SELECT provider_order,refund_id,provider_payment,status FROM orders WHERE id=? AND "
              "stall_id=?");
    s.bind(1, id).bind(2, sid);
    require(s.row(), 404, "Order missing");
    auto providerOrder = s.str(0), refundId = s.str(1);
    if (!refundId.empty()) {
        auto refund = provider(a, "/refunds/" + identifier(refundId));
        finishRefund(d, id, sid, refund);
    } else if (!providerOrder.empty()) {
        auto payments = provider(a, "/orders/" + identifier(providerOrder) + "/payments");
        require(payments.has("items"), 503, "Invalid payment collection");
        for (auto &p : payments["items"]) {
            auto state = std::string(p["status"].s());
            if (state == "captured") {
                auto verified = provider(a, "/payments/" + identifier(std::string(p["id"].s())));
                applyCapture(d, id, sid, verified);
            }
        }
    } else
        checkout(d, id, sid);
    return orderJson(d, id);
}
inline Json refund(Db &d, const std::string &id, long long sid) {
    auto a = account(sid);
    std::string paymentId, key, refundId;
    long long amount;
    {
        Transaction tx(d);
        Stmt s(d, "SELECT provider_payment,refund_key,refund_id,total,status,settlement FROM "
                  "orders WHERE id=? AND stall_id=?");
        s.bind(1, id).bind(2, sid);
        require(s.row() && s.str(4) == "CANCELLED" && !s.str(0).empty(), 409,
                "Only cancelled captured prepaid orders can be refunded");
        if (s.str(5) == "REFUNDED") {
            tx.commit();
            return orderJson(d, id);
        }
        paymentId = s.str(0);
        key = s.str(1);
        refundId = s.str(2);
        amount = s.num(3);
        if (key.empty()) {
            key = token();
            Stmt u(d, "UPDATE orders SET refund_key=? WHERE id=?");
            u.bind(1, key).bind(2, id).row();
        }
        tx.commit();
    }
    if (refundId.empty()) {
        Json request;
        request["amount"] = amount;
        auto result = provider(a, "/payments/" + identifier(paymentId) + "/refund", "POST",
                               request.dump(), {"X-Refund-Idempotency: " + key});
        refundId = identifier(text(result, "id", 100));
        Stmt u(d, "UPDATE orders SET refund_id=? WHERE id=? AND refund_id=''");
        u.bind(1, refundId).bind(2, id).row();
    }
    auto verified = provider(a, "/refunds/" + identifier(refundId));
    finishRefund(d, id, sid, verified);
    return orderJson(d, id);
}
// Claim work before network I/O; stable provider keys and local unique records
// make a retry after a crashed worker safe.
inline std::jthread reconciliationWorker() {
    if (env("CHAY_RECONCILE_WORKER", "0") != "1")
        return {};
    return std::jthread([](std::stop_token stop) {
        std::mutex mutex;
        std::condition_variable_any wake;
        while (!stop.stop_requested()) {
            try {
                Db d(path);
                std::string id, lease;
                long long sid = 0, attempt = 0;
                {
                    Transaction tx(d);
                    Stmt s(
                        d,
                        "SELECT id,stall_id,reconcile_attempts FROM orders WHERE policy='PREPAID' "
                        "AND next_reconcile<=? AND created<=? AND (status='PAYMENT_PENDING' OR "
                        "settlement='REFUND_PENDING' OR (status='CANCELLED' AND "
                        "provider_payment='' AND (provider_order!='' OR setup_state IN "
                        "('CREATING','UNKNOWN')))) ORDER BY next_reconcile,rowid LIMIT 1");
                    s.bind(1, now()).bind(2, now() - 30);
                    if (s.row()) {
                        id = s.str(0);
                        sid = s.num(1);
                        attempt = s.num(2) + 1;
                        lease = token();
                        Stmt claim(
                            d,
                            "UPDATE orders SET "
                            "next_reconcile=?,reconcile_attempts=?,reconcile_lease=? WHERE id=?");
                        claim.bind(1, now() + 60).bind(2, attempt).bind(3, lease).bind(4, id).row();
                    }
                    tx.commit();
                }
                if (!id.empty()) {
                    bool failed = false;
                    try {
                        auto result = reconcile(d, id, sid);
                        auto parsed = crow::json::load(result.dump());
                        if (std::string(parsed["settlement"].s()) == "REFUND_PENDING")
                            refund(d, id, sid);
                    } catch (const std::exception &) {
                        failed = true;
                    }
                    auto delay =
                        failed ? std::min(300LL, 5LL * (1LL << std::min(attempt, 6LL))) : 30LL;
                    Stmt finish(d, "UPDATE orders SET next_reconcile=?,last_reconcile_error=? "
                                   "WHERE id=? AND reconcile_lease=?");
                    finish.bind(1, now() + delay)
                        .bind(2, failed ? "Provider recovery pending; see operations runbook" : "")
                        .bind(3, id)
                        .bind(4, lease)
                        .row();
                }
            } catch (const std::exception &) { /* Retry after storage recovery. */
            }
            std::unique_lock lock(mutex);
            wake.wait_for(lock, stop, std::chrono::seconds(2), [] { return false; });
        }
    });
}
} // namespace orders
