# CommonAPI + SOME/IP on QNX & Linux

> **Cross-platform IPC project**: Linux x86_64 Server (Lab PC) ↔ QNX ARM64 Client (RPi5) using **CommonAPI C++** with **SOME/IP** binding over WiFi.

---

## 📡 System Architecture

```
┌─────────────────────┐                      ┌─────────────────────┐
│   🖥️ Lab PC (Linux)  │◄──── WiFi ────►│   📱 RPi5 (QNX 8.0)  │
│   IP: 192.168.1.5   │   SOME/IP UDP    │   IP: 192.168.1.4   │
│   CommonAPI Server  │   Multicast SD   │   CommonAPI Client  │
└─────────────────────┘                      └─────────────────────┘
```

---

## 📁 Project Structure (After Cleanup)

```
test_common_api/
├── Config/
│   ├── vsomeip-server.json          # Server vsomeip config (unicast: 192.168.1.5)
│   └── vsomeip-client.json          # Client vsomeip config (unicast: 192.168.1.4)
├── fidl/
│   ├── HelloWorld.fidl              # Interface definition
│   ├── HelloWorld.fdepl             # SOME/IP deployment spec
│   └── src-gen/v1/commonapi/        # Generated code (Proxy, StubAdapter, Deployment)
│       ├── HelloWorldSomeIPDeployment.cpp
│       ├── HelloWorldSomeIPProxy.cpp
│       └── HelloWorldSomeIPStubAdapter.cpp
├── SRC/
│   ├── Server.cpp                   # Linux Server main
│   ├── Client.cpp                   # QNX Client main
│   └── HelloWorldStubImpl.hpp       # Service implementation
├── build-qnx-client/
│   ├── CMakeLists.txt               # QNX cross-compile (manual linking)
│   └── eventfd_stub.c               # QNX eventfd workaround (pipe-based)
├── build-qnx/                       # QNX build output (ARM64 binaries)
├── build-server-linux/              # Linux build output (x86_64 binaries)
├── CMakeLists.client.txt            # Linux client CMake (reference)
├── CMakeLists.server.txt            # Linux server CMake (reference)
├── CMakeLists.txt                   # Top-level CMake
├── run_server.sh                    # Wrapper script for Linux Server
└── .vscode/                         # VS Code settings
```

---

## 📝 Interface Definition (FIDL)

**`fidl/HelloWorld.fidl`**
```fidl
package commonapi

interface HelloWorld {
    version { major 1 minor 0 }
    method sayHello {
        in { String name }
        out { String message }
    }
}
```

**`fidl/HelloWorld.fdepl`** — SOME/IP deployment spec:
```fdepl
import "platform:/plugin/org.genivi.commonapi.someip/deployment/CommonAPI-4-SOMEIP_deployment_spec.fdepl"
import "HelloWorld.fidl"

define org.genivi.commonapi.someip.deployment for interface commonapi.HelloWorld {
    SomeIpServiceID = 4660
    method sayHello {
        SomeIpMethodID = 30000
        SomeIpReliable = false
    }
}

define org.genivi.commonapi.someip.deployment for provider as Service {
    instance commonapi.HelloWorld {
        InstanceId = "commonapi.HelloWorld"
        SomeIpInstanceID = 22136
        SomeIpUnicastAddress = "192.168.1.5"
        SomeIpReliableUnicastPort = 30490
        SomeIpUnreliableUnicastPort = 30490
    }
}
```

---

## 💻 Source Code

### Server (`SRC/Server.cpp`)
```cpp
#include <iostream>
#include <thread>
#include <CommonAPI/CommonAPI.hpp>
#include "HelloWorldStubImpl.hpp"

using namespace v1::commonapi;

int main() {
    auto runtime = CommonAPI::Runtime::get();
    auto myService = std::make_shared<HelloWorldStubImpl>();

    runtime->registerService("local", "commonapi.HelloWorld", myService, "HelloWorldService");

    std::cout << "HelloWorld Service registered on Lab PC!" << std::endl;
    std::cout << "Waiting for clients..." << std::endl;

    while (true) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
    return 0;
}
```

