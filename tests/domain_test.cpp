#include "core.h"
#include <iostream>
int main() {
    using namespace cr;
    auto check = [](bool ok) {
        if (!ok)
            throw std::runtime_error("Assertion failed");
    };
    check(total(1500, 3) == 4500);
    bool rejected = false;
    try {
        total(1500, 501);
    } catch (const Failure &) {
        rejected = true;
    }
    check(rejected);
    check(transition("PLACED", "ACCEPTED"));
    check(!transition("PLACED", "FULFILLED"));
    check(!transition("CANCELLED", "ACCEPTED"));
    check(token().size() == 64);
    check(digest("a") != digest("b"));
    check(hmac("key", "message") ==
          "6e9ef29b75fffc5b7abae527d58fdadb2fe42e7219011976917343065f58ed4a");
    check(nextMonth("2026-09") - monthStart("2026-09") == 30 * 86400);
    check(nextMonth("2028-02") - monthStart("2028-02") == 29 * 86400);
    rejected = false;
    try {
        monthStart("2026-13");
    } catch (const Failure &) {
        rejected = true;
    }
    check(rejected);
    Db d(":memory:");
    d.exec("CREATE TABLE t(n INTEGER)");
    {
        Transaction tx(d);
        d.exec("INSERT INTO t VALUES(1)");
    }
    Stmt s(d, "SELECT COUNT(*) FROM t");
    check(s.row() && s.num(0) == 0);
    std::cout << "Money bounds, state transitions, tokens and rollback passed\n";
}
