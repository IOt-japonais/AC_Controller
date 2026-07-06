# Smart Heat Stroke Prevention System using ESP32

In this project, we build a **smart IoT system** designed to help protect elderly people and family members from **heat stress and heat stroke** using an **ESP32** and a **DHT22 temperature and humidity sensor**.

The ESP32 continuously measures the ambient temperature and humidity, calculates the **Heat Index**, and determines the current heat risk level. The system then displays the risk level and automatically controls the air conditioner when necessary.

To operate the air conditioner, the ESP32 transmits **infrared (IR)** commands that replicate those sent by the original remote control, allowing the system to automatically turn the AC on or off based on the environmental conditions.

The project is also connected to the Internet, enabling remote monitoring and control through an Android application. Family members can:

* Monitor the temperature, humidity, and Heat Index from anywhere.
* Remotely control the air conditioner.
* Receive instant notifications when the Heat Index reaches a dangerous level, allowing them to take immediate action.

## What you'll learn

* Understanding the **Heat Index** and its importance in preventing heat-related illnesses.
* Interfacing the **DHT22** sensor with the ESP32.
* Measuring temperature and humidity in real time.
* Calculating the Heat Index on the ESP32.
* Controlling an air conditioner using **infrared (IR)** communication.
* Capturing and replaying IR commands from the original AC remote.
* Connecting the ESP32 to the Internet.
* Sending real-time notifications to family members.
* Developing an Android application for remote monitoring and control.

## Technologies Used

* ESP32
* DHT22 Temperature & Humidity Sensor
* Infrared (IR) LED
* 2N2222 NPN Transistor
* Wi-Fi
* Android App
* IoT
* Heat Index Calculation

This project demonstrates how low-cost IoT hardware can be used to build a practical system that improves home safety by automatically monitoring environmental conditions, controlling air conditioning, and notifying caregivers before heat-related emergencies occur.
