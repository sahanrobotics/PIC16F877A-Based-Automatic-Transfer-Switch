# PIC Microcontroller ATS Controller

This repository contains the C source code for a robust Automatic Transfer Switch (ATS) controller built using a PIC microcontroller. The firmware is designed to monitor a primary power source (mains/CEB) and automatically start a backup generator and transfer the load upon a power failure. It is designed with safety, reliability, and clear user feedback in mind.

## Key Features

- **Automatic Operation**: Fully automates the power transfer process between the Commercial Electric Board (CEB) and a backup generator.
- **Robust State Machine**: The core logic is built on a finite state machine, ensuring predictable and reliable operation through all phases (monitoring, starting, warm-up, transfer, cool-down).
- **Configurable Timers**: All critical timing delays are defined as constants, making it easy to customize for different generator models and site requirements.
  - CEB Fail Verification Delay
  - Generator Crank Time
  - Generator Warm-up Period
  - CEB Return Stability Delay
  - Generator Cool-down Period
- **Failure Detection**: The system can detect and report critical failures, such as the generator failing to start or failing during runtime.
- **Auto-Recovery Logic**: A key safety feature where the system can automatically recover from a generator fault if the mains power returns, immediately switching to the stable source and silencing the alarm.
- **User Interface**: Provides clear, real-time status updates on a 16x2 character LCD, using custom characters for an intuitive display.
- **Audible Alerts**: An onboard buzzer provides audible feedback for critical events like power failure, successful transfer, and system faults.

## System Logic (How it Works)

The controller operates based on a state machine. It continuously reads the status of the CEB and generator power inputs and transitions between states accordingly.

- **`STATE_ON_CEB`**: Normal state. Mains power is active and supplying the load. The system monitors for a mains failure.
- **`STATE_CEB_FAILED`**: Mains power has been lost. A short timer starts to verify the outage is not a transient glitch.
- **`STATE_STARTING_GEN`**: The mains failure is confirmed. The controller engages the generator's starter relay for a defined crank time.
- **`STATE_GEN_WARMING_UP`**: The generator has started successfully. A warm-up timer runs (without load) to allow the engine to stabilize.
- **`STATE_ON_GENERATOR`**: The warm-up is complete. The load is transferred to the generator. The system now monitors for the return of mains power or a generator fault.
- **`STATE_CEB_RETURNED`**: Mains power has been restored. A stability timer begins to ensure the mains supply is stable before transferring the load back.
- **`STATE_GEN_COOLDOWN`**: The load has been transferred back to the mains. The generator runs for a cool-down period (without load) to prevent engine damage. After the timer expires, the system returns to the `STATE_ON_CEB`.
- **`STATE_GEN_START_FAIL`**: The generator failed to start within the allowed crank time. A critical alarm is activated.
- **`STATE_GEN_RUNTIME_FAIL`**: The generator failed while running. A critical alarm is activated.

In either failure state, the system continuously checks for the return of mains power. If mains returns, it will **automatically recover** to `STATE_ON_CEB` and connect the load to the safe power source.

## Hardware Requirements

- **Microcontroller**: A PIC microcontroller compatible with the MPLAB XC8 C Compiler (e.g., PIC16F877A, which this code is well-suited for).
- **Crystal Oscillator**: 8 MHz crystal with appropriate loading capacitors.
- **Display**: Standard 16x2 Character LCD (HD44780 compatible).
- **Relays**:
  - 1x Contactor/Relay for CEB Load
  - 1x Contactor/Relay for Generator Load
  - 1x Relay for Generator Starter Motor
- **Power Sensing**: A circuit to safely detect the presence of AC voltage from both CEB and the generator, providing a logic-level (0V/5V) signal to the microcontroller inputs.
- **Buzzer**: A 5V active buzzer for audible alerts.
- **Power Supply**: A stable 5V DC power supply for the microcontroller and peripherals.

## Pinout Configuration

The firmware is configured with the following pin definitions. These can be easily changed in the source code.

| Pin | Function              | C Macro           |
|-----|-----------------------|-------------------|
| RA0 | CEB/Mains Sense Input | `CEB_ON_INPUT`    |
| RA1 | Generator Sense Input | `GEN_ON_INPUT`    |
| RB0 | Generator Load Relay  | `GEN_RELAY`       |
| RB1 | CEB/Mains Load Relay  | `CEB_RELAY`       |
| RB2 | Generator Start Relay | `GEN_START_RELAY` |
| RD1 | Buzzer Output         | `BUZZER`          |
| RD2 | LCD Register Select   | `RS`              |
| RD3 | LCD Enable            | `EN`              |
| RD4 | LCD Data 4            | `D4`              |
| RD5 | LCD Data 5            | `D5`              |
| RD6 | LCD Data 6            | `D6`              |
| RD7 | LCD Data 7            | `D7`              |

## Software & Setup

1.  **IDE**: [MPLAB X IDE](https://www.microchip.com/en-us/development-tools-and-software/mplab-x-ide)
2.  **Compiler**: [MPLAB XC8 C Compiler](https://www.microchip.com/en-us/development-tools-and-software/mplab-xc-compilers)
3.  **Programmer**: A PIC programmer like PICkit 3, PICkit 4, or MPLAB SNAP.

To compile and flash the firmware:
1.  Create a new project in MPLAB X IDE for your target PIC device.
2.  Add the `main.c` file to the project.
3.  Set the oscillator frequency in the project properties to `8000000` Hz.
4.  Build the project to generate the `.hex` file.
5.  Use the MPLAB IPE (Integrated Programming Environment) or program directly from the IDE to flash the generated `.hex` file to your microcontroller.

## Customization

The operational timing can be easily adjusted by modifying the `#define` constants at the top of the `ATS APPLICATION LOGIC` section in `main.c`.

```c
#define CEB_FAIL_DELAY        3    // Seconds to wait before starting gen
#define GEN_START_CRANK_TIME    7    // Max seconds to crank the engine
#define GEN_WARMUP_TIME         10   // Seconds for engine to warm up
#define CEB_RETURN_DELAY      15   // Seconds to ensure mains is stable
#define GEN_COOLDOWN_TIME       30   // Seconds for engine to cool down
```

## License

This project is licensed under the MIT License. See the [LICENSE](LICENSE) file for details.
