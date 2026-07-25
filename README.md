<div align="center">

# ⚙️ Mechatronics & IoT Engineering Projects

*Centralized repository for the development of unified embedded systems.*

[![Status](https://img.shields.io/badge/Status-Active_Development-2ea44f?style=for-the-badge&logo=github)](https://github.com/)
[![Hardware](https://img.shields.io/badge/EDA-KiCad_8-314CB6?style=for-the-badge&logo=kicad)](https://www.kicad.org/)
[![MCU](https://img.shields.io/badge/MCU-ESP32_32U-E7352C?style=for-the-badge&logo=espressif)](https://www.espressif.com/)
[![Manufacturing](https://img.shields.io/badge/PCB-Panelized-00A6CE?style=for-the-badge)](https://github.com/)

</div>

---

> **💡 Manufacturing Note:** To drastically optimize industrial production costs, the PCBs for both projects (HydroScan and AUTOAMBU) have been unified into a single manufacturing panel (Panelized Gerber), sharing the same manufacturing cycle.

---

## 🌊 Project 1: HydroScan (Maritime IoT Node)
**Smart buoy for real-time water quality monitoring.**

<p align="center">
  <img src="https://img.shields.io/badge/Sensor-pH-0078D7?style=flat-square" />
  <img src="https://img.shields.io/badge/Sensor-Salinity-00B294?style=flat-square" />
  <img src="https://img.shields.io/badge/Telecom-RF-FF8C00?style=flat-square" />
</p>

* 📡 **Telemetry:** Environmental data transmission via radio frequency.
* 🔬 **Application:** Oceanographic research and environmental monitoring in coastal areas.
* 📁 [**Explore HydroScan Firmware ➔**](./Firmware_HydroScan)

---

## 🫁 Project 2: AUTOAMBU (Automated Respirator)
**Automated respiratory assistance system.**

<p align="center">
  <img src="https://img.shields.io/badge/Driver-L298N-D83B01?style=flat-square" />
  <img src="https://img.shields.io/badge/Actuators-Steppers/DC-FFB900?style=flat-square" />
  <img src="https://img.shields.io/badge/Doc-Thesis_Chapter-107C10?style=flat-square" />
</p>

* 🧠 **Base Control:** Centralized control architecture on the microcontroller.
* 🌡️ **Hardware:** Dedicated thermal management for continuous and safe operation.
* 📁 [**Explore AUTOAMBU Firmware ➔**](./Firmware_AUTOAMBU)

---

## 👥 Development Team

* ⚡ **Paul** - Hardware Design and PCB Routing (Panelization)
* 🏗️ **Miguel** - Structural 3D Design
* 💻 **Isaias** - Firmware Development
* 💻 **Alberto** - Firmware Development

---

## 📂 Repository Architecture

| Directory | Description |
| :--- | :--- |
| 🖨️ [**`Hardware_Panelized/`**](./Hardware_Panelized) | Unified KiCad project and joint production Gerber files ready for manufacturing. |
| 💻 [**`Firmware_AUTOAMBU/`**](./Firmware_AUTOAMBU) | Source code (C/C++) and control routines for the medical respirator. |
| 📡 [**`Firmware_HydroScan/`**](./Firmware_HydroScan) | Telemetry logic and sensor reading for the maritime buoy. |
| 📚 [**`Docs/`**](./Docs) | Technical documentation, datasheets, schematics, and 3D models for both systems. |
