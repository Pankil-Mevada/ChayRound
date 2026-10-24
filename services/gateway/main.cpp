#include "core.h"
using namespace cr;
int main() {
    curl_global_init(CURL_GLOBAL_DEFAULT);
    crow::SimpleApp app;
    CROW_ROUTE(app, "/health")([] {
        Json j;
        j["status"] = "ok";
        return json(200, std::move(j));
    });
    CROW_ROUTE(app, "/")([] {
        std::ifstream f(env("CHAY_FRONTEND", "frontend") + "/index.html");
        if (!f)
            return crow::response(503, "Frontend unavailable");
        std::string s((std::istreambuf_iterator<char>(f)), {});
        crow::response r(200, s);
        r.set_header("Content-Type", "text/html; charset=utf-8");
        r.set_header("Cache-Control", "no-store");
        r.set_header("Referrer-Policy", "no-referrer");
        r.set_header("X-Content-Type-Options", "nosniff");
        r.set_header("Content-Security-Policy",
                     "default-src 'self'; script-src 'self' 'unsafe-inline'; "
                     "style-src 'self' 'unsafe-inline'; frame-ancestors 'none'");
        return r;
    });
    CROW_ROUTE(app, "/api/<path>")
        .methods("GET"_method, "POST"_method)([](const crow::request &r, std::string path) {
            return handle([&] {
                require(r.body.size() <= 16384, 413, "Request too large");
                require(path.find("..") == std::string::npos && path.find('%') == std::string::npos,
                        400, "Invalid path");
                bool orders = path == "office" || path == "orders" || path.rfind("orders/", 0) == 0;
                auto base = orders ? env("CHAY_ORDER_URL", "http://127.0.0.1:9182")
                                   : env("CHAY_MERCHANT_URL", "http://127.0.0.1:9181");
                auto method = r.method == crow::HTTPMethod::Post ? "POST" : "GET";
                auto out = http(base + "/api/" + path, method, r.body,
                                {"Authorization: " + r.get_header_value("Authorization"),
                                 "X-Office-Token: " + r.get_header_value("X-Office-Token"),
                                 "Idempotency-Key: " + r.get_header_value("Idempotency-Key")});
                crow::response response(static_cast<int>(out.status), out.body);
                response.set_header("Content-Type", "application/json");
                response.set_header("Cache-Control", "no-store");
                return response;
            });
        });
    app.bindaddr("127.0.0.1").port(port(9180)).concurrency(4).run();
}
