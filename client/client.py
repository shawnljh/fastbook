import socket
import time

FILENAME = "client/orders.bin"
SERVER = ("127.0.0.1", 8080)

with open(FILENAME, "rb") as f:
    data = f.read()

sock = socket.create_connection(SERVER)
send = sock.sendall

t0 = time.time()
send(data)
t1 = time.time()

elapsed = t1 - t0
N = len(data) // 25  # approximate
print(f"Replayed {N:,} orders in {elapsed:.3f}s → {N/elapsed:,.0f} orders/sec")

sock.close()
