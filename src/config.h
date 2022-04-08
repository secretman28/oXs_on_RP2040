#pragma once

#include <stdint.h>
#define VERSION "0.0.4"
// ------- General ------------------
// This project can be interfaced with an ELRS or a FRSKY receiver (protocol has to be selected accordingly)
// 
// This project is foreseen to generate telemetry data (e.g. when a flight controller is not used) , PWM and Sbus signals
// For telemetry, it can provide
//    - up to 4 analog voltages measurement (with scaling and offset)
//    - the altitude and the vertical speed when connected to a pressure sensor (optional)
//    - GPS data (longitude, latitude, speed, altitude,...) (optional)
//
// It can also provide 8 PWM RC channels (channels 1...4 and 6...9) form a CRSF or a Sbus signal.
// It can also provide SBUS signal (only from CRSF/ELRS signal; for Frsky Sbus is provide by the Frsky Receiver itself)  
//
// -------  Hardware -----------------
// This project requires a board with a RP2040 processor (like the rapsberry pi pico).
// A better alternative is the RP2040-Zero (same processor but smaller board)
// This board can be connected to:
//    - a pressure sensor (GY63 or GY86 board based on MS5611) to get altitude and vertical speed
//    - a GPS from UBlox (like the beitian bn220) or one that support CASIC messages
//       note : a Ublox GPS has to use the default standard config. It will be automatically reconfigure by this firmware
//              a CASIC gps has to be configured before use in order to generate only NAV-PV messages at 38400 bauds
//             this can be done using a FTDI and the program GnssToolkit3.exe (to download from internet)
//    - some voltage dividers (=2 resistors) when the voltages to measure exceed 3V
//       note : a voltage can be used to measure e.g. a current when some external devices are use to generate an analog voltage 
//
// ----------Wiring --------------------
// FRSKY/ELRS receiver, MS5611 and GPS must share the same Gnd
// Connect a 5V source to the Vcc pin of RP2040 board
// When used with a ELRS receiver:
//    - Connect gpio 9 from RP2040 (= PIO RX signal) to the TX pin from ELRS receiver (this wire transmit the RC channels)
//    - Connect gpio 10 from RP2040 (= PIO TX signal) to the RX pin from ELRS receiver (this wire transmits the telemetry data)
// When used with a FRSKY receiver:
//    - Connect gpio 9 from RP2040 (= UART0 RX signal) to the Sbus pin from Frsky receiver (this wire transmit the RC channels)
//    - Connect gpio 10 from RP2040 (= PIO TX signal) via a 1k resistor to the Sport pin from Frsky receiver (this wire transmits the telemetry data)
//
// SBus signal (output based on CRSF) is available on gpio 0
// PWM signals (channels 1...4 and 6...9) are avaialble on gpio 1...8
//     note : channel 5 is used for arming in ELRS but has no real sense for fixed wing. 
//
// Voltages 1...4 are measured on gpio 26...29 
//       Take care to use a voltage divider (2 resistances) in order to limit the voltage on those pins to 3V max 
//
// When a MS5611 (baro sensor) is used:
//       Connect the 3V pin from RP2040 board to the 5V pin of GY63/GY86 
//            Note: do not connect 5V pin of GY63/GY86 to a 5V source because the SDA and SCL would then be at 5V level and would damage the RP2040          
//       Connect SCL to gpio 15 (I2C1)
//       Connect SDA to gpio 14 (I2C1)
//
// When a GPS is used:
//    Connect the 3V pin from RP2040 board to the Vin/5V pin from GPS
//    Connect the RX pin from GPS to gpio 12 (UART0-TX) 
//    Connect the TX pin from GPS to gpio 13 (UART0-RX)
//        
// --------- software -------------------
//    This software has been developped using the RP2040 SDK provided by Rapsberry.
//    It uses as IDE platformio and the WIZIO extension (to be found on internet here : https://github.com/Wiz-IO/wizio-pico )
//    Developers can compile and flash this software with those tools.
//    Still if you just want to use it, there is no need to install/use those tools.
//    On github, in uf2 folder, there is already a compile version of this software that can be directly uploaded and configured afterwards
//    To upload this compiled version, the process is the folowing:
//        - download the file in folder uf2 on your pc
//        - insert the USB cable in the RP2040 board
//        - press on the "boot" button on the RP2040 board while you insert the USB cable in your PC.
//        - this will enter a special bootloader mode and your pc should show a new drive named RPI-RP2
//        - copy and paste the uf2 file to this new drive
//        - the file should be automatically picked up by the RP2040 bootloader and flashed
//        - the RPI_RP2 drive should disapear from the PC and the PC shoud now have a new serial port (COMx on windows)
//        - you can now use a serial terminal (like putty , the one from arduino IDE, ...) and set it up for 115200 baud 8N1
//        - while the RP2040 is connected to the pc with the USB cable, connect this serial terminal to the serial port from the RP2040
//        - when the RP2040 start (or pressing the reset button), it will display the current configuration and the commands to change it.
//        - if you want to change some parameters, fill in the command and press the enter.
//        - the RP2040 should then display the new (saved) config.  
//
// Notes:
// The RP2040 send the telemetry data to the ELRS receiver at some speed.
// This speed (=baud rate) must be the same as the baudrate defined on the receiver
// Usually ELRS receiver uses a baudrate of 420000 to transmit the CRSF channels signal to the flight controller and to get the telemetry data.
// Still, ELRS receivers can be configured to use another baud rate. In this case, change the baudrate in parameters accordingly
//
// The number of data that ELRS can send back per second to the transmitter is quite limitted (and depends on your ELRS setup)
// There are 4 types of frame being generated (voltage, gps, vario and attitude)
// Following #define allow to set up the interval between 2 frames of the same group.
// This is also valid for data sent via the Sport protocol.
// This allows e.g. to transmit vertical speed (in 'vario' frame) more often than GPS data
// The values are in milli seconds
#define VOLTAGE_FRAME_INTERVAL 500 // This version transmit only one voltage; it could be change in the future
#define VARIO_FRAME_INTERVAL 50   // This frame contains only Vertical speed
#define GPS_FRAME_INTERVAL 2000     // This frame contains longitude, latitude, altitude, ground speed, heading and number of satellites
#define ATTITUDE_FRAME_INTERVAL 500 // This should normally contains pitch, roll and yaw. It is currently not used in this project.
// Note: ELRS has only one field for Altitude and is normally part of the GPS frame.  
//       When a baro sensor is used, it provides an altitude that is more accurate than the GPS altitude.
//       So for ELRS protocol, priority is given to the baro altitude when it is available.
//       In ELRS, when there is a baro sensor but no GPS, all GPS data are transmitted but only the Altitude is meaningful  

// Here some additional parameters that can't be changed via the serial terminal 
// -------- Parameters for the vario -----
#define SENSITIVITY_MIN 50
#define SENSITIVITY_MAX 300
#define SENSITIVITY_MIN_AT 100
#define SENSITIVITY_MAX_AT 1000
#define VARIOHYSTERESIS 5

// --------- Parameters for GPS ---------------
#define GPS_REFRESH_RATE 5 // For Ublox GPS, it is possible to select a refresh rate of 1Hz, 5Hz (defeult) or 10Hz 
//                        note :a casic gps has to be configured before use in order to generate only NAV-PV messages at 38400 bauds
//                        this can be done using a FTDI and program GnssToolkit3.exe (to download from internet)

// --------- 10 - Reserved for developer. DEBUG must be activated here when we want to debug one or several functions in some other files. ---------

typedef struct {
  uint8_t available ;
  int32_t value ;
} oneMeasurement_t;



//#define YES 1
//#define NO  0

//#define DEBUG

