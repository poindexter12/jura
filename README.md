# ☕ Jura ESPHome Components

[![Build](https://github.com/poindexter12/jura/actions/workflows/build.yml/badge.svg)](https://github.com/poindexter12/jura/actions/workflows/build.yml)

**Control and monitor your Jura coffee machine and CoolControl milk cooler directly from ESPHome and Home Assistant.**

This is a maintained fork of [tiaanv/jura](https://github.com/tiaanv/jura), which modernized [Ryan Alden's original Jura component](https://github.com/ryanalden/esphome-jura-component) for ESPHome's external-component architecture. Full lineage in [Credits](#-credits).

> ⚠️ *Use at your own risk.*
> This project interfaces with hardware not meant for third-party control. While it has been tested successfully on several Jura models (notably the **E8**), you are responsible for any damage or warranty issues.

---

## 🌟 Overview

Two independent ESPHome external components live in this repo:

- **`jura`** — polls a Jura coffee machine over its service UART, decoding drink counters, maintenance counters, and status flags into Home Assistant entities. You can also send commands (power on/off, brew a drink).
- **`jura_coolcontrol`** — reads the Jura **CoolControl** milk cooler, exposing milk level and temperature.

### 🧰 Key improvements over the original component

- ✅ Migrated from `custom_component` → ESPHome's **`external_component`** system
- ✅ Replaced Arduino `String` with C++ `std::string` → **works on ESP32 IDF**
- ✅ Fully self-contained (no external sensors required)
- ✅ Updated **bit-flags** for the Jura E8 (your model may differ)
- ✅ **Model selection** — per-model sensor maps, refined with community feedback
- ✅ **Jura CoolControl** support (milk cooler integration)
- ✅ **Diagnostic sensors** to help discover counter and flag meanings — see [Diagnostics](#-diagnostics)

---

## ⚡ Hardware & Wiring

> **⚠️ Warning:** Incorrect wiring can permanently damage your Jura machine or your ESP device.
> ESP pins are **not 5 V tolerant**. Always use a level-shifter if unsure.

![Jura Wiring Diagram](https://github.com/user-attachments/assets/6f2bb48f-e853-409c-b768-2b08b87c70d2)

| Pin | Description | Notes |
|-----|--------------|-------|
| TX  | ESP → Jura   | Send commands |
| RX  | Jura → ESP   | Receive data |
| GND | Common ground | Shared reference |

### Example UART setup (ESP32-C3)

```yaml
uart:
  id: uart_bus
  tx_pin: GPIO21
  rx_pin: GPIO20
  baud_rate: 9600
```

On ESP8266 boards (e.g. a D1 mini), `D1`/`D2` work well — see [examples/e6_e8.yaml](examples/e6_e8.yaml).

### ⚙️ Practical note about voltage tolerance

Officially, ESP devices are **not 5 V tolerant**.
Unofficially — many of us have connected 5 V UARTs to ESP boards without immediate issues.
Proceed at your own risk: your luck, your device, your coffee. ☕😅

---

## 🚀 Installation & Quick Start

Add the components to your ESPHome YAML:

```yaml
external_components:
  - source:
      type: git
      url: https://github.com/poindexter12/jura
    components: [ jura, jura_coolcontrol ]  # You only need one!

uart:
  id: uart_bus
  tx_pin: GPIO21
  rx_pin: GPIO20
  baud_rate: 9600

# Only define one! One ESP, one machine.
# Use this one for coffee machines:
jura:
  id: jura_main
  uart_id: uart_bus
  model: E8

# ...or this one for the milk cooler:
jura_coolcontrol:
  id: jura_cool
  uart_id: uart_bus
```

This is just the barebones structure. The [examples/](examples/) folder has complete, buildable configurations:

| Example | Target | Framework |
|---------|--------|-----------|
| [e6_e8.yaml](examples/e6_e8.yaml) | ESP8266 (D1 mini) | Arduino |
| [f6.yaml](examples/f6.yaml) | ESP8266 (D1 mini) | Arduino |
| [f7.yaml](examples/f7.yaml) | ESP8266 (D1 mini) | Arduino |
| [coolcontrol.yaml](examples/coolcontrol.yaml) | ESP32-C3 | ESP-IDF |

---

## ☕ Jura Component

The **`jura`** component polls the machine (default every **2 s**), sending two serial commands — `RT:0000` for counters and `IC:` for status flags — and decoding the responses.

### Configuration

```yaml
jura:
  id: jura_coffee
  uart_id: uart_bus
  model: E8              # Required: E6, E8, F6, F7, or UNKNOWN
  update_interval: 2s    # Optional, default 2s
```

| Option | Type | Default | Description |
|--------|------|---------|-------------|
| `model` | enum | **required** | One of `E6`, `E8`, `F6`, `F7`, `UNKNOWN` (case-insensitive). Selects which counters map to which sensors. |
| `uart_id` | id | — | The UART bus connected to the machine. |
| `update_interval` | time | `2s` | Polling interval. |
| *sensor keys* | schema | sensible names | Every entity below has a YAML key (e.g. `single_espresso_made`, `machine_status`) you can optionally override to change its name, icon, id, filters, etc. If omitted, default names are used. |

Only the entities relevant to the selected `model` are created — overriding a key for a sensor your model doesn't expose has no effect.

### Home Assistant entities

Which numeric sensors you get depends on `model`:

| YAML key / entity | E6 | E8 | F6 | F7 | UNKNOWN |
|-------------------|:--:|:--:|:--:|:--:|:-------:|
| `single_espresso_made` — Single Espresso Made | ✅ | ✅ | ✅ | ✅ | ✅ |
| `double_espresso_made` — Double Espresso Made | ✅ | ✅ | ✅ | ✅ | ✅ |
| `coffee_made` — Coffee Made | ✅ | ✅ | ✅ | ✅ | ✅ |
| `double_coffee_made` — Double Coffee Made | ✅ | — | ✅ | ✅ | — |
| `flat_white_made` — Flat White Made | — | ✅ | — | — | ✅ |
| `cappuccino_made` — Cappuccino Made | — | ✅ | — | ✅ | ✅ |
| `ristretto_made` — Ristretto Made | — | — | — | ✅ | — |
| `double_ristretto_made` — Double Ristretto Made | — | — | — | ✅ | — |
| `milk_portion_made` — Milk Portions Made | — | ✅ | — | — | — |
| `brews_performed` — Brews Performed | ✅ | — | ✅ | — | — |
| `brews_movements_performed` — Brew Movements Performed | — | ✅ | — | ✅ | ✅ |
| `rinses_performed` — Rinses Performed | — | ✅ | — | — | ✅ |
| `cleanings_performed` — Cleanings Performed | ✅ | ✅ | ✅ | ✅ | ✅ |
| `descalings_performed` — Descalings Performed | ✅ | ✅ | — | ✅ | — |
| `brews_since_cleaning_performed` — Brews Performed Since Cleaning | — | ✅ | — | — | — |
| `brews_since_descaling_performed` — Brews Performed Since Descaling | — | ✅ | — | — | — |
| `grounds_level` — Grounds Level (pucks in the grounds bin) | ✅ | ✅ | ✅ | ✅ | ✅ |

All models also get these text sensors:

| YAML key / entity | Values / purpose |
|-------------------|------------------|
| `tray_status` — Tray Status | `Present` / `Missing` |
| `water_tank_status` — Water Tank Status | `OK` / `Fill Tank` |
| `machine_status` — Machine Status | `Ready`, `Tray Missing`, `Fill Tank`, `Busy (Coffee Drink)`, `Busy (Milk Drink)` |
| `counters_changed` — Changed Counters | Diagnostic: raw counter deltas between polls, for discovery |
| `ic_bits` — IC Bits | Diagnostic: raw status bit flags (`A=… B=…`), for discovery |

### Example dashboard

![Jura Dashboard](https://github.com/user-attachments/assets/8fde2d3c-cc85-4a5d-ab0a-e84f5641cd6e)

### Control commands

Send commands with the `cmd2jura()` function. The most useful ones:

| Command | Description |
|---------|-------------|
| `AN:01` | Turn machine on |
| `AN:02` | Turn machine off |
| `FA:04` | Single espresso |
| `FA:05` | Ristretto |
| `FA:06` | Hot water |
| `FA:07` | Cappuccino (models with milk capability) |
| `FA:09` | Coffee |

**Note:** `FA:` commands simulate button presses and are **model-specific** — the same command may trigger different drinks on different machines. Always test on your model.

#### Example button

```yaml
button:
  - platform: template
    name: 'Make Espresso'
    icon: "mdi:coffee"
    on_press:
      - lambda: |-
          auto result = id(jura_coffee).cmd2jura("FA:04");
```

The [examples/](examples/) folder has full button sets per model.

📖 **For the comprehensive command list and discovery guidance, see [COMMANDS.md](COMMANDS.md).**

---

## 🥶 Jura CoolControl Component

Reads the official Jura **CoolControl milk cooler**, which broadcasts its status over its own UART link.

### Configuration

```yaml
jura_coolcontrol:
  id: jura_cool
  uart_id: uart_bus
  update_interval: 2s    # Optional, default 2s
  level:
    name: "Jura Coolcontrol Level"
  temperature:
    name: "Jura Coolcontrol Temperature"
```

| Option | Type | Default | Description |
|--------|------|---------|-------------|
| `uart_id` | id | — | The UART bus connected to the cooler. |
| `update_interval` | time | `2s` | Polling interval. |
| `level` | sensor schema | not created | Milk level sensor (`%`, 0 decimals). Omit to skip it. |
| `temperature` | sensor schema | not created | Cooler temperature sensor (`°C`, 1 decimal). Omit to skip it. |

Unlike the `jura` component, the two sensors are only created if you declare them in YAML.

![CoolControl Entities](https://github.com/user-attachments/assets/f9654b9d-b26e-46c5-b7aa-83a001afc28c)

See [examples/coolcontrol.yaml](examples/coolcontrol.yaml) for a complete ESP32-C3 (ESP-IDF) configuration.

---

## 🧠 Advanced Info

- The coffee machine speaks Jura's **2-bits-per-byte serial encoding** over UART at 9600 baud — each real byte is spread across four UART bytes, with an `8 ms` delay between characters.
- Responses end with `\r\n`.
- Bit positions differ between models — use the debug logs and diagnostic sensors to identify your own.

---

## 🔧 Diagnostics

Two diagnostic sensors make it easier to figure out what your specific machine's registers mean, so we can map new models.

### Changed Counters

Captures the raw `RT:0000` register values (converted to decimal). Whenever a value changes between polls, the deltas are published as a comma-separated text value (shown here as it appears in the Home Assistant logbook):

![Changed Counters Example](https://github.com/user-attachments/assets/022bba13-35f9-4531-8a3d-6e19dc9cb5a1)

```text
changed to counter_4 9836→9837, counter_11 41177→41181, counter_14 630→631, counter_15 7→8, counter_16 136→137
```

The list gives you clues about what changed after making a specific beverage. In the example above, after a Flat White on a Jura E8:

| Counter | Values | Notes |
|--------|-------|-------------|
| counter_4 | 9836→9837 | On other machines this position is `double_coffee`; on the E8 it tracks Flat Whites — this finding updated the E8 sensor map. |
| counter_11 | 41177→41181 | Increases by more than 1 per beverage; research suggests brew movements. |
| counter_14 | 630→631 | Brews since descaling (resets after a descale). |
| counter_15 | 7→8 | Grounds (pucks) in the bin — resets when the grounds container is emptied. |
| counter_16 | 136→137 | Brews since cleaning. |

### Contributing your findings

Once you've established some known values, open an issue with:

- Model name
- Counter number
- Example from → to values
- Suggested sensor name and description

See the [Jura UART map](Jura_uart_map.md) for what we know so far.

---

## 🔧 Development Notes

- Tested on **ESP8266** and **ESP32** variants (the examples cover a D1 mini and an ESP32-C3)
- Compatible with **ESPHome ≥ 2024.4**
- Compiles on both **Arduino** and **ESP-IDF** frameworks
- **CI:** every pull request and push to `main` [compiles all four example configs](.github/workflows/build.yml), so changes that break a build are caught before merge

---

## 🧰 Future Improvements

- Figure out more bit-flag meanings from `IC:`
- Clarify more counter values from `RT:`
- Map more models (see [Jura_uart_map.md](Jura_uart_map.md))

---

## 💡 Contributing

Please use the [diagnostics procedure](#-diagnostics) above to contribute your machine's counter values and bit flags!

Pull requests, improvements, and new flag maps for other Jura models are very welcome.
Let's make our coffee smarter — responsibly.

---

## 🌟 Credits

- [tiaanv/jura](https://github.com/tiaanv/jura) — the upstream of this fork, which modernized the component for ESPHome's external-component architecture and added CoolControl support
- [Ryan Alden's original component](https://github.com/ryanalden/esphome-jura-component) — the one that started it all
- [AH Wood's fantastic F7 component](https://github.com/alco28/Jura-F7-ESPHOME) — with much more!
- [Jutta-Proto protocol project](https://github.com/Jutta-Proto/protocol-cpp) — protocol research

---

## ⚠️ Final Thoughts

> ☕ "Just because you *can*, doesn't mean you *should*."
> This project is purely for educational tinkering.
> Interfacing directly with commercial appliances **can be dangerous**.
> Be cautious, monitor your device, and never leave it unattended.