### Client (`SRC/Client.cpp`)
```cpp
#include <iostream>
#include <thread>
#include <chrono>
#include <CommonAPI/CommonAPI.hpp>
#include "v1/commonapi/HelloWorldProxy.hpp"

using namespace v1::commonapi;

int main() {
    auto runtime = CommonAPI::Runtime::get();
    auto myProxy = runtime->buildProxy<HelloWorldProxy>("local", "commonapi.HelloWorld", "HelloWorldClient");

    if (!myProxy) {
        std::cerr << "Failed to create proxy!" << std::endl;
        return 1;
    }

    std::cout << "QNX Client: Waiting for service..." << std::endl;

    int timeout = 100;
    while (!myProxy->isAvailable() && timeout > 0) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        timeout--;
    }

    if (!myProxy->isAvailable()) {
        std::cerr << "Service not available after timeout!" << std::endl;
        return 1;
    }

    std::cout << "Service available! Starting periodic calls..." << std::endl;

    int counter = 0;
    while (true) {
        CommonAPI::CallStatus callStatus;
        std::string returnMessage;
        std::string name = "QNX_RPi5_Ayman!!" + std::to_string(counter);

        myProxy->sayHello(name, callStatus, returnMessage);

        if (callStatus == CommonAPI::CallStatus::SUCCESS) {
            std::cout << "[" << counter << "] Got message: '" << returnMessage << "'" << std::endl;
        } else {
            std::cout << "[" << counter << "] Call failed: " << (int)callStatus << std::endl;
        }

        counter++;
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
    return 0;
}
```

---

## 🏗️ Build Instructions

### 1. Linux Server (Lab PC)

```bash
# Use the server CMakeLists
cp CMakeLists.server.txt /tmp/server-build-src/CMakeLists.txt

cmake -B build-server-linux -S /tmp/server-build-src -DCMAKE_BUILD_TYPE=Release
cmake --build build-server-linux -j$(nproc)
```

### 2. QNX Client (RPi5) — Cross-Compilation

```bash
# Source QNX SDP environment
source ~/ITI/QNX/repo/qnx800/qnxsdp-env.sh

# Build with QNX toolchain
cmake -B build-qnx       -DCMAKE_TOOLCHAIN_FILE=/home/ayman/ITI/QNX/repo/src/qnx_aarch64_toolchain.cmake       -DCMAKE_BUILD_TYPE=Release       -S build-qnx-client

cmake --build build-qnx -j$(nproc)
```

> **Note**: `build-qnx-client/CMakeLists.txt` uses **manual linking** (absolute `.so` paths) because `find_package()` does not work correctly on QNX SDP.

---

## 🔧 QNX-Specific Fixes

### Fix 1: `eventfd` Missing on QNX
QNX does not implement `eventfd()`. We provide a **pipe-based stub**:

**`build-qnx-client/eventfd_stub.c`**
```c
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <stdint.h>

#define EFD_NONBLOCK  O_NONBLOCK
#define EFD_CLOEXEC   O_CLOEXEC
#define EFD_SEMAPHORE 1

int eventfd(unsigned int initval, int flags) {
    int fds[2];
    if (pipe(fds) < 0) return -1;
    if (flags & EFD_NONBLOCK) {
        fcntl(fds[0], F_SETFL, O_NONBLOCK);
        fcntl(fds[1], F_SETFL, O_NONBLOCK);
    }
    if (flags & EFD_CLOEXEC) {
        fcntl(fds[0], F_SETFD, FD_CLOEXEC);
        fcntl(fds[1], F_SETFD, FD_CLOEXEC);
    }
    close(fds[1]);
    return fds[0];
}
```

### Fix 2: Compile Definitions for QNX
```cmake
target_compile_definitions(HelloWorldClient PRIVATE
    _QNX_SOURCE
    SA_RESTART=0
    __unix__
    __LITTLE_ENDIAN=1234
    __BIG_ENDIAN=4321
    __BYTE_ORDER=1234
)
```

