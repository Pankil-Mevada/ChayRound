#pragma once
#include <algorithm>
#include <cctype>
#include <chrono>
#include <crow.h>
#include <cstdlib>
#include <curl/curl.h>
#include <fstream>
#include <limits>
#include <map>
#include <memory>
#include <openssl/crypto.h>
#include <openssl/evp.h>
#include <openssl/hmac.h>
#include <openssl/rand.h>
#include <regex>
#include <sqlite3.h>
#include <stdexcept>
#include <string>
#include <vector>

namespace cr {
using Json = crow::json::wvalue;
struct Failure : std::runtime_error {
    int status;
    Failure(int s, std::string m) : std::runtime_error(m), status(s) {}
};
inline void require(bool ok, int status, const std::string &message) {
    if (!ok)
        throw Failure(status, message);
}
inline std::string env(const char *key, const std::string &fallback = "") {
    auto p = std::getenv(key);
    return p ? std::string(p) : fallback;
}
inline long long now() {
    if (env("CHAY_TEST_FIXTURES") == "1" && !env("CHAY_TEST_CLOCK_FILE").empty()) {
        std::ifstream clock(env("CHAY_TEST_CLOCK_FILE"));
        long long value = 0;
        require(bool(clock >> value) && value > 0, 500, "Invalid isolated test clock");
        return value;
    }
    return std::chrono::duration_cast<std::chrono::seconds>(
               std::chrono::system_clock::now().time_since_epoch())
        .count();
}
inline std::string hex(const unsigned char *bytes, size_t n) {
    const char *digits = "0123456789abcdef";
    std::string s;
    for (size_t i = 0; i < n; ++i) {
        s += digits[bytes[i] >> 4];
        s += digits[bytes[i] & 15];
    }
    return s;
}
inline std::string token() {
    unsigned char b[32];
    require(RAND_bytes(b, sizeof b) == 1, 500, "Random generator failed");
    return hex(b, sizeof b);
}
inline std::string digest(const std::string &s) {
    unsigned char b[EVP_MAX_MD_SIZE];
    unsigned n = 0;
    require(EVP_Digest(s.data(), s.size(), b, &n, EVP_sha256(), nullptr) == 1, 500,
            "Digest failed");
    return hex(b, n);
}
inline std::string password(const std::string &p, const std::string &salt) {
    unsigned char b[32];
    require(PKCS5_PBKDF2_HMAC(p.c_str(), static_cast<int>(p.size()),
                              reinterpret_cast<const unsigned char *>(salt.data()),
                              static_cast<int>(salt.size()), 600000, EVP_sha256(), 32, b) == 1,
            500, "Password derivation failed");
    return hex(b, 32);
}
inline bool equal(const std::string &a, const std::string &b) {
    return a.size() == b.size() && CRYPTO_memcmp(a.data(), b.data(), a.size()) == 0;
}
inline std::string hmac(const std::string &key, const std::string &message) {
    unsigned char result[EVP_MAX_MD_SIZE];
    unsigned length = 0;
    require(HMAC(EVP_sha256(), key.data(), static_cast<int>(key.size()),
                 reinterpret_cast<const unsigned char *>(message.data()), message.size(), result,
                 &length) != nullptr,
            500, "HMAC failed");
    return hex(result, length);
}
inline std::string emailAddress(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return std::tolower(c); });
    require(s.size() <= 200 && std::regex_match(s, std::regex(R"([^\s@]+@[^\s@]+\.[^\s@]+)")), 400,
            "Invalid email");
    return s;
}
inline std::string optionalText(const crow::json::rvalue &j, const char *key, size_t max = 300) {
    if (!j.has(key))
        return "";
    require(j[key].t() == crow::json::type::String, 400, "Expected text");
    std::string value = j[key].s();
    require(value.size() <= max, 400, "Text too long");
    return value;
}
inline bool flag(const crow::json::rvalue &j, const char *key) {
    require(j.has(key) &&
                (j[key].t() == crow::json::type::True || j[key].t() == crow::json::type::False),
            400, std::string("Expected boolean: ") + key);
    return j[key].b();
}
inline std::string identifier(const std::string &s) {
    require(
        !s.empty() && s.size() <= 100 &&
            std::all_of(s.begin(), s.end(),
                        [](unsigned char c) { return std::isalnum(c) || c == '_' || c == '-'; }),
        400, "Invalid identifier");
    return s;
}
inline std::string text(const crow::json::rvalue &j, const char *k, size_t max = 120) {
    require(j.has(k) && j[k].t() == crow::json::type::String, 400,
            std::string("Missing text: ") + k);
    std::string s = j[k].s();
    require(!s.empty() && s.size() <= max, 400, std::string("Invalid text: ") + k);
    return s;
}
inline long long integer(const crow::json::rvalue &j, const char *k, long long lo, long long hi) {
    require(j.has(k) && j[k].t() == crow::json::type::Number, 400,
            std::string("Missing number: ") + k);
    double d = j[k].d();
    require(d >= lo && d <= hi && d == static_cast<long long>(d), 400,
            std::string("Invalid integer: ") + k);
    return static_cast<long long>(d);
}
inline crow::json::rvalue body(const crow::request &r) {
    require(r.body.size() <= 65536, 413, "Request too large");
    auto j = crow::json::load(r.body);
    require(j && j.t() == crow::json::type::Object, 400, "Expected JSON object");
    return j;
}
inline crow::response json(int status, Json value) {
    crow::response r(status, value.dump());
    r.set_header("Content-Type", "application/json");
    r.set_header("Cache-Control", "no-store");
    return r;
}
template <class F> crow::response handle(F f) {
    try {
        return f();
    } catch (const Failure &e) {
        Json j;
        j["error"] = e.what();
        return json(e.status, std::move(j));
    } catch (const std::exception &) {
        Json j;
        j["error"] = "Internal service error";
        return json(500, std::move(j));
    }
}
struct Db {
    sqlite3 *p = nullptr;
    explicit Db(const std::string &path) {
        if (sqlite3_open(path.c_str(), &p) != SQLITE_OK) {
            if (p)
                sqlite3_close(p);
            p = nullptr;
            throw std::runtime_error("Database open failed");
        }
        sqlite3_busy_timeout(p, 5000);
        exec("PRAGMA foreign_keys=ON;");
    }
    ~Db() {
        if (p)
            sqlite3_close(p);
    }
    Db(const Db &) = delete;
    Db &operator=(const Db &) = delete;
    void exec(const std::string &s) {
        char *err = nullptr;
        if (sqlite3_exec(p, s.c_str(), nullptr, nullptr, &err) != SQLITE_OK) {
            sqlite3_free(err);
            throw std::runtime_error("Database operation failed");
        }
    }
};
struct Stmt {
    sqlite3_stmt *p = nullptr;
    explicit Stmt(Db &d, const char *s) {
        if (sqlite3_prepare_v2(d.p, s, -1, &p, nullptr) != SQLITE_OK)
            throw std::runtime_error("Prepare failed");
    }
    ~Stmt() { sqlite3_finalize(p); }
    Stmt(const Stmt &) = delete;
    Stmt &bind(int i, const std::string &s) {
        if (sqlite3_bind_text(p, i, s.c_str(), -1, SQLITE_TRANSIENT) != SQLITE_OK)
            throw std::runtime_error("Bind failed");
        return *this;
    }
    Stmt &bind(int i, long long n) {
        if (sqlite3_bind_int64(p, i, n) != SQLITE_OK)
            throw std::runtime_error("Bind failed");
        return *this;
    }
    bool row() {
        int rc = sqlite3_step(p);
        if (rc == SQLITE_ROW)
            return true;
        if (rc == SQLITE_DONE)
            return false;
        if (rc == SQLITE_BUSY || rc == SQLITE_LOCKED)
            throw Failure(503, "Database busy; retry with the same request key");
        throw std::runtime_error("Statement failed");
    }
    std::string str(int i) {
        auto s = sqlite3_column_text(p, i);
        return s ? reinterpret_cast<const char *>(s) : "";
    }
    long long num(int i) { return sqlite3_column_int64(p, i); }
};
struct Transaction {
    Db &d;
    bool committed = false;
    explicit Transaction(Db &db) : d(db) { d.exec("BEGIN IMMEDIATE"); }
    void commit() {
        d.exec("COMMIT");
        committed = true;
    }
    ~Transaction() {
        if (!committed)
            sqlite3_exec(d.p, "ROLLBACK", nullptr, nullptr, nullptr);
    }
};
inline void column(Db &d, const std::string &table, const std::string &name,
                   const std::string &definition) {
    Stmt s(d, ("PRAGMA table_info(" + table + ")").c_str());
    while (s.row())
        if (s.str(1) == name)
            return;
    d.exec("ALTER TABLE " + table + " ADD COLUMN " + name + " " + definition);
}
inline void audit(Db &d, const std::string &kind, const std::string &entity,
                  const std::string &actor, const std::string &details) {
    Stmt s(d, "INSERT INTO audit(kind,entity,actor,details,created) VALUES(?,?,?,?,?)");
    s.bind(1, kind).bind(2, entity).bind(3, actor).bind(4, details).bind(5, now()).row();
}
inline void auditSchema(Db &d) {
    d.exec("CREATE TABLE IF NOT EXISTS audit(id INTEGER PRIMARY KEY,kind TEXT NOT NULL,entity TEXT "
           "NOT NULL,actor TEXT NOT NULL,details TEXT NOT NULL,created INTEGER NOT NULL);"
           "CREATE TRIGGER IF NOT EXISTS audit_no_update BEFORE UPDATE ON audit BEGIN SELECT "
           "RAISE(ABORT,'immutable audit'); END;"
           "CREATE TRIGGER IF NOT EXISTS audit_no_delete BEFORE DELETE ON audit BEGIN SELECT "
           "RAISE(ABORT,'immutable audit'); END;");
}
inline long long total(long long price, long long qty) {
    require(price >= 1 && price <= 1000000 && qty >= 1 && qty <= 500, 400,
            "Invalid price or quantity");
    return price * qty;
}
inline bool transition(const std::string &from, const std::string &to) {
    return (from == "PLACED" && (to == "ACCEPTED" || to == "CANCELLED")) ||
           (from == "ACCEPTED" && (to == "READY" || to == "CANCELLED")) ||
           (from == "READY" && (to == "FULFILLED" || to == "CANCELLED"));
}
struct HttpResult {
    long status = 0;
    std::string body;
};
inline size_t receive(char *p, size_t a, size_t b, void *ctx) {
    auto &out = *static_cast<std::string *>(ctx);
    if (out.size() + a * b > 2 * 1024 * 1024)
        return 0;
    out.append(p, a * b);
    return a * b;
}
inline HttpResult http(const std::string &url, const std::string &method = "GET",
                       const std::string &payload = "",
                       const std::vector<std::string> &headers = {},
                       const std::string &basic = "") {
    std::unique_ptr<CURL, decltype(&curl_easy_cleanup)> c(curl_easy_init(), curl_easy_cleanup);
    require(bool(c), 503, "HTTP unavailable");
    HttpResult out;
    curl_slist *list = nullptr;
    for (auto &h : headers) {
        list = curl_slist_append(list, h.c_str());
    }
    list = curl_slist_append(list, "Content-Type: application/json");
    std::unique_ptr<curl_slist, decltype(&curl_slist_free_all)> guard(list, curl_slist_free_all);
    curl_easy_setopt(c.get(), CURLOPT_URL, url.c_str());
    curl_easy_setopt(c.get(), CURLOPT_CUSTOMREQUEST, method.c_str());
    curl_easy_setopt(c.get(), CURLOPT_HTTPHEADER, list);
    curl_easy_setopt(c.get(), CURLOPT_CONNECTTIMEOUT_MS, 1000L);
    curl_easy_setopt(c.get(), CURLOPT_TIMEOUT_MS, 12000L);
    curl_easy_setopt(c.get(), CURLOPT_NOSIGNAL, 1L);
    curl_easy_setopt(c.get(), CURLOPT_WRITEFUNCTION, receive);
    curl_easy_setopt(c.get(), CURLOPT_WRITEDATA, &out.body);
    if (!basic.empty()) {
        curl_easy_setopt(c.get(), CURLOPT_HTTPAUTH, CURLAUTH_BASIC);
        curl_easy_setopt(c.get(), CURLOPT_USERPWD, basic.c_str());
    }
    if (method != "GET") {
        curl_easy_setopt(c.get(), CURLOPT_POSTFIELDS, payload.c_str());
        curl_easy_setopt(c.get(), CURLOPT_POSTFIELDSIZE, static_cast<long>(payload.size()));
    }
    auto code = curl_easy_perform(c.get());
    require(code == CURLE_OK, 503,
            std::string("Dependency unavailable: ") + curl_easy_strerror(code));
    curl_easy_getinfo(c.get(), CURLINFO_RESPONSE_CODE, &out.status);
    return out;
}
inline std::string serviceKey() {
    auto s = env("CHAY_INTERNAL_KEY");
    require(s.size() >= 32, 500, "CHAY_INTERNAL_KEY must contain at least 32 characters");
    return s;
}
inline void internal(const crow::request &r) {
    require(equal(r.get_header_value("X-Internal-Key"), serviceKey()), 401,
            "Internal access denied");
}
inline crow::json::rvalue remote(const std::string &path, const std::string &method = "GET",
                                 const std::string &data = "", const std::string &auth = "") {
    auto r = http(env("CHAY_MERCHANT_URL", "http://127.0.0.1:9181") + path, method, data,
                  {"X-Internal-Key: " + serviceKey(), "Authorization: " + auth});
    require(r.status == 200, static_cast<int>(r.status),
            r.status >= 500 ? "Merchant service unavailable" : "Access or configuration denied");
    auto j = crow::json::load(r.body);
    require(bool(j), 503, "Invalid dependency response");
    return j;
}
inline int port(int fallback) { return std::stoi(env("PORT", std::to_string(fallback))); }
inline std::string endpoint(const char *overrideKey, const std::string &normal) {
    auto target = env(overrideKey);
    if (target.empty())
        return normal;
    require(env("CHAY_TEST_FIXTURES") == "1" && target.rfind("http://127.0.0.1:", 0) == 0, 500,
            "Provider overrides are limited to isolated loopback fixtures");
    return target;
}
inline long long monthStart(const std::string &period) {
    require(std::regex_match(period, std::regex(R"(20[0-9]{2}-(0[1-9]|1[0-2]))")), 400,
            "Use YYYY-MM");
    auto date = std::chrono::year(std::stoi(period.substr(0, 4))) /
                std::chrono::month(std::stoi(period.substr(5, 2))) / 1;
    return std::chrono::duration_cast<std::chrono::seconds>(
               std::chrono::sys_days(date).time_since_epoch())
               .count() -
           19800;
}
inline long long nextMonth(const std::string &period) {
    auto date = std::chrono::year(std::stoi(period.substr(0, 4))) /
                std::chrono::month(std::stoi(period.substr(5, 2))) / 1;
    return std::chrono::duration_cast<std::chrono::seconds>(
               std::chrono::sys_days(date + std::chrono::months(1)).time_since_epoch())
               .count() -
           19800;
}
} // namespace cr
