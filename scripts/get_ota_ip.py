Import("env")
import re

try:
    with open("src/secret.h", "r") as f:
        match = re.search(r'STATIC_IP\[\]\s*=\s*"([^"]+)"', f.read())
        if match:
            ip = match.group(1)
            env.Replace(UPLOAD_PORT=ip)
            print(f"\n[OTA-CONFIG] upload_port otomatis diambil dari src/secret.h: {ip}\n")
except Exception as e:
    print(f"\n[OTA-CONFIG] Gagal membaca src/secret.h: {e}\n")
