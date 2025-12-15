import struct
import random
import math
import csv
from collections import defaultdict

N = 10_000_000

FAIR_PRICE = 100_000
SPREAD_TICKS = 2
BUY_RATIO = 0.5
TICK_SIZE = 1
ACCOUNT_ID_MAX = 100_000

# distribution params
BUY_RATIO = 0.52           # slight imbalance
PRICE_DECAY = 0.003        # exponential lambda for |Δprice|
LOGNORM_MU = math.log(8)   # median ~8
LOGNORM_SIGMA = 1.0        # heavy tail
FAR_THRESHOLD = 50        # ticks

ORDER_LIMIT = 0
ORDER_MARKET = 1
ORDER_CANCEL = 2

P_LIMIT = 0.6
P_MARKET = 0.10
P_CANCEL = 0.3

pack = struct.pack

far_pool = defaultdict(list)
near_pool = defaultdict(list)


def sample_price_around_mid(mid: int) -> int:
    # sample side-relative offset using Laplace / double exponential
    # f(x) = 0.5 * λ * exp(-λ|x|)
    u = random.random() - 0.5
    offset = int(math.copysign(math.log1p(-2*abs(u)) / -PRICE_DECAY, u))
    return max(1, mid + offset)


def sample_quantity() -> int:
    # lognormal with heavy right tail
    q = random.lognormvariate(LOGNORM_MU, LOGNORM_SIGMA)
    return max(1, int(round(q)))


def choose_event(has_live):
    r = random.random()
    if not has_live:
        return ORDER_LIMIT if r < P_LIMIT/(P_LIMIT + P_MARKET) else ORDER_MARKET
    if r < P_LIMIT:
        return ORDER_LIMIT
    elif r < P_LIMIT + P_MARKET:
        return ORDER_MARKET
    else:
        return ORDER_CANCEL


def sample_account_id() -> int:
    return random.randint(1, ACCOUNT_ID_MAX)


def generate_orders(bin_path, csv_path):
    live_ids = set()
    next_id = 1
    i = 0
    mid = FAIR_PRICE

    with open(bin_path, "wb") as fbin, open(csv_path, "w", newline="") as fcsv:
        w = csv.writer(fcsv)
        w.writerow(["side", "type", "acct", "price", "quantity", "order_id"])

        for _ in range(N):
            # small random drift of midprice
            # mid += random.choice([-1, 0, 1]) * random.randint(0, 1)

            has_live = bool(live_ids)
            evt = choose_event(has_live)

            if evt == ORDER_LIMIT:
                side = 0 if random.random() < BUY_RATIO else 1
                price = sample_price_around_mid(mid)
                qty = sample_quantity()
                oid = next_id
                account_id = sample_account_id()
                next_id += 1
                live_ids.add(oid)
                delta = abs(price - mid)
                if delta > FAR_THRESHOLD:
                    far_pool[price].append(oid)
                else:
                    near_pool[price].append(oid)

            elif evt == ORDER_MARKET:
                side = 0 if random.random() < BUY_RATIO else 1
                price = 0
                qty = sample_quantity()
                account_id = sample_account_id()
                oid = next_id
                next_id += 1

            else:
                # Order cancel
                if not far_pool and not near_pool:
                    evt = ORDER_LIMIT
                    side = 0 if random.random() < BUY_RATIO else 1
                    price = sample_price_around_mid(mid)
                    qty = sample_quantity()
                    account_id = sample_account_id()
                    oid = next_id
                    next_id += 1
                    live_ids.add(oid)
                    delta = abs(price - mid)
                    if delta > FAR_THRESHOLD:
                        far_pool[price].append(oid)
                    else:
                        near_pool[price].append(oid)

                else:
                    evt = ORDER_CANCEL
                    use_far = random.random() < 0.8
                    if (use_far or not near_pool):
                        pool = far_pool
                    else:
                        pool = near_pool
                    price = random.choice(list(pool.keys()))
                    oid = pool[price].pop()
                    if not pool[price]:
                        del pool[price]
                    if oid in live_ids:
                        live_ids.remove(oid)
                    side, price, qty, account_id = 0, 0, 0, 0

            payload = pack("<BBxxLQQQ", side, evt,
                           account_id, price, qty, oid)
            fbin.write(payload)
            w.writerow([side, evt, account_id, price, qty, oid])
            if i < 5:
                print(side, evt, account_id, price, qty, oid)
            i = i + 1


if __name__ == "__main__":
    random.seed(42)
    generate_orders("client/orders.bin", "client/orders.csv")
    print(f"Generated {N:,} orders around mid={FAIR_PRICE}")
