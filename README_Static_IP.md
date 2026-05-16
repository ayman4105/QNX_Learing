# Setting Static IP on QNX at Runtime

> Quick guide for configuring a static IP address on **QNX 8.0** (RPi5) at runtime.

---

## 🎯 Goal

Set a **static IP** for the WiFi interface (`bcm0`) on QNX so it persists across reboots and doesn't change via DHCP.

---

## 🔍 Check Current IP

```bash
ssh root@192.168.1.4

ifconfig bcm0 | grep inet
```

**Example output:**
```
inet 192.168.1.4 netmask 0xffffff00 broadcast 192.168.1.255
```

> Note: QNX 8.0 Non-Commercial does **not** support `reboot` command. Use `shutdown` instead.

---

## ⚙️ Method 1: `/boot/network` (Preferred)

Create a config file in `/boot` to set hostname and IP at boot time.

```bash
# On RPi5 (QNX)
cat > /boot/network << 'EOF'
HOSTNAME=aymanqnx
IP_ADDR=192.168.1.4
EOF
```

Then reboot:
```bash
shutdown
# Reconnect after boot
ssh root@192.168.1.4
```

Verify:
```bash
ifconfig bcm0 | grep inet
# Expected: inet 192.168.1.4
```

> ⚠️ **Note:** This method works on QNX 8.0+ but may not persist on some Non-Commercial builds if `/boot` is not properly mounted or the startup script doesn't read it.

---

## ⚙️ Method 2: Runtime `ifconfig` (Immediate, non-persistent)

If you need to set the IP **right now** without rebooting:

```bash
# On RPi5 (QNX)
ifconfig bcm0 192.168.1.4 netmask 255.255.255.0 up

# Add default gateway (if needed)
route add default 192.168.1.1
```

Verify:
```bash
ifconfig bcm0 | grep inet
```

> ⚠️ **This resets after reboot.** Use Method 1 or 3 for persistence.

---

## ⚙️ Method 3: `rc.local` / Startup Script (Persistent)

Create a startup script that runs at boot to set the IP.

```bash
# On RPi5 (QNX)
mkdir -p /data/var/etc/rc.d

cat > /data/var/etc/rc.d/rc.local << 'EOF'
#!/bin/sh
# Static IP for WiFi (bcm0)
ifconfig bcm0 192.168.1.4 netmask 255.255.255.0 up
route add default 192.168.1.1
EOF

chmod +x /data/var/etc/rc.d/rc.local
```

Hook it into the startup sequence:
```bash
cat >> /system/etc/startup/post_startup.sh << 'EOF'

# Run custom rc.local if exists
if [ -f /data/var/etc/rc.d/rc.local ]; then
    . /data/var/etc/rc.d/rc.local
fi
EOF
```

Then reboot:
```bash
shutdown
```

---

## 📊 Comparison

| Method | Persistent? | Reboot Required? | Best For |
|--------|------------|-------------------|----------|
| `/boot/network` | ✅ Yes | ✅ Yes | Clean, official approach |
| `ifconfig` runtime | ❌ No | ❌ No | Quick testing |
| `rc.local` | ✅ Yes | ✅ Yes | Fallback when `/boot/network` fails |
| **Router DHCP Reservation** | ✅ Yes | ❌ No | **Best long-term solution** |

---

## 💡 Best Practice: Router DHCP Reservation

The most reliable way is to configure your **WiFi router** to reserve the IP by MAC address:

1. Get the MAC address of `bcm0`:
   ```bash
   ifconfig bcm0 | grep ether
   # Example: ether 88:a2:9e:00:c7:cc
   ```

2. Open your router admin page (usually `http://192.168.1.1`)

3. Go to **DHCP Reservation / Static Lease**

4. Add a reservation:
   - **MAC:** `88:a2:9e:00:c7:cc`
   - **IP:** `192.168.1.4` (or `192.168.1.3` if you prefer)
   - **Hostname:** `aymanqnx`

5. Save and reboot the RPi5

> This ensures the RPi5 always gets the same IP without any QNX-side configuration.

---

## ✅ Verification Checklist

After setting static IP:

```bash
# 1. Check IP is correct
ifconfig bcm0 | grep inet

# 2. Check connectivity to Lab PC
ping 192.168.1.5

# 3. Check gateway
route -n show

# 4. Verify from Lab PC
ssh root@192.168.1.4
```

---

## 📝 Update vsomeip Config After IP Change

If the RPi5 IP changes, update the client config:

```bash
# On RPi5
cat > /tmp/vsomeip-client.json << 'EOF'
{
    "unicast": "192.168.1.4",
    "netmask": "255.255.255.0",
    ...
}
EOF
```

---

## 🚀 Quick Reference

| Task | Command |
|------|---------|
| Check IP | `ifconfig bcm0 \| grep inet` |
| Set IP now | `ifconfig bcm0 192.168.1.4 netmask 255.255.255.0 up` |
| Add gateway | `route add default 192.168.1.1` |
| Check MAC | `ifconfig bcm0 \| grep ether` |
| Reboot QNX | `shutdown` |
| Test connectivity | `ping 192.168.1.5` |

---

> **Tip:** Always verify the IP after reboot before starting the CommonAPI client, or the vsomeip config will be wrong!
