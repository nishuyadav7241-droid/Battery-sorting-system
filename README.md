# AI-Powered Battery Sorting System

A real-time AI system that automatically detects and sorts batteries 
by type using a conveyor belt — zero human intervention.

## What it does
- Detects battery using YOLOv8 (object detection)
- Classifies type using EfficientNet (18650, 21700, 9V, CoinCell, DCell, LeadAcid)
- ESP32 manages queue and fires correct servo gate
- Arduino UNO controls conveyor motor and IR sensor

## Files
| File | Description |
|---|---|
| battery_sorter_esp32.ino | ESP32 master controller code |
| conveyor_controller.ino  | Arduino UNO conveyor code |
| live_battery_detector.py | Python AI detection script |

## Hardware
- ESP32, Arduino UNO
- 6x SG90 Servo motors
- L298N motor driver + 6V gear motor
- IR sensor, DHT11, HC-SR04, 16x2 LCD

## Tech Stack
Python · PyTorch · OpenCV · YOLOv8 · EfficientNet · C++ · ESP32 · Arduino

## Built by
Nishanth N — Internship Project
