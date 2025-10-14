# 🌞 Solar Sync — MPPT Solar Tracking

## Overview
**Solar Sync** is a compact Arduino-based platform combining solar tracking and hybrid MPPT control.  
It uses the **ASSTA Hybrid (Adjustable Step Size Theta)** algorithm with a **current-zone clamp**.

---

## Features
- **50 kHz PWM control** of a DC/DC converter (Timer 1 Fast PWM)
- **Real-time OLED display** (128×64 I²C @ 0x3D)
- **PV voltage/current/output sensing** via ADC (A0–A2)
- **ACS712 30 A current sensor** input with calibration support
- **Serial Plotter output** for VS Code or Arduino IDE  
  (`>Vpv:…,Ipv:…,Vout:…,Ppv:…,Duty:…,Imp:…`)

---

## Hardware Setup
| Signal | Pin | Description |
|---------|-----|-------------|
| PV voltage | A0 | 82 kΩ / 18 kΩ divider |
| PV current | A1 | ACS712-30A sensor (66 mV/A) |
| Buck output voltage | A2 | 82 kΩ / 18 kΩ divider |
| PWM output | D9 | 50 kHz gate drive |
| OLED display | I²C @ 0x3D | 128×64 SSD1306 |

--

## Hybrid MPPT Flowchart

![Hybrid MPPT Flowchart](Technical%20Documents/Hybrid%20MPPT-Flowchart.png)