This project works with the STM32MP157C-DK2, using both processing cores of the board
for their intended purposes, the Cortex-M4 handling real time sensor acquisition and the
Cortex-A7 running Linux for application-level display and networking.
I used an HC-SR04 ultrasonic distance sensor interfaced with the Cortex-M4, which handles
the precise timing required to generate trigger pulses and measure echo responses. Measured
distance values are sent to the Linux side via OpenAMP RPMsg, an inter processor communication framework built on shared memory. On the Linux side, I first wanted to make
a display interface of a web based dashboard over USB networking but I then thought it
would make more sense to display it directly on the boards built in display so I developed a
small GTK application to do it.
The project spans hardware design, real time firmware development, embedded Linux bring
up, inter processor communication, and application level software serving as a demonstration
of a modern heterogeneous embedded system.

For more detailed documentation see docs folder or for a video see this link https://youtu.be/8nIt7zsRcJg?si=upIalE33QjS0QFfQ