### Fix 3: Manual Library Linking
```cmake
target_link_libraries(HelloWorldClient PRIVATE
    ${LIB_DIR}/libCommonAPI.so
    ${LIB_DIR}/libCommonAPI-SomeIP.so
    ${LIB_DIR}/libvsomeip3.so
    ${LIB_DIR}/libboost_thread.so
    ${LIB_DIR}/libboost_system.so
    -L${LIB_DIR}
    -Wl,-rpath-link,${LIB_DIR}
)
```

---

## 📦 Deploy to RPi5

### Transfer binaries & config
```bash
# Client binary
scp build-qnx/HelloWorldClient root@192.168.1.4:/tmp/

# vsomeip config
scp Config/vsomeip-client.json root@192.168.1.4:/tmp/

# Boost libraries (if missing on target)
scp ~/ITI/QNX/repo/src/stage/usr/lib/libboost_*.so.1.82.0 root@192.168.1.4:/usr/usr/lib/
```

### One-time RPi5 setup
```bash
# SSH into RPi5
ssh root@192.168.1.4

# Create symlinks for CommonAPI libraries
ln -sf /usr/usr/lib/libCommonAPI.so.3      /usr/usr/lib/libCommonAPI.so.3.2.4
ln -sf /usr/usr/lib/libCommonAPI-SomeIP.so.3 /usr/usr/lib/libCommonAPI-SomeIP.so.3.2.4

# Create vsomeip socket directory
mkdir -p /var/vsomeip
chmod 777 /var/vsomeip

# CommonAPI runtime config
cat > /etc/commonapi.ini << 'EOF'
[default]
binding=someip
libpath=/usr/usr/lib
EOF
```

---

## ⚙️ Configuration Files

### Server (`Config/vsomeip-server.json`)
```json
{
    "unicast": "192.168.1.5",
    "netmask": "255.255.255.0",
    "device": "wlp111s0",
    "logging": { "level": "debug", "console": true, "file": { "enable": false } },
    "applications": [ { "name": "HelloWorldService", "id": "0x1234" } ],
    "services": [ { "service": "0x1234", "instance": "0x5678", "unreliable": "30490" } ],
    "routing": "HelloWorldService",
    "service-discovery": {
        "enable": true, "multicast": "224.224.224.245", "port": "30490",
        "protocol": "udp", "initial_delay_min": "10", "initial_delay_max": "100",
        "repetitions_base_delay": "200", "repetitions_max": "3", "ttl": "3",
        "cyclic_offer_delay": "2000", "request_response_delay": "1500"
    }
}
```

### Client (`/tmp/vsomeip-client.json` on RPi5)
```json
{
    "unicast": "192.168.1.4",
    "netmask": "255.255.255.0",
    "logging": { "level": "debug", "console": true, "file": { "enable": false } },
    "applications": [ { "name": "HelloWorldClient", "id": "0x1235" } ],
    "routing": "HelloWorldClient",
    "service-discovery": {
        "enable": true, "multicast": "224.224.224.245", "port": "30490",
        "protocol": "udp", "initial_delay_min": "10", "initial_delay_max": "100",
        "repetitions_base_delay": "200", "repetitions_max": "3", "ttl": "3",
        "cyclic_offer_delay": "2000", "request_response_delay": "1500"
    }
}
```

---

## 🚀 Running the System

### Option A: Manual (with exports)

**Lab PC (Server):**
```bash
cd ~/ITI/QNX/test_common_api
export VSOMEIP_CONFIGURATION=$PWD/Config/vsomeip-server.json
export VSOMEIP_APPLICATION_NAME=HelloWorldService
./build-server-linux/HelloWorldServer
```

**RPi5 (Client):**
```bash
export VSOMEIP_CONFIGURATION=/tmp/vsomeip-client.json
export LD_LIBRARY_PATH=/usr/usr/lib
export VSOMEIP_APPLICATION_NAME=HelloWorldClient
/tmp/HelloWorldClient
```

