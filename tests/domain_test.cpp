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
