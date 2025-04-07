# 🌫️ ESP32 DHT11-Based Smart Humidifier Controller (WIP)

This project automates a **humidifier** using data from a **DHT11 sensor** and allows **manual or automatic control** via the **Blynk IoT app**. Communication is handled over **WiFi** using the **MQTT protocol**, and real-time data (temperature and humidity) is sent to the Blynk server.

> ⚠️ **Note:** This is a **work in progress** project.

---

## 🚀 Features

- 📡 **Read temperature and humidity** from DHT11
- 🌫️ **Auto-mode**: Automatically turns humidifier ON/OFF based on temperature levels
- 🎛️ **Manual-mode**: Control the humidifier manually from Blynk app
- 📲 **Real-time data publishing** to Blynk via MQTT
- 🌐 **WiFi-enabled ESP32** communication
- ⏱️ **FreeRTOS**-based task management for smooth multitasking

---

## 🔧 Hardware Requirements

- ESP32 Development Board  
- DHT11 Sensor  
- Humidifier (controlled via relay module)  
- Relay module (for humidifier control)  
- Jumper wires  
- Breadboard (optional)  

---

## 📱 Blynk Setup

1. Create a project in [Blynk](https://blynk.io/)
2. Add the following widgets:
   - Switch (Manual / Auto toggle)
   - Switch (Manual humidifier ON/OFF)
   - Value Displays for temperature & humidity
3. Note down the **Auth Token**, **WiFi credentials**, and **MQTT server details**

---

## 🗂️ Project Structure

---

## ⚙️ How It Works

- DHT11 periodically sends data
- Data is published to MQTT topics
- Blynk app subscribes to the topics for real-time display
- Based on the mode selected (Manual / Auto), control logic:
  - Auto: turns humidifier on/off based on temperature threshold
  - Manual: user controls it directly from the app
