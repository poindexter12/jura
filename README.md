# ☕ Jura ESPHome Components

**Control and monitor your Jura Coffee Machine and CoolControl milk cooler directly from ESPHome and Home Assistant.**

> ⚠️ *Use at your own risk.*  
> This project interfaces with hardware not meant for third-party control. While it’s been tested successfully on several Jura models (notably the **E8**), you are responsible for any damage or warranty issues.

---

## 🌟 Overview

This project builds on the fantastic work by [Ryan Alden’s original Jura component](https://github.com/ryanalden/esphome-jura-component) more credits below, modernizing it for **ESPHome’s new external component architecture** and ensuring **cross-platform compatibility** (ESP8266 & ESP32).

### 🧰 Key Improvements

- ✅ Migrated from `custom_component` → ESPHome’s **`external_component`** system  
- ✅ Replaced Arduino `String` with C++ `std::string` → **works on ESP32 IDF**  
- ✅ Fully self-contained component (no external sensors required)  
- ✅ Updated **bit-flags** for the Jura E8 (your model may differ)
- ✅ Introduced model selection. With the help of the community, we will refine this for each machine type.
- ✅ Added **Jura CoolControl** support (milk cooler integration)  
- ✅ Diagnostic Sensors to help discover counter and flag meanings. See below 

---

## ⚡ Hardware & Wiring

> **⚠️ Warning:** Incorrect wiring can permanently damage your Jura machine or your ESP device.  
> ESP pins are **not 5 V tolerant**. Always use a level-shifter if unsure.
<img width="468" height="324" alt="image" src="https://github.com/user-attachments/assets/6f2bb48f-e853-409c-b768-2b08b87c70d2" />


| Pin | Description | Notes |
|-----|--------------|-------|
| TX  | ESP → Jura   | Send commands |
| RX  | Jura → ESP   | Receive data |
| GND | Common Ground | Shared reference |

### Example connection (ESP32-C3)

```yaml
uart:
  id: uart_bus
  tx_pin: GPIO21
  rx_pin: GPIO20
  baud_rate: 9600
```

<details>
<summary>⚙️ Practical note about voltage tolerance</summary>

Officially, ESP devices are **not 5 V tolerant**.  
Unofficially—many of us have connected 5 V UARTs to ESP boards without immediate issues.  
Proceed at your own risk: your luck, your device, your coffee. ☕😅  
</details>

---

## 🧩 ESPHome Configuration

Example YAML:

```yaml
external_components:
  - source:
      type: git
      url: https://github.com/tiaanv/jura
    components: [ jura, jura_coolcontrol ] #You only need one!

uart:
  id: uart_bus
  tx_pin: GPIO21
  rx_pin: GPIO20
  baud_rate: 9600

#Only define one! One ESP. One Machine
#Use this one for coffee machines
jura:
  id: jura_main
  uart_id: uart_bus
  model: E8

#Use this one for the Milk Cooler!
jura_coolcontrol:
  id: jura_cool
  uart_id: uart_bus
```
This is just the barebones structure to show you how to reference external components.

*Note that for the Jura component, you need to specify a Model. Current options are: E6, E8, F6, F7 or UNKNOWN.  We are working on adding support for new models and enhancing current model detail as the communnty gives feedback, using the diagnostic functions.*

See the [examples/](examples/) folder for complete configuration examples.

---

## ☕ Jura Component

The **Jura component** polls the machine every **2 seconds**, sending two serial commands and decoding the responses.

### Home Assistant Entities

| Sensor | Description |
|---------|--------------|
| `Single Espresso Made` | Total count |
| `Double Espresso Made` | Total count |
| `Coffee Made` | Total count |
| `Double Coffee Made` | Total count |
| `Cleanings Performed` | Total cycles |
| `Brews Performed` | Total count |
| `Grounds Capacity Remaining` | Remaining ground cycles |
| `Tray Status` | “Present” / “Missing” |
| `Water Tank Status` | “OK” / “Fill Tank” |
| `Machine Status` | Ready, Busy, Tray Missing, etc. |
| `Changed Counters` | Show the raw value changes between previous and current for diagnostics and discovery|
| `IC Bits` | Show the raw bit/status flags for diagnostics and discovery |


The sensors exposed by each machine may be different depending on the model selected.

### Example Dashboard

<img width="330" alt="Jura Dashboard" src="https://github.com/user-attachments/assets/8fde2d3c-cc85-4a5d-ab0a-e84f5641cd6e" />

### Control Commands

You can create buttons to control your Jura machine using the `cmd2jura()` function. Here are common commands:

#### Machine Control
| Command | Description |
|---------|-------------|
| `AN:01` | Turn machine on |
| `AN:02` | Turn machine off |

#### Beverage Commands
| Command | Beverage | Notes |
|---------|----------|-------|
| `FA:04` | Single Espresso | Standard espresso shot |
| `FA:05` | Ristretto | Short, concentrated espresso |
| `FA:06` | Hot Water | Dispense hot water |
| `FA:07` | Cappuccino | Espresso with milk foam (models with milk capability) |
| `FA:09` | Coffee | Standard coffee |

**Note**: Available beverages vary by model. Not all commands work on all machines (e.g., cappuccino requires milk frothing capability). FA commands are model-specific and may trigger different drinks on different models.

#### Example Button Configuration

```yaml
button:
  - platform: template
    name: 'Make Espresso'
    icon: "mdi:coffee"
    on_press:
      - lambda: |-
          auto result = id(jura_coffee).cmd2jura("FA:04");
```

See the [examples/](examples/) folder for complete button configurations for each model.

📖 **For a comprehensive list of known commands and discovery guidance, see [COMMANDS.md](COMMANDS.md)**

---

## 🥶 Jura CoolControl Component

Monitors the official Jura **CoolControl milk cooler**, which continuously broadcasts its data.

### Exposed Sensors

| Sensor | Unit | Range | Description |
|--------|-------|--------|-------------|
| Level | % | 0–100 | Milk level |
| Temperature | °C | 0–50 | Cooler temperature |

<img width="325" alt="CoolControl Entities" src="https://github.com/user-attachments/assets/f9654b9d-b26e-46c5-b7aa-83a001afc28c" />

---

## 🧠 Advanced Info

- Communication uses a **custom 2-bit serial protocol** over UART.  
- Each message is sent as encoded bytes with timing delays (`8 ms` between chars).  
- The machine echoes responses ending with `\r\n`.  
- Bit positions differ between models — use the debug logs to identify your own.

---

## 🔧 Diagnostics

Two sensors are added to the component to make it simpler to figure out your specific model Jura's values (from the registers) and the possible sensors

Changed Counters

This sensor captures the RAW values converted to decimals from the first data register.  When it detects a change, it logs this change as a text value as shown:

<img width="557" height="149" alt="image" src="https://github.com/user-attachments/assets/022bba13-35f9-4531-8a3d-6e19dc9cb5a1" />

This comma-separated list will give you clues about what values changed after making a specific beverage.
```
changed to counter_4 9836→9837, counter_11 41177→41181, counter_14 630→631, counter_15 7→8, counter_16 136→137
```
In the example above, after making a "Flat White"  on the Jura E8, you can see several counters increased:
| Counter | Values | Notes |
|--------|-------|-------------|
| counter_4 | 9836→9837 | After investigation, this is the value on other machines related to double_coffee.   Using this information, I updated the sensor publishing for the E8 model.|
| counter_11 | 41177→41181 | On the Jura E8, the value increased normally by more than 1 per beverage, and from research, it seems this is the amount of brew_movements.|
| counter_14 | 630→631 | Unknown at this stage.... we will see if it resets.|
| counter_15 | 7→8 | This one is known to be the amount of grounds(pucks) after emptying the grounds container, it resets.|
| counter_16 | 136→137 | Unknown for now.  It might be the count of beverages made after cleaning or descaling.  Will monitor.|

Once you have established some known values, you may create an issue with your findings.  Please make sure to include the following details:
- Model Name
- Counter number
- Example from and to values
- suggested sensor name and description

Please see the [Jura UART map](https://github.com/tiaanv/jura/blob/main/Jura_uart_map.md) for what we know so far.



---


## 🔧 Development Notes

- Tested on **ESP8266**, **ESP32-C1**, and **ESP32-S3**
- Compatible with **ESPHome ≥ 2024.4**
- Compiles cleanly on both **Arduino** and **ESP-IDF**

---

## 🧰 Future Improvements

- Figure out more "bit-flag" meanings from **CI:**
- Clarify more quantity Values from **RT:**

## 🌟 Credits

- [Ryan Alden's OG component that inspired this one](https://github.com/ryanalden/esphome-jura-component)
- [AH Wood's fantastic component with much more!](https://github.com/alco28/Jura-F7-ESPHOME)
- [Jura Proto project](https://github.com/Jutta-Proto/protocol-cpp?tab=readme-ov-file)

---
## ⚠️ Final Thoughts

> ☕ “Just because you *can*, doesn’t mean you *should*.”  
> This project is purely for educational tinkering.  
> Interfacing directly with commercial appliances **can be dangerous**.  
> Be cautious, monitor your device, and never leave it unattended.

---

## 💡 Contributions Welcome

Please use the diagnostics procedure above to contribute your machine values and bit status flags!

Pull requests, improvements, or new flag maps for other Jura models are very welcome!  
Let’s make our coffee smarter — responsibly.
