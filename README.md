STM32MP157C-DK2 Distance Monitoring System

This project implements a heterogeneous embedded system on the STM32MP157C-DK2, using both cores for their intended purposes.

Cortex-M4 handles real-time sensor acquisition
Cortex-A7 (Linux) handles display and application-level processing

An HC-SR04 ultrasonic sensor is interfaced with the M4, which generates the trigger pulse and measures the echo response using microsecond-level timing via the DWT cycle counter. Distance values are transmitted to the Linux side through OpenAMP RPMsg, enabling inter-processor communication over shared memory.

On the A7, a Python GTK application runs fullscreen on the board’s touchscreen and displays live distance readings with color-based feedback, allowing the system to operate independently without an external device.

What This Project Covers
Hardware interfacing (HC-SR04 + voltage divider)
Real-time firmware development (Cortex-M4)
Embedded Linux bring-up (Cortex-A7)
Inter-processor communication (OpenAMP / RPMsg)
Application-level display using GTK
Documentation and Demo
Full technical write-up:
Demo video: https://youtu.be/8nIt7zsRcJg?si=upIalE33QjS0QFfQ
