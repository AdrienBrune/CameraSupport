<div align="center">

# 📷 DIY Zigbee Motorized Camera Mount

<p align="center">
  <img width="254" height="248" alt="image" src="https://github.com/user-attachments/assets/fb879e1c-1efd-4de9-8502-a520acef1356" style="border-radius: 8px;" />
</p>

*A custom 3D-printed, 180° pan-and-tilt motorized camera mount powered by an ESP32, running natively on Zigbee for seamless Home Assistant integration.*

---

![Version](https://img.shields.io/badge/version-1.0.0-blue.svg)
![Author](https://img.shields.io/badge/author-Adrien%20Brune-orange.svg)
![Language](https://img.shields.io/badge/language-C%2F++-yellow.svg)
![Protocol](https://img.shields.io/badge/protocol-Zigbee-blueviolet.svg)
![Platform](https://img.shields.io/badge/platform-ESP32%20%7C%20Home%20Assistant-lightgrey.svg)
![Status](https://img.shields.io/badge/status-active-success.svg)

</div>

## 💡 Introduction

Welcome to the **DIY Zigbee Motorized Camera Mount** project! This device bridges custom hardware design and home automation by offering a smart, rotatable mount for your security or monitoring cameras. 

Built around an **ESP32** microcontroller communicating via the **Zigbee** protocol, the mount integrates into **Home Assistant Z2M** with an external converter file provided in source code. This device requires no cloud services or complex bridges. Inside the sleek custom enclosure, a precision servomotor provides a **180° tilt and rotation range**, allowing you to adjust your camera's field of view directly from your dashboard or via automation routines.

---

## ✨ Key Features

* **Native Zigbee Integration:** Directly pairs with your Zigbee coordinator (Zigbee2MQTT) for local, fast, and secure communication.
* **Home Assistant Ready:** Fully compatible with Home Assistant out of the box using custom external definition files.
* **Smooth 180° Range:** Powered by an internal servomotor managed by optimized C++ firmware.
* **ESP32-Powered:** Highly reliable, low-latency wireless microcontroller handling peripheral control and network stack.
* **Clean Design:** Compact, custom-designed enclosure with a discreet status indicator and clean USB power routing.

---

## 🛠️ Technical Stack & Hardware

* **Microcontroller:** ESP32 (Wireless SoC)
* **Firmware Language:** C++
* **Communication Protocol:** Zigbee
* **Actuator:** Servomotor (180° operational angle)
* **Smart Home Platform:** Home Assistant (Zigbee2MQTT)
