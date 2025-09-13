# Medi Dispense

An **embedded systems project** on the ATmega328P (Arduino Uno) that automates medication dispensing using **Timer1 register interrupts**, **GPIO with debouncing**, and a custom **UART protocol** for communication between two microcontrollers.  

The system allows a nurse (or user) to input a patient ID and two medication selections using binary-encoded button presses. The master Arduino then simulates dispensing by controlling LEDs and logging events, while the slave Arduino drives an LCD display to provide real-time feedback.

## Features
- **Binary-encoded input**: Patient ID (4 buttons) and medication selection (2 × 2 buttons).  
- **Debouncing**: Hardware and software methods used to ensure clean button input.  
- **Timer1 register interrupts**: Configured in CTC mode to generate ~16 ms interrupts for precise, non-blocking timing.  
- **Event tracking**: Tracks dispensing durations, input timing, inactivity timeouts, and post-dispense delays.  
- **UART communication**: Custom protocol structures logs and status messages (`@DISPENSE|ID|MED1,MED2|IN_PROGRESS`, etc.).  
- **Dual microcontroller design**:  
  - **Master (Arduino Uno)** — Handles input, timing, dispensing logic, and communication.  
  - **Slave (Arduino Uno)** — Controls the LCD display for system prompts and status updates.  

## Demo Videos
- [Demo Video 1](https://photos.app.goo.gl/nNaLDPbZv9YzhNyf8)  
- [Demo Video 2](https://photos.app.goo.gl/yyuoFuU44rtTrbd97)

## Authors
Made by **Raiyan Aaijaz** and **Matthew Huynh**
