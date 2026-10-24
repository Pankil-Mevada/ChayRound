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
                     "default-src 'self'; script-src 'self' https://checkout.razorpay.com; "
                     "style-src 'self' 'unsafe-inline'; frame-src https://api.razorpay.com "
                     "https://checkout.razorpay.com; connect-src 'self' https://api.razorpay.com "
                     "https://checkout.razorpay.com; frame-ancestors 'none'");
        return r;
    });
    CROW_ROUTE(app, "/<string>")([](std::string name) {
        if (name != "app.js" && name != "styles.css")
            return crow::response(404);
        std::ifstream f(env("CHAY_FRONTEND", "frontend") + "/" + name);
        if (!f)
            return crow::response(503, "Asset unavailable");
        std::string source((std::istreambuf_iterator<char>(f)), {});
        crow::response response(200, source);
        response.set_header("Content-Type", name == "app.js"
                                                ? "application/javascript; charset=utf-8"
                                                : "text/css; charset=utf-8");
        response.set_header("Cache-Control", "no-store");
        response.set_header("X-Content-Type-Options", "nosniff");
        return response;
    });
    CROW_ROUTE(app, "/api/<path>")
        .methods("GET"_method, "POST"_method)([](const crow::request &r, std::string path) {
            return handle([&] {
                require(r.body.size() <= 65536, 413, "Request too large");
                require(path.find("..") == std::string::npos && path.find('%') == std::string::npos,
                        400, "Invalid path");
                bool orders = path == "office" || path == "orders" ||
                              path.rfind("orders/", 0) == 0 || path == "account" ||
                              path.rfind("accounts/", 0) == 0 || path.rfind("receipts/", 0) == 0 ||
                              path.rfind("invoices/", 0) == 0 || path.rfind("webhooks/", 0) == 0 ||
                              path == "operations";
                auto base = orders ? env("CHAY_ORDER_URL", "http://127.0.0.1:9182")
                                   : env("CHAY_MERCHANT_URL", "http://127.0.0.1:9181");
                auto method = r.method == crow::HTTPMethod::Post ? "POST" : "GET";
                auto question = r.raw_url.find('?');
                auto query =
                    question == std::string::npos ? std::string() : r.raw_url.substr(question);
                require(query.size() <= 200, 400, "Query too long");
                auto out =
                    http(base + "/api/" + path + query, method, r.body,
                         {"Authorization: " + r.get_header_value("Authorization"),
                          "X-Office-Token: " + r.get_header_value("X-Office-Token"),
                          "X-Office-Pin: " + r.get_header_value("X-Office-Pin"),
                          "X-Razorpay-Signature: " + r.get_header_value("X-Razorpay-Signature"),
                          "X-Razorpay-Event-Id: " + r.get_header_value("X-Razorpay-Event-Id"),
                          "Idempotency-Key: " + r.get_header_value("Idempotency-Key")});
                crow::response response(static_cast<int>(out.status), out.body);
                response.set_header("Content-Type", "application/json");
                response.set_header("Cache-Control", "no-store");
                return response;
            });
        });
    app.loglevel(crow::LogLevel::Warning)
        .bindaddr("127.0.0.1")
        .port(port(9180))
        .concurrency(4)
        .run();
}
