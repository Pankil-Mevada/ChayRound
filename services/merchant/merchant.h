#pragma once
#include "core.h"
#include <ctime>
namespace merchant {
using namespace cr;
inline std::string path;
inline void migrate() {
    Db d(path);
    Transaction tx(d);
    d.exec(R"SQL(
CREATE TABLE IF NOT EXISTS owners(id INTEGER PRIMARY KEY,email TEXT UNIQUE NOT NULL,salt TEXT NOT NULL,password TEXT NOT NULL);
CREATE TABLE IF NOT EXISTS sessions(hash TEXT PRIMARY KEY,owner_id INTEGER NOT NULL REFERENCES owners(id),expires INTEGER NOT NULL);
CREATE TABLE IF NOT EXISTS stalls(id INTEGER PRIMARY KEY,owner_id INTEGER UNIQUE NOT NULL REFERENCES owners(id),name TEXT NOT NULL);
CREATE TABLE IF NOT EXISTS menu(id INTEGER PRIMARY KEY,stall_id INTEGER NOT NULL REFERENCES stalls(id),name TEXT NOT NULL,price INTEGER NOT NULL CHECK(price>0));
CREATE TABLE IF NOT EXISTS offices(id INTEGER PRIMARY KEY,stall_id INTEGER NOT NULL REFERENCES stalls(id),name TEXT NOT NULL,token_hash TEXT UNIQUE NOT NULL,policy TEXT NOT NULL CHECK(policy IN ('PREPAID','MONTHLY')),credit_limit INTEGER NOT NULL CHECK(credit_limit>=0));
CREATE TABLE IF NOT EXISTS challenges(id TEXT PRIMARY KEY,subject TEXT NOT NULL,code_hash TEXT NOT NULL,expires INTEGER NOT NULL,attempts INTEGER NOT NULL DEFAULT 0,consumed INTEGER NOT NULL DEFAULT 0,created INTEGER NOT NULL);
CREATE INDEX IF NOT EXISTS challenges_subject ON challenges(subject,created);
CREATE TABLE IF NOT EXISTS rate_limits(key TEXT PRIMARY KEY,start INTEGER NOT NULL,count INTEGER NOT NULL);
)SQL");
    column(d, "owners", "verified", "INTEGER NOT NULL DEFAULT 0");
    column(d, "stalls", "area", "TEXT NOT NULL DEFAULT ''");
    column(d, "stalls", "paused", "INTEGER NOT NULL DEFAULT 1");
    column(d, "stalls", "opens", "INTEGER NOT NULL DEFAULT 360");
    column(d, "stalls", "closes", "INTEGER NOT NULL DEFAULT 1320");
    column(d, "stalls", "days", "INTEGER NOT NULL DEFAULT 127");
    column(d, "stalls", "version", "INTEGER NOT NULL DEFAULT 1");
    column(d, "menu", "available", "INTEGER NOT NULL DEFAULT 1");
    column(d, "menu", "version", "INTEGER NOT NULL DEFAULT 1");
    for (auto name : {"email", "address", "area", "instructions", "pin_hash", "pin_salt"})
        column(d, "offices", name, "TEXT NOT NULL DEFAULT ''");
    column(d, "offices", "verified", "INTEGER NOT NULL DEFAULT 0");
    column(d, "offices", "owner_approved", "INTEGER NOT NULL DEFAULT 0");
    d.exec("CREATE TABLE IF NOT EXISTS office_devices(id TEXT PRIMARY KEY,token_hash TEXT UNIQUE NOT NULL,stall_id INTEGER NOT NULL REFERENCES stalls(id),office_name TEXT NOT NULL,office_id INTEGER REFERENCES offices(id),status TEXT NOT NULL DEFAULT 'PENDING',created INTEGER NOT NULL);");
    column(d, "offices", "service_enabled", "INTEGER NOT NULL DEFAULT 1");
    column(d, "offices", "credit_enabled", "INTEGER NOT NULL DEFAULT 1");
    column(d, "offices", "version", "INTEGER NOT NULL DEFAULT 1");
    column(d, "challenges", "email", "TEXT NOT NULL DEFAULT ''");
    auditSchema(d);
    d.exec("PRAGMA user_version=2");
    tx.commit();
    d.exec("PRAGMA journal_mode=WAL");
}
inline void throttle(Db &d, const std::string &key, long long limit, long long seconds) {
    Transaction tx(d);
    Stmt s(d, "SELECT start,count FROM rate_limits WHERE key=?");
    s.bind(1, key);
    long long start = now(), count = 0;
    if (s.row() && s.num(0) + seconds > now()) {
        start = s.num(0);
        count = s.num(1);
    }
    require(count < limit, 429, "Too many attempts; try later");
    Stmt u(d, "INSERT INTO rate_limits VALUES(?,?,?) ON CONFLICT(key) DO UPDATE SET "
              "start=excluded.start,count=excluded.count");
    u.bind(1, key).bind(2, start).bind(3, count + 1).row();
    tx.commit();
}
inline long long owner(Db &d, const crow::request &r, bool verified = true) {
    auto auth = r.get_header_value("Authorization");
    require(auth.rfind("Bearer ", 0) == 0, 401, "Sign in required");
    Stmt s(d, "SELECT s.owner_id,o.verified FROM sessions s JOIN owners o ON o.id=s.owner_id WHERE "
              "hash=? AND expires>?");
    s.bind(1, digest(auth.substr(7))).bind(2, now());
    require(s.row(), 401, "Session expired");
    require(!verified || s.num(1), 403, "Verify your owner email first");
    return s.num(0);
}
inline long long stall(Db &d, long long uid) {
    Stmt s(d, "SELECT id FROM stalls WHERE owner_id=?");
    s.bind(1, uid);
    require(s.row(), 404, "Stall missing");
    return s.num(0);
}
inline long long officeId(Db &d, const crow::request &r) {
    auto key = r.get_header_value("X-Office-Token");
    require(key.size() == 64, 401, "Private office link required");
    Stmt s(d, "SELECT id,pin_hash,pin_salt FROM offices WHERE token_hash=?");
    s.bind(1, digest(key));
    if (!s.row()) {
        Stmt device(d, "SELECT office_id FROM office_devices WHERE token_hash=? AND status='APPROVED'");
        device.bind(1, digest(key));
        require(device.row(), 401, "Office link invalid or revoked");
        return device.num(0);
    }
    if (!s.str(1).empty()) {
        auto pin = r.get_header_value("X-Office-Pin");
        require(pin.size() >= 4 && pin.size() <= 8 && equal(password(pin, s.str(2)), s.str(1)), 401,
                "Office PIN required or incorrect");
    }
    return s.num(0);
}
inline Json menu(Db &d, long long sid) {
    Stmt s(d, "SELECT id,name,price,available,version FROM menu WHERE stall_id=? ORDER BY id");
    s.bind(1, sid);
    Json list = Json::list();
    int i = 0;
    while (s.row()) {
        list[i]["id"] = s.num(0);
        list[i]["name"] = s.str(1);
        list[i]["price_paise"] = s.num(2);
        list[i]["available"] = bool(s.num(3));
        list[i++]["version"] = s.num(4);
    }
    return list;
}
inline bool open(long long opens, long long closes, long long days) {
    auto t = static_cast<time_t>(now() + 19800);
    std::tm local{};
    gmtime_r(&t, &local);
    auto minute = local.tm_hour * 60 + local.tm_min;
    return (days & (1 << local.tm_wday)) &&
           (opens == closes || (opens < closes ? (minute >= opens && minute < closes)
                                               : (minute >= opens || minute < closes)));
}
inline Json officeInfo(Db &d, long long oid) {
    Stmt s(d, "SELECT "
              "o.stall_id,o.name,o.policy,o.credit_limit,o.email,o.address,o.area,o.instructions,o."
              "verified,o.service_enabled,o.credit_enabled,o.version,s.name,s.area,s.paused,s."
              "opens,s.closes,s.days,u.verified,o.owner_approved FROM offices o JOIN stalls s ON s.id=o.stall_id "
              "JOIN owners u ON u.id=s.owner_id WHERE o.id=?");
    s.bind(1, oid);
    require(s.row(), 404, "Office missing");
    Json j;
    j["office_id"] = oid;
    j["stall_id"] = s.num(0);
    j["office_name"] = s.str(1);
    j["policy"] = s.str(2);
    j["credit_limit_paise"] = s.num(3);
    j["email"] = s.str(4);
    j["address"] = s.str(5);
    j["area"] = s.str(6);
    j["instructions"] = s.str(7);
    j["verified"] = bool(s.num(8) || s.num(19));
    j["email_verified"] = bool(s.num(8));
    j["owner_approved"] = bool(s.num(19));
    j["service_enabled"] = bool(s.num(9));
    j["credit_enabled"] = bool(s.num(10));
    j["version"] = s.num(11);
    j["stall_name"] = s.str(12);
    j["service_area"] = s.str(13);
    j["ordering_open"] = !s.num(14) && open(s.num(15), s.num(16), s.num(17));
    j["owner_verified"] = bool(s.num(18));
    j["menu"] = menu(d, s.num(0));
    return j;
}
inline std::string pinHash(const std::string &pin, const std::string &salt) {
    if (pin.empty())
        return "";
    require(
        pin.size() >= 4 && pin.size() <= 8 &&
            std::all_of(pin.begin(), pin.end(), [](unsigned char c) { return std::isdigit(c); }),
        400, "PIN must contain 4-8 digits");
    return password(pin, salt);
}
inline void verification(crow::SimpleApp &app) {
    CROW_ROUTE(app, "/api/verification/request").methods("POST"_method)([](const crow::request &r) {
        return handle([&] {
            Db d(path);
            bool isOffice = !r.get_header_value("X-Office-Token").empty();
            auto id = isOffice ? officeId(d, r) : owner(d, r, false);
            auto subject = (isOffice ? "office:" : "owner:") + std::to_string(id);
            throttle(d, "verify:" + subject, 3, 600);
            Stmt s(d, isOffice ? "SELECT email FROM offices WHERE id=?"
                               : "SELECT email FROM owners WHERE id=?");
            s.bind(1, id);
            s.row();
            auto email = emailAddress(s.str(0));
            require(!env("CHAY_EMAIL_API_KEY").empty() && !env("CHAY_EMAIL_FROM").empty(), 503,
                    "Email delivery is not configured; verification remains pending");
            unsigned random = 0;
            require(RAND_bytes(reinterpret_cast<unsigned char *>(&random), sizeof random) == 1, 500,
                    "Random generator failed");
            auto code = std::to_string(100000 + random % 900000);
            auto challenge = token();
            Json mail;
            mail["from"] = env("CHAY_EMAIL_FROM");
            mail["to"] = Json::list();
            mail["to"][0] = email;
            mail["subject"] = "ChayRound verification code";
            mail["text"] =
                "Your ChayRound verification code is " + code + ". It expires in 10 minutes.";
            Stmt insert(d, "INSERT INTO challenges(id,subject,code_hash,expires,created,email) "
                           "VALUES(?,?,?,?,?,?)");
            insert.bind(1, challenge)
                .bind(2, subject)
                .bind(3, hmac(serviceKey(), challenge + ":" + code))
                .bind(4, now() + 600)
                .bind(5, now())
                .bind(6, email)
                .row();
            auto sent = http(endpoint("CHAY_EMAIL_TEST_URL", "https://api.resend.com/emails"),
                             "POST", mail.dump(),
                             {"Authorization: Bearer " + env("CHAY_EMAIL_API_KEY"),
                              "Idempotency-Key: verify-" + challenge});
            require(sent.status >= 200 && sent.status < 300, 503,
                    "Verification email could not be delivered; retry later");
            Json out;
            out["challenge_id"] = challenge;
            out["expires_in"] = 600;
            return json(200, std::move(out));
        });
    });
    CROW_ROUTE(app, "/api/verification/confirm").methods("POST"_method)([](const crow::request &r) {
        return handle([&] {
            auto j = body(r);
            auto challenge = text(j, "challenge_id", 64), code = text(j, "code", 6);
            Db d(path);
            bool isOffice = !r.get_header_value("X-Office-Token").empty();
            auto id = isOffice ? officeId(d, r) : owner(d, r, false);
            auto subject = (isOffice ? "office:" : "owner:") + std::to_string(id);
            Transaction tx(d);
            Stmt s(d, "SELECT code_hash,expires,attempts,consumed,email FROM challenges WHERE id=? "
                      "AND subject=?");
            s.bind(1, challenge).bind(2, subject);
            require(s.row() && s.num(1) > now() && s.num(2) < 5 && !s.num(3), 400,
                    "Challenge expired, exhausted, or already used");
            Stmt contact(d, isOffice ? "SELECT email FROM offices WHERE id=?"
                                     : "SELECT email FROM owners WHERE id=?");
            contact.bind(1, id);
            contact.row();
            bool valid = equal(s.str(0), hmac(serviceKey(), challenge + ":" + code)) &&
                         s.str(4) == contact.str(0);
            Stmt attempt(d, "UPDATE challenges SET attempts=attempts+1,consumed=? WHERE id=?");
            attempt.bind(1, valid ? 1 : 0).bind(2, challenge).row();
            if (valid) {
                Stmt v(d, isOffice ? "UPDATE offices SET verified=1 WHERE id=?"
                                   : "UPDATE owners SET verified=1 WHERE id=?");
                v.bind(1, id).row();
                audit(d, "CONTACT_VERIFIED", subject, subject, "Email code verified");
            }
            tx.commit();
            require(valid, 400, "Incorrect verification code");
            Json out;
            out["verified"] = true;
            return json(200, std::move(out));
        });
    });
}
} // namespace merchant
