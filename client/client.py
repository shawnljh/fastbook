import socket
import time

FILENAME = "client/orders.bin"


def wait_for_server(host='127.0.0.1', port=8080):
    print(f"Waiting for server on port {port}...")
    while True:
        try:
            sock = socket.create_connection((host, port))
            print("server online")
            return sock
        except (ConnectionRefusedError, TimeoutError, OSError):
            time.sleep(0.025)


def fire_orders(sock):
    with open(FILENAME, "rb") as f:
        data = f.read()

    send = sock.sendall

    t0 = time.time()
    send(data)
    t1 = time.time()

    elapsed = t1 - t0
    N = len(data) // 32  # approximate
    print(f"Replayed {N:,} orders in {
          elapsed:.3f}s → {N/elapsed:,.0f} orders/sec")

    sock.close()


if __name__ == "__main__":
    sock = wait_for_server()
    fire_orders(sock)
