import socket, struct

side = 0  # Buy
price = 100
quantity = 42

payload = struct.pack(">BQQ", side, price, quantity)
length_prefix = struct.pack(">I", len(payload))
msg = length_prefix + payload

sock = socket.create_connection(("127.0.0.1", 8080))
sock.sendall(msg)
sock.close()