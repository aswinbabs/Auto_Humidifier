# 🌫️ ESP32 DHT11-Based Smart Humidifier Controller 

This project automates a **humidifier** using data from a **DHT11 sensor** and allows **manual or automatic control** via the **Blynk IoT app**. Communication is handled over **WiFi** using the **HTTP protocol**, and real-time data (temperature and humidity) is sent to the Blynk server.

---

## 🚀 Features

- 📡 **Read temperature and humidity** from DHT11  
- 🌫️ **Auto-mode**: Automatically turns humidifier ON/OFF based on temperature levels  
- 🎛️ **Manual-mode**: Control the humidifier manually from Blynk app  
- 📲 **Real-time data publishing** to Blynk via HTTP  
- 🌐 **WiFi-enabled ESP32** communication  
- ⏱️ **FreeRTOS**-based task management for smooth multitasking  

---

## 📦 Tech Stack

- **ESP32** – Microcontroller  
- **DHT11** – Temperature and humidity sensor  
- **HTTP (GET/POST)** – For sending and receiving data  
- **Blynk IoT** – For app-based control and data visualization  
- **FreeRTOS** – Task scheduling and multitasking  
- **C++ / ESP-IDF Framework** – Firmware development  

---

## 🔧 Hardware Requirements

- ESP32 Development Board  
- DHT11 Sensor  
- Humidifier
- IRLB4132 / IRLZ44N or any logic level MOSFET or standard MOSFET with driver  
- Resistors - 220Ω, 10KΩ  
- Diode - 1N4007  
- Jumper wires  
- Breadboard (optional)  

---

## 📱 Blynk Setup

1. Create a project in [Blynk](https://blynk.io/)  
2. Add the following widgets:
   - Switch (Manual / Auto toggle)  
   - Switch (Manual humidifier ON/OFF)  
   - Value Displays for temperature & humidity  
3. Note down the **Auth Token**, **WiFi credentials**, and **HTTP server details**  

---

## 🗂️ Project Structure

---

## ⚙️ How It Works

- DHT11 periodically sends temperature and humidity data  
- Data is sent to the server using **HTTP POST** requests  
- The app fetches the latest readings via **HTTP GET** requests  
- Based on the mode selected (**Manual / Auto**), control logic:  
  - **Auto**: the system automatically turns the humidifier on/off based on temperature thresholds  
  - **Manual**: user can control the humidifier directly from the app via HTTP commands  
