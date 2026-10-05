import socket, time
import sys
port = int(sys.argv[1]) if len(sys.argv) > 1 else 27036
s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
try:
    s.bind(("0.0.0.0", port))
except OSError as e:
    print(f"bind {port} failed: {e}", flush=True)
    sys.exit(1)
s.settimeout(2.0)
print(f"sniffing UDP {port} for 10s...", flush=True)
deadline = time.time() + 10
try:
    while time.time() < deadline:
        try:
            data, addr = s.recvfrom(4096)
            print(f"PACKET from {addr} len={len(data)} head={data[:24].hex()}", flush=True)
        except socket.timeout:
            print("timeout tick", flush=True)
except Exception as e:
    print("ERR", e, flush=True)
