import struct
import random
import math
import csv

SEED = 42
random.seed(SEED)

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

pack = struct.pack
BIN_PATH = "client/orders.bin"
CSV_PATH = "client/orders.csv"


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


def sample_account_id() -> int:
    return random.randint(1, ACCOUNT_ID_MAX)


mid = FAIR_PRICE
with open(BIN_PATH, "wb") as fbin, open(CSV_PATH, "w", newline="") as fcsv:
    w = csv.writer(fcsv)
    w.writerow(["side", "price", "quantity"])

    for _ in range(N):
        # small random drift of midprice
        mid += random.choice([-1, 0, 1]) * random.randint(0, 1)

        side = 0 if random.random() < BUY_RATIO else 1
        price = sample_price_around_mid(mid)
        qty = sample_quantity()
        account_id = sample_account_id()

        payload = pack(">BQQQ", side, price, qty, account_id)
        msg = pack(">I", len(payload)) + payload
        fbin.write(msg)
        w.writerow([side, price, qty])


print(f"Generated {N:,} orders around mid={FAIR_PRICE}")
