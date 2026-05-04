# Creating a Power Switch Driver/Module in Zephyr RTOS for STM32F401CC

A complete, beginner-friendly guide to building a custom GPIO-based power switch driver in Zephyr — packaged as a reusable out-of-tree module.

---

## Table of Contents

1. [Essential Zephyr Concepts](#part-1-essential-zephyr-concepts)
2. [Driver vs. Module — What's the Difference?](#part-2-driver-vs-module)
3. [Project Structure](#part-3-project-structure)
4. [Purpose of Each File](#part-4-purpose-of-each-file)
5. [Complete File Contents](#part-5-complete-file-contents)
6. [How Functions Connect (Diagrams)](#part-6-how-functions-connect)
7. [Why `static inline` in Public Headers?](#part-7-why-static-inline-in-headers)
8. [Build & Flash Steps](#part-8-build--flash-steps)
9. [Common Pitfalls (Lessons Learned)](#part-9-common-pitfalls)

---

## PART 1: Essential Zephyr Concepts

Zephyr's architecture is built on **three pillars**:

| Pillar | What it does |
|--------|-------------|
| **Devicetree (DTS)** | Describes hardware declaratively (separate from code) |
| **Kconfig** | Configures which features/drivers are compiled |
| **Device Driver Model** | Provides hardware-agnostic API for applications |

### Devicetree File Types

| Extension | Role |
|-----------|------|
| `.dts` | Main board hardware description |
| `.dtsi` | Shared/included DT fragment (like a header) |
| `.overlay` | Application-specific additions or modifications |
| `.yaml` (binding) | Schema validating DTS nodes and properties |

### Kconfig Files

| File | Role |
|------|------|
| `Kconfig` | Defines build-time options |
| `prj.conf` | Application's selected option values |

### Build System

- **CMake**: Zephyr's build orchestrator
- **CMakeLists.txt**: Tells CMake which sources to compile

### Device Driver Model

- Each driver implements an **API struct** (function pointers).
- Drivers are instantiated from devicetree nodes via `DEVICE_DT_INST_DEFINE()`.
- Applications get a `const struct device *` handle and call API functions.

---

## PART 2: Driver vs. Module

Your `power_switch` is **BOTH**:
- A **driver** (the code itself).
- Packaged as a **module** (the way it's distributed).

> **Mental model: A module is the *container*. A driver is the *content*.**

### Driver

| Property | Description |
|----------|-------------|
| Purpose | Abstracts hardware behind a uniform API |
| Location | Lives under a `drivers/` directory |
| Built using | `DEVICE_DT_INST_DEFINE()` macro |
| Tied to | A devicetree node (via `compatible` string) |
| Examples | GPIO, I2C, SPI, UART, sensors, **power_switch** |

### Module

| Property | Description |
|----------|-------------|
| Purpose | Add external code to a Zephyr build |
| Identified by | A `zephyr/module.yml` file |
| Can contain | Drivers, libraries, subsystems, samples |
| Examples | `hal_stm32`, `mcuboot`, `lvgl`, **your `power_switch` folder** |

### Comparison

| Aspect | Driver | Module |
|--------|--------|--------|
| What it is | Hardware control code | Packaging unit |
| Required files | `.c`, `.yaml` binding, Kconfig | `module.yml`, `CMakeLists.txt`, `Kconfig` |
| Integration | `DEVICE_DT_INST_DEFINE` + DT compatible | `west.yml` or `ZEPHYR_EXTRA_MODULES` |
| Analogy | A book chapter | A book |

---

## PART 3: Project Structure

```
power_switch_project/
├── CMakeLists.txt                    # App build file
├── prj.conf                          # App config
├── boards/
│   └── blackpill_f401cc.overlay      # Board-specific DT changes
├── src/
│   └── main.c                        # Application code
└── modules/
    └── power_switch/                 # Your custom module
        ├── zephyr/
        │   └── module.yml            # Tells Zephyr this is a module
        ├── CMakeLists.txt            # Module build entry
        ├── Kconfig                   # Module config entry
        ├── drivers/
        │   └── power_switch/
        │       ├── CMakeLists.txt    # Driver sources
        │       ├── Kconfig           # Driver options
        │       └── power_switch.c    # Driver implementation
        ├── dts/
        │   └── bindings/
        │       └── power-switch.yaml # Devicetree binding
        └── include/
            └── drivers/
                └── power_switch.h    # Public driver API
```

---

## PART 4: Purpose of Each File

| File | Purpose |
|------|---------|
| `module.yml` | Declares this directory as a Zephyr module |
| `modules/power_switch/CMakeLists.txt` | Top-level module CMake; enters `drivers/` |
| `modules/power_switch/Kconfig` | Top-level module Kconfig; sources driver Kconfig |
| `drivers/power_switch/CMakeLists.txt` | Compiles `power_switch.c` |
| `drivers/power_switch/Kconfig` | Defines `CONFIG_POWER_SWITCH` option |
| `drivers/power_switch/power_switch.c` | Actual driver logic |
| `dts/bindings/power-switch.yaml` | Schema for `compatible = "power-switch"` |
| `include/drivers/power_switch.h` | Public API header |
| `boards/<board>.overlay` | Instantiates a `power_switch` node on a GPIO pin |
| `prj.conf` | Enables `CONFIG_GPIO=y` and `CONFIG_POWER_SWITCH=y` |
| `src/main.c` | Application using your driver |

---

## PART 5: Complete File Contents

### 5.1 `modules/power_switch/zephyr/module.yml`

```yaml
name: power_switch
build:
  cmake: .
  kconfig: Kconfig
  settings:
    dts_root: .
```

### 5.2 `modules/power_switch/CMakeLists.txt` (top-level)

```cmake
zephyr_include_directories(include)

add_subdirectory_ifdef(CONFIG_POWER_SWITCH drivers/power_switch)
```

### 5.3 `modules/power_switch/Kconfig` (top-level)

```kconfig
rsource "drivers/power_switch/Kconfig"
```

### 5.4 `modules/power_switch/drivers/power_switch/CMakeLists.txt`

```cmake
zephyr_library_named(power_switch_driver)
zephyr_library_sources(power_switch.c)
```

> ⚠️ Path is **relative to this CMakeLists.txt**, so just `power_switch.c` (not `drivers/power_switch/power_switch.c`).

### 5.5 `modules/power_switch/drivers/power_switch/Kconfig`

```kconfig
config POWER_SWITCH
    bool "Power Switch GPIO driver"
    default y
    depends on GPIO
    depends on DT_HAS_POWER_SWITCH_ENABLED
    help
      Enable the GPIO-based power switch driver.
```

### 5.6 `modules/power_switch/dts/bindings/power-switch.yaml`

```yaml
description: GPIO-controlled power switch

compatible: "power-switch"

include: base.yaml

properties:
  gpios:
    type: phandle-array
    required: true
    description: GPIO pin controlling the power switch.

  startup-delay-ms:
    type: int
    default: 0
    description: Delay after power-on in milliseconds.
```

### 5.7 `modules/power_switch/include/drivers/power_switch.h`

```c
#ifndef ZEPHYR_INCLUDE_DRIVERS_POWER_SWITCH_H_
#define ZEPHYR_INCLUDE_DRIVERS_POWER_SWITCH_H_

#include <zephyr/device.h>
#include <zephyr/kernel.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef int (*power_switch_on_t)(const struct device *dev);
typedef int (*power_switch_off_t)(const struct device *dev);
typedef int (*power_switch_get_state_t)(const struct device *dev, bool *state);

__subsystem struct power_switch_driver_api {
    power_switch_on_t on;
    power_switch_off_t off;
    power_switch_get_state_t get_state;
};

static inline int power_switch_on(const struct device *dev)
{
    const struct power_switch_driver_api *api = dev->api;
    return api->on(dev);
}

static inline int power_switch_off(const struct device *dev)
{
    const struct power_switch_driver_api *api = dev->api;
    return api->off(dev);
}

static inline int power_switch_get_state(const struct device *dev, bool *state)
{
    const struct power_switch_driver_api *api = dev->api;
    return api->get_state(dev, state);
}

#ifdef __cplusplus
}
#endif

#endif
```

### 5.8 `modules/power_switch/drivers/power_switch/power_switch.c`

```c
#define DT_DRV_COMPAT power_switch

#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/logging/log.h>
#include <drivers/power_switch.h>

LOG_MODULE_REGISTER(power_switch, CONFIG_LOG_DEFAULT_LEVEL);

struct power_switch_config {
    struct gpio_dt_spec gpio;
    uint32_t startup_delay_ms;
};

struct power_switch_data {
    bool state;
};

static int power_switch_api_on(const struct device *dev)
{
    const struct power_switch_config *cfg = dev->config;
    struct power_switch_data *data = dev->data;
    int ret;

    ret = gpio_pin_set_dt(&cfg->gpio, 1);
    if (ret < 0) {
        LOG_ERR("Failed to set GPIO high: %d", ret);
        return ret;
    }

    if (cfg->startup_delay_ms) {
        k_msleep(cfg->startup_delay_ms);
    }

    data->state = true;
    LOG_INF("Power switch ON");
    return 0;
}

static int power_switch_api_off(const struct device *dev)
{
    const struct power_switch_config *cfg = dev->config;
    struct power_switch_data *data = dev->data;
    int ret;

    ret = gpio_pin_set_dt(&cfg->gpio, 0);
    if (ret < 0) {
        LOG_ERR("Failed to set GPIO low: %d", ret);
        return ret;
    }

    data->state = false;
    LOG_INF("Power switch OFF");
    return 0;
}

static int power_switch_api_get_state(const struct device *dev, bool *state)
{
    struct power_switch_data *data = dev->data;
    *state = data->state;
    return 0;
}

static const struct power_switch_driver_api power_switch_api = {
    .on = power_switch_api_on,
    .off = power_switch_api_off,
    .get_state = power_switch_api_get_state,
};

static int power_switch_init(const struct device *dev)
{
    const struct power_switch_config *cfg = dev->config;
    int ret;

    if (!gpio_is_ready_dt(&cfg->gpio)) {
        LOG_ERR("GPIO device not ready");
        return -ENODEV;
    }

    ret = gpio_pin_configure_dt(&cfg->gpio, GPIO_OUTPUT_INACTIVE);
    if (ret < 0) {
        LOG_ERR("Failed to configure GPIO: %d", ret);
        return ret;
    }

    LOG_INF("Power switch initialized");
    return 0;
}

#define POWER_SWITCH_INIT(inst)                                        \
    static struct power_switch_data power_switch_data_##inst;          \
                                                                       \
    static const struct power_switch_config power_switch_cfg_##inst = {\
        .gpio = GPIO_DT_SPEC_INST_GET(inst, gpios),                    \
        .startup_delay_ms = DT_INST_PROP(inst, startup_delay_ms),      \
    };                                                                 \
                                                                       \
    DEVICE_DT_INST_DEFINE(inst,                                        \
                          power_switch_init,                           \
                          NULL,                                        \
                          &power_switch_data_##inst,                   \
                          &power_switch_cfg_##inst,                    \
                          POST_KERNEL,                                 \
                          CONFIG_KERNEL_INIT_PRIORITY_DEVICE,          \
                          &power_switch_api);

DT_INST_FOREACH_STATUS_OKAY(POWER_SWITCH_INIT)
```

### 5.9 `boards/blackpill_f401cc.overlay`

```dts
/ {
    pwr_sw1: power_switch_1 {
        compatible = "power-switch";
        status = "okay";
        gpios = <&gpioa 5 GPIO_ACTIVE_HIGH>;
        startup-delay-ms = <10>;
    };
};
```

> ⚠️ **`pwr_sw1:`** before the node name is the **devicetree node label**, required for `DT_NODELABEL(pwr_sw1)` in the application.

### 5.10 `prj.conf`

```ini
CONFIG_GPIO=y
CONFIG_POWER_SWITCH=y
CONFIG_LOG=y
CONFIG_PRINTK=y
```

### 5.11 `CMakeLists.txt` (application root)

```cmake
cmake_minimum_required(VERSION 3.20.0)

list(APPEND ZEPHYR_EXTRA_MODULES
  ${CMAKE_CURRENT_SOURCE_DIR}/modules/power_switch
)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(power_switch_app)

target_sources(app PRIVATE src/main.c)
```

### 5.12 `src/main.c`

```c
#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <drivers/power_switch.h>

#define PWR_SW_NODE DT_NODELABEL(pwr_sw1)

int main(void)
{
    const struct device *pwr = DEVICE_DT_GET(PWR_SW_NODE);

    if (!device_is_ready(pwr)) {
        printk("Power switch not ready!\n");
        return -1;
    }

    while (1) {
        power_switch_on(pwr);
        k_sleep(K_SECONDS(2));
        power_switch_off(pwr);
        k_sleep(K_SECONDS(2));
    }
    return 0;
}
```

---

## PART 6: How Functions Connect

### 6.1 Layered Abstraction Model

```
┌─────────────────────────────────────────────────────────────────┐
│  LAYER 5: Application (main.c)                                  │
│  - Calls: power_switch_on(dev)                                  │
└──────────────────────────┬──────────────────────────────────────┘
                           │
┌──────────────────────────▼──────────────────────────────────────┐
│  LAYER 4: Driver Public API (power_switch.h)                    │
│  - static inline wrappers (zero overhead)                       │
└──────────────────────────┬──────────────────────────────────────┘
                           │
┌──────────────────────────▼──────────────────────────────────────┐
│  LAYER 3: Driver API Vtable (power_switch_driver_api)           │
│  - Function pointer table → enables polymorphism                │
└──────────────────────────┬──────────────────────────────────────┘
                           │
┌──────────────────────────▼──────────────────────────────────────┐
│  LAYER 2: Driver Implementation (power_switch.c)                │
│  - Real on/off/get_state functions                              │
└──────────────────────────┬──────────────────────────────────────┘
                           │
┌──────────────────────────▼──────────────────────────────────────┐
│  LAYER 1: GPIO Subsystem                                        │
│  - Generic API → SoC-specific driver                            │
└──────────────────────────┬──────────────────────────────────────┘
                           │
┌──────────────────────────▼──────────────────────────────────────┐
│  LAYER 0: STM32 GPIO + HAL → Hardware Registers                 │
└─────────────────────────────────────────────────────────────────┘
```

### 6.2 Boot-Time Flow

```
Zephyr Kernel Init
      │
      ▼
Iterates POST_KERNEL devices
      │
      ▼
power_switch_init(dev)   ◄── registered by DEVICE_DT_INST_DEFINE
      │
      ├── gpio_is_ready_dt(&cfg->gpio)
      └── gpio_pin_configure_dt(&cfg->gpio, GPIO_OUTPUT_INACTIVE)
```

### 6.3 Runtime Call Chain

```
APPLICATION (main.c)
   │
   │  power_switch_on(pwr);
   ▼
PUBLIC API (power_switch.h) — inlined at compile time
   │
   │  return api->on(dev);
   ▼
VTABLE (power_switch.c) — function pointer dispatch
   │
   │  .on = power_switch_api_on
   ▼
IMPLEMENTATION (power_switch.c)
   │
   │  gpio_pin_set_dt(&cfg->gpio, 1);
   ▼
GPIO SUBSYSTEM → STM32 HAL → GPIOA->BSRR
   │
   ▼
PHYSICAL PIN (PA5 = HIGH)
```

### 6.4 How `dev->api` Gets Wired

```
┌─────────────────────────────────────────────────────────────┐
│ 1. In power_switch.c, define the vtable:                    │
│    static const struct power_switch_driver_api              │
│       power_switch_api = { .on=..., .off=..., ... };        │
└──────────────────────┬──────────────────────────────────────┘
                       │
                       ▼
┌─────────────────────────────────────────────────────────────┐
│ 2. Pass it as last arg to DEVICE_DT_INST_DEFINE:            │
│    DEVICE_DT_INST_DEFINE(... &power_switch_api);            │
└──────────────────────┬──────────────────────────────────────┘
                       │
                       ▼
┌─────────────────────────────────────────────────────────────┐
│ 3. Zephyr generates a struct device:                        │
│    const struct device __device_dts_ord_N = {               │
│        .api  = &power_switch_api,   ← AUTO-WIRED            │
│    };                                                       │
└──────────────────────┬──────────────────────────────────────┘
                       │
                       ▼
┌─────────────────────────────────────────────────────────────┐
│ 4. App: DEVICE_DT_GET(node) → returns ptr → uses dev->api   │
└─────────────────────────────────────────────────────────────┘
```

### 6.5 Function Roles Summary

| Function | Where | Visibility | Called By | Calls |
|----------|-------|------------|-----------|-------|
| `power_switch_on()` | `.h` | Public inline | Application | `api->on()` |
| `power_switch_off()` | `.h` | Public inline | Application | `api->off()` |
| `power_switch_get_state()` | `.h` | Public inline | Application | `api->get_state()` |
| `power_switch_api_on()` | `.c` | Private (`static`) | Vtable dispatch | `gpio_pin_set_dt()` |
| `power_switch_api_off()` | `.c` | Private (`static`) | Vtable dispatch | `gpio_pin_set_dt()` |
| `power_switch_api_get_state()` | `.c` | Private (`static`) | Vtable dispatch | (memory read) |
| `power_switch_init()` | `.c` | Private (`static`) | Zephyr at boot | `gpio_pin_configure_dt()` |

---

## PART 7: Why `static inline` in Headers?

### The Apparent Contradiction

- `static` = visible only within a single `.c` file
- Public header = visible to everyone

How can both be true at once?

### The Resolution

Every `.c` file that `#include`s the header gets its **own private inlined copy** of the function. The compiler **inlines** the function body at the call site, eliminating any actual function call.

### Why This Pattern?

| Reason | Explanation |
|--------|-------------|
| **Performance** | Function body inlined → zero call overhead |
| **No linker conflicts** | Each TU has its own copy → no "multiple definition" errors |
| **Type safety** | Wrapper provides type-checked API |
| **Encapsulation** | Hides `dev->api` dereference from users |
| **Convention** | Used throughout Zephyr (GPIO, I2C, SPI APIs) |

### What It Compiles To

```c
// User writes:
power_switch_on(my_dev);

// Compiler effectively produces (after inlining):
((const struct power_switch_driver_api *)my_dev->api)->on(my_dev);
```

The wrapper **disappears** at compile time — pure syntactic sugar with type checking.

---

## PART 8: Build & Flash Steps

### Setup Zephyr (one-time)

```bash
west init ~/zephyrproject
cd ~/zephyrproject && west update
source zephyr/zephyr-env.sh
```

### Build

```bash
cd power_switch_project
rm -rf build
west build -b blackpill_f401cc
```

### Verify Driver Compiled

```bash
west build -b blackpill_f401cc 2>&1 | grep "power_switch.c"
```

You should see something like:
```
[XX/YYY] Building C object .../power_switch.c.obj
```

### Flash via ST-Link

```bash
west flash
```

### Flash via DFU (Black Pill USB)

```bash
west flash --runner dfu-util
```

### Debug Tips

| Check | Command |
|-------|---------|
| DT node merged? | `grep -A5 power_switch build/zephyr/zephyr.dts` |
| Macros generated? | `grep power_switches build/zephyr/include/generated/zephyr/devicetree_generated.h` |
| Kconfig set? | `grep POWER_SWITCH build/zephyr/.config` |
| Module registered? | `cat build/zephyr/zephyr_modules.txt \| grep power_switch` |

---

## PART 9: Common Pitfalls

These are the issues most likely to bite you when creating a custom driver/module.

### Pitfall 1: Missing Devicetree Node Label

```dts
/* ❌ WRONG — only a node name */
power_switches {
    compatible = "power-switch";
};

/* ✅ CORRECT — node label `pwr_sw1` */
pwr_sw1: power_switch_1 {
    compatible = "power-switch";
};
```

`DT_NODELABEL(name)` requires the `label:` syntax **before** the node name.

### Pitfall 2: Compatible Naming Mismatch

The same identifier must appear in three forms:

| Place | Format | Example |
|-------|--------|---------|
| YAML / DTS | hyphenated | `power-switch` |
| C `DT_DRV_COMPAT` | underscored | `power_switch` |
| Kconfig symbol | UPPERCASE | `POWER_SWITCH` |

### Pitfall 3: Wrong Source Path in `zephyr_library_sources()`

Paths are **relative to the CMakeLists.txt that calls them**.

```cmake
# Inside drivers/power_switch/CMakeLists.txt:

# ❌ WRONG — doubles the path
zephyr_library_sources(drivers/power_switch/power_switch.c)

# ✅ CORRECT — relative to current dir
zephyr_library_sources(power_switch.c)
```

### Pitfall 4: `ZEPHYR_EXTRA_MODULES` After `find_package`

```cmake
# ❌ WRONG — Zephyr never sees the module
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
list(APPEND ZEPHYR_EXTRA_MODULES ${CMAKE_CURRENT_SOURCE_DIR}/modules/power_switch)

# ✅ CORRECT — must be set BEFORE find_package
list(APPEND ZEPHYR_EXTRA_MODULES ${CMAKE_CURRENT_SOURCE_DIR}/modules/power_switch)
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
```

### Pitfall 5: Deprecated `label` Property

```dts
/* ❌ Deprecated — generates a warning */
pwr_sw1: power_switch_1 {
    compatible = "power-switch";
    label = "PWR_SW_1";   /* don't use */
};

/* ✅ Use the node label instead */
pwr_sw1: power_switch_1 {
    compatible = "power-switch";
};
```

### Pitfall 6: YAML Indentation

`module.yml` uses **spaces**, not tabs. Validate with:

```bash
python3 -c "import yaml; print(yaml.safe_load(open('modules/power_switch/zephyr/module.yml')))"
```

### Pitfall 7: Inner `CMakeLists.txt` Logic Mismatch

A typical pattern uses **two** CMakeLists.txt files:

| File | Role | Contents |
|------|------|----------|
| `modules/power_switch/CMakeLists.txt` | "Manager" | `add_subdirectory_ifdef(...)` |
| `modules/power_switch/drivers/power_switch/CMakeLists.txt` | "Worker" | `zephyr_library_named(...)` + `zephyr_library_sources(...)` |

Don't swap their contents — manager delegates, worker compiles.

---

## Summary

You now have a complete understanding of:

- ✅ Zephyr's three pillars (Devicetree, Kconfig, Driver Model)
- ✅ The distinction between **drivers** (code) and **modules** (packaging)
- ✅ The full file layout for an out-of-tree driver module
- ✅ Why `static inline` in public headers is a Zephyr/Linux idiom
- ✅ How application calls flow through layers down to hardware
- ✅ How `dev->api` is automatically wired by `DEVICE_DT_INST_DEFINE`
- ✅ The most common pitfalls and how to avoid them

### Possible Next Steps

- 🔌 Add **interrupt support** for an overcurrent feedback pin
- 💻 Add a **shell command** (e.g. `power_switch on/off` via UART)
- 🧪 Write a **unit test** with Ztest
- 📦 Publish your module via a **west manifest**
- 🔁 Convert to an **input switch** (button reader with debouncing)

````
