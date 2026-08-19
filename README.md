<div align="center">

#  Mechatronics & IoT Engineering Projects

*Centralized repository for the development of unified embedded systems.*

[![Status](https://img.shields.io/badge/Status-Active_Development-2ea44f?style=for-the-badge&logo=github)](https://github.com/)
[![Hardware](https://img.shields.io/badge/EDA-KiCad_10-314CB6?style=for-the-badge&logo=kicad)](https://www.kicad.org/)
[![MCU](https://img.shields.io/badge/MCU-ESP32_32U-E7352C?style=for-the-badge&logo=espressif)](https://www.espressif.com/)
[![Manufacturing](https://img.shields.io/badge/PCB-Panelized-00A6CE?style=for-the-badge)](https://github.com/)

</div>

---

> **💡 Manufacturing Note:** To drastically optimize industrial production costs, the PCBs for both projects (HydroScan and AUTOAMBU) have been unified into a single manufacturing panel (Panelized Gerber), sharing the same manufacturing cycle.

---

##  Project 1: HydroScan (Maritime IoT Node)
**Smart buoy for real-time water quality monitoring.***

<p align="center">
  <img src="https://img.shields.io/badge/Telecom-LTE-FF8C00?style=flat-square" />
  <img src="https://img.shields.io/badge/Location-GPS-D83B01?style=flat-square" />
  <img src="https://img.shields.io/badge/Sensor-Salinity-00B294?style=flat-square" />
  <img src="https://img.shields.io/badge/Power-Solar_Panels-FFD700?style=flat-square" />
</p>

* 📡 **Telemetry:** Real-time environmental data transmission via LTE networks.
* 📍 **Tracking:** Integrated GPS geolocation for precise positioning of the node.
* 🔋 **Power:** Self-sustaining energy system utilizing solar panels to charge and monitor the sensors autonomously.
* 🔬 **Application:** Oceanographic research and environmental monitoring in coastal areas.
* 📁 [**Explore HydroScan Firmware ➔**](./Firmware_HydroScan)

---

## 🫁 Project 2: AUTOAMBU (Automated Respirator)
**Automated respiratory assistance system.**

<p align="center">
  <img src="https://img.shields.io/badge/Actuator-DC_Motor_w/_Encoder-FFB900?style=flat-square" />
  <img src="https://img.shields.io/badge/Driver-L298N-D83B01?style=flat-square" />
  <img src="https://img.shields.io/badge/Power-USB_Fast_Charging-00A6CE?style=flat-square" />
  <img src="https://img.shields.io/badge/Doc-Thesis_Chapter-107C10?style=flat-square" />
</p>

* ⚙️ **Actuation & Control:** Precision mechanical control using a DC motor equipped with an encoder and driven by an L298N H-bridge.
* ⚡ **Power Management:** USB Super Fast Charging architecture designed to simultaneously power the circuitry and charge the system.
* 🧠 **Architecture:** Centralized ESP32 32U control logic with dedicated thermal management for continuous and safe operation.
* 📁 [**Explore AUTOAMBU Firmware ➔**](./Firmware_AUTOAMBU)

---

## 👥 Development Team

* ⚡ **Paul** - Hardware Design and PCB Routing 
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