### Option B: Wrapper Scripts (Recommended)

**Lab PC — `run_server.sh`:**
```bash
#!/bin/bash
export VSOMEIP_CONFIGURATION=$PWD/Config/vsomeip-server.json
export VSOMEIP_APPLICATION_NAME=HelloWorldService
$PWD/build-server-linux/HelloWorldServer
```

**RPi5 — `/tmp/run_client.sh`:**
```bash
#!/bin/sh
export VSOMEIP_CONFIGURATION=/tmp/vsomeip-client.json
export LD_LIBRARY_PATH=/usr/usr/lib
export VSOMEIP_APPLICATION_NAME=HelloWorldClient
/tmp/HelloWorldClient
```

Then simply run:
```bash
./run_server.sh        # On Lab PC
/tmp/run_client.sh     # On RPi5
```

---

## 🐛 Errors & Fixes Summary

| # | Error | Cause | Fix |
|---|-------|-------|-----|
| 1 | `find_package` fails on QNX | QNX SDP missing CMake config modules | Manual linking with absolute `.so` paths |
| 2 | `uid_t` / `gid_t` conflict | QNX headers vs Linux headers | Add `-D__unix__` compile definition |
| 3 | `__BYTE_ORDER` not declared | Missing endianness macros on QNX | Add `__LITTLE_ENDIAN`, `__BIG_ENDIAN`, `__BYTE_ORDER` defines |
| 4 | `eventfd` not found | QNX has no `eventfd` syscall | Created `eventfd_stub.c` using `pipe()` |
| 5 | `libboost_*` not found | Boost libs not deployed to RPi5 | `scp` boost `.so` files to `/usr/usr/lib/` |
| 6 | `libCommonAPI.so.3.2.4` not found | Versioned symlink missing | Created symlinks on RPi5 |
| 7 | `/var/vsomeip-0` not found | vsomeip runtime directory missing | `mkdir -p /var/vsomeip` |
| 8 | App name empty in vsomeip logs | Missing app name in proxy/stub ctor | Pass `"HelloWorldService"` / `"HelloWorldClient"` to `registerService()` / `buildProxy()` |
| 9 | Server routes via `127.0.0.1` | vsomeip not binding to correct interface | Added `"device": "wlp111s0"` to server JSON |
| 10 | `Didn't receive a multicast SD message` | WiFi router filtering multicast | **Normal warning** — unicast RPC still works |

---

## 🎯 Current Status

| Component | Status |
|-----------|--------|
| ✅ QNX Client build | Successful |
| ✅ Linux Server build | Successful |
| ✅ Client runs on RPi5 | Finds routing, connects |
| ✅ Server registers service | On `192.168.1.5` |
| ✅ RPC Communication | `sayHello()` works both ways |
| ✅ Cross-platform IPC | Linux ↔ QNX over WiFi |

---

## 📚 Useful Commands

```bash
# Check IPs
ip -4 addr show | grep inet        # Lab PC
ifconfig bcm0 | grep inet          # RPi5 QNX

# Check library dependencies on QNX
ldd /tmp/HelloWorldClient

# Kill stuck client on QNX
slay HelloWorldClient
# or
killall HelloWorldClient

# Disable firewall on Lab PC (if needed)
sudo ufw disable
sudo iptables -F
```

---

## 🔗 References

- [GENIVI CommonAPI C++](https://github.com/GENIVI/capicxx-core-runtime)
- [GENIVI CommonAPI SOME/IP](https://github.com/GENIVI/capicxx-someip-runtime)
- [COVESA vsomeip](https://github.com/COVESA/vsomeip)
- [QNX SDP 8.0 Documentation](https://www.qnx.com/developers/docs/)

---

> **Author:** Ayman  
> **Platform:** Linux x86_64 ↔ QNX 8.0 (ARM64 / RPi5)  
> **Protocol:** SOME/IP over UDP (WiFi)  
> **Binding:** CommonAPI C++ 3.2 + vsomeip 3.5.5
