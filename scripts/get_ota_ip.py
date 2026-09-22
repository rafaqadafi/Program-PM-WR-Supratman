Import("env")
import re

try:
    with open("src/secret.h", "r") as f:
        content = f.read()
        static_flag = re.search(r'USE_STATIC_IP\s*=\s*(true|false)', content)
        if static_flag and static_flag.group(1) == "true":
            match = re.search(r'STATIC_IP\[\]\s*=\s*"([^"]+)"', content)
            if match:
                ip = match.group(1)
                env.Replace(UPLOAD_PORT=ip)
                print(f"\n[OTA-CONFIG] IP Statis: {ip}\n")
        else:
            match_host = re.search(r'OTA_HOSTNAME\[\]\s*=\s*"([^"]+)"', content)
            hostname = (match_host.group(1) if match_host else "PM-WR-Supratman") + ".local"
            env.Replace(UPLOAD_PORT=hostname)
            print(f"\n[OTA-CONFIG] DHCP aktif -> upload_port mDNS: {hostname}\n")
except Exception as e:
    print(f"\n[OTA-CONFIG] Gagal membaca src/secret.h: {e}\n")
