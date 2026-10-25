#pragma once
namespace orders {
inline crow::response placeOrder(const crow::request &r, const crow::json::rvalue &o) {
            auto j = body(r);
            auto key = idem(r);
            std::map<long long, long long> quantities;
            if (j.has("items")) {
                require(j["items"].t() == crow::json::type::List && j["items"].size() > 0 &&
                            j["items"].size() <= 20,
                        400, "Basket must have 1-20 lines");
                for (auto &line : j["items"]) {
                    auto id = integer(line, "item_id", 1, 1000000000),
                         qty = integer(line, "quantity", 1, 500);
                    quantities[id] += qty;
                    require(quantities[id] <= 500, 400, "Too many cups of one item");
                }
            } else
                quantities[integer(j, "item_id", 1, 1000000000)] = integer(j, "quantity", 1, 500);
            auto requested = optionalText(j, "payment_mode", 20),
                 note = optionalText(j, "note", 300);
            require(requested.empty() || (requested == "PREPAID" || requested == "ADVANCE"), 400,
                    "Choose PREPAID or ADVANCE");
            auto delivery=j.has("delivery_at")?integer(j,"delivery_at",now(),now()+7*86400):now();
            std::string fingerprint = requested + ":" + note+(j.has("delivery_at")?std::to_string(delivery):"");
            long long cups = 0;
            for (auto &[id, qty] : quantities) {
                fingerprint += "|" + std::to_string(id) + ":" + std::to_string(qty);
                cups += qty;
            }
            require(cups <= 500, 400, "Basket exceeds 500 cups");
            auto hash = digest(fingerprint);
            auto sid = o["stall_id"].i(), oid = o["office_id"].i();
            Db d(path);
            Transaction tx(d);
            Stmt prior(d, "SELECT id,request_hash FROM orders WHERE office_id=? AND idem=?");
            prior.bind(1, oid).bind(2, key);
            if (prior.row()) {
                require(prior.str(1) == hash, 409, "Request key reused for another basket");
                auto out = orderJson(d, prior.str(0));
                tx.commit();
                return json(200, std::move(out));
            }
            if(j.has("internal_round_id")){auto round=text(j,"internal_round_id",64);auto day=integer(j,"internal_round_day",(now()+19800)/86400,(now()+19800)/86400);Stmt guard(d,"SELECT 1 FROM recurring_rounds r WHERE r.id=? AND r.office_id=? AND r.paused=0 AND r.archived=0 AND NOT EXISTS(SELECT 1 FROM recurring_exceptions e WHERE e.round_id=r.id AND e.day=?)");guard.bind(1,round).bind(2,oid).bind(3,day);require(guard.row(),409,"Recurring round paused, cancelled or skipped");}
            require(o["verified"].b() && o["owner_verified"].b(), 403,
                    "Verify office and owner email before ordering");
            require(o["ordering_open"].b(), 409, "Stall is paused or outside its opening hours");
            require(o["service_enabled"].b() && !std::string(o["address"].s()).empty() &&
                        std::string(o["area"].s()) == std::string(o["service_area"].s()),
                    409, "Office is outside the configured service area or service is disabled");
            auto dayStart=((now()+19800)/86400)*86400-19800;Stmt intake(d,"SELECT COALESCE(SUM(quantity),0) FROM orders WHERE stall_id=? AND created>=? AND created<? AND status!='CANCELLED'");intake.bind(1,sid).bind(2,dayStart).bind(3,dayStart+86400);intake.row();require(intake.num(0)+cups<=o["daily_capacity"].i(),409,"Stall daily intake capacity is full");
            std::map<long long, std::pair<std::string, long long>> catalog;
            for (auto &item : o["menu"])
                if (item["available"].b())
                    catalog[item["id"].i()] = {std::string(item["name"].s()),
                                               item["price_paise"].i()};
            long long amount = 0;
            for (auto &[id, qty] : quantities) {
                require(catalog.count(id), 409,
                        "A basket item is unavailable or belongs to another stall");
                amount += total(catalog[id].second, qty);
            }
            std::map<long long,std::string> promotions;
            for(auto &m:o["menu"])if(m.has("promotion_id")&&quantities.count(m["id"].i())){
                auto item=m["id"].i();auto promo=std::string(m["promotion_id"].s());require(delivery>=now()+m["notice_minutes"].i()*60,409,"Meeting pack or offer requires more advance notice");
                Stmt used(d,"SELECT COALESCE(SUM(i.quantity),0) FROM order_items i JOIN orders o ON o.id=i.order_id WHERE i.promotion_id=? AND o.status!='CANCELLED'");used.bind(1,promo);used.row();require(used.num(0)+quantities[item]<=m["maximum_units"].i(),409,"Offer capacity is exhausted");promotions[item]=promo;
            }
            require(amount <= 100000000, 400, "Order value too large");
            if(j.has("maximum_total_paise")) require(amount<=integer(j,"maximum_total_paise",1,100000000),409,"Price exceeds approved recurring budget");
            auto policy = requested.empty() ? std::string(o["policy"].s()) : requested;
            if (policy == "MONTHLY") {
                require(o["credit_enabled"].b() && !overdue(d, oid), 409,
                        "Credit is disabled or overdue; choose prepayment");
                require(reserved(d, oid) + outstanding(d, oid) + amount <=
                            o["credit_limit_paise"].i(),
                        409, "Credit limit exceeded; choose prepayment");
            } else if(policy=="ADVANCE") {
                requireFinancialReview();require(advanceBalance(d,oid)-advanceReserved(d,oid)>=amount,409,"Advance balance insufficient; choose another payment method");
            } else account(sid);
            auto id = token();
            auto first = quantities.begin();
            Stmt insert(d,
                        "INSERT INTO "
                        "orders(id,stall_id,office_id,office_name,item_id,item_name,quantity,total,"
                        "status,settlement,policy,idem,request_hash,created,address,instructions,"
                        "policy_version) VALUES(?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?)");
            insert.bind(1, id)
                .bind(2, sid)
                .bind(3, oid)
                .bind(4, std::string(o["office_name"].s()))
                .bind(5, first->first)
                .bind(6, catalog[first->first].first)
                .bind(7, cups)
                .bind(8, amount)
                .bind(9, policy == "PREPAID" ? "PAYMENT_PENDING" : "PLACED")
                .bind(10, policy == "PREPAID" ? "PAYMENT_PENDING" : "UNPAID")
                .bind(11, policy)
                .bind(12, key)
                .bind(13, hash)
                .bind(14, now())
                .bind(15, std::string(o["address"].s()))
                .bind(16, std::string(o["instructions"].s()) + " " + note+(j.has("delivery_at")?"; requested delivery epoch="+std::to_string(delivery):""))
                .bind(17, o["version"].i())
                .row();
            Stmt cycle(d,"UPDATE orders SET billing_cycle=? WHERE id=?");cycle.bind(1,std::string(o["billing_cycle"].s())).bind(2,id).row();
            std::map<long long,long long> costs,listPrices;for(auto &c:o["internal_costs"])costs[c["item_id"].i()]=c["cost_paise"].i();for(auto &m:o["menu"])listPrices[m["id"].i()]=m.has("list_price_paise")?m["list_price_paise"].i():m["price_paise"].i();
            for (auto &[item, qty] : quantities) {
                Stmt line(d, "INSERT INTO order_items(order_id,item_id,name,price,quantity,promotion_id,cost_paise,list_price) VALUES(?,?,?,?,?,?,?,?)");
                line.bind(1, id)
                    .bind(2, item)
                    .bind(3, catalog[item].first)
                    .bind(4, catalog[item].second)
                    .bind(5, qty)
                    .bind(6,promotions[item]).bind(7,costs.count(item)?costs[item]:-1).bind(8,listPrices[item])
                    .row();
            }
            event(d, id, policy == "PREPAID" ? "PREPAYMENT_REQUIRED" : "PLACED");
            audit(d, "ORDER_CREATED", id, "office:" + std::to_string(oid), policy);
            tx.commit();
            return json(201, orderJson(d, id));
}
}
