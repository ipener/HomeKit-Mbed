# HomeKit Bluetooth® LE 5 Library for Mbed OS

## Overview
This is a Bluetooth Low Energy (BLE) implementation of the [HomeKit Accessory Protocol (HAP)](https://developer.apple.com/homekit/faq/) for [Mbed OS](https://github.com/ARMmbed/mbed-os). It uses the open source [HomeKit Accessory Development Kit (ADK)](https://github.com/apple/HomeKitADK) with minor patches to utilize [ARM® TrustZone® CryptoCell-310](https://infocenter.nordicsemi.com/index.jsp?topic=%2Fps_nrf9160%2Fcryptocell.html), a hardware-accelerated cryptography subsystem on the [nRF52840](https://www.nordicsemi.com/Products/nRF52840) SoC. While it should work on all boards that support these features, it has only been tested on the [Arduino Nano 33 BLE](https://docs.arduino.cc/hardware/nano-33-ble) and [Arduino Nano 33 BLE Sense](https://docs.arduino.cc/hardware/nano-33-ble-sense) boards.

## Getting Started
This project was developed on macOS so please use common sense to follow similar instructions on Windows/Linux.

1. Download [Mbed Studio](https://os.mbed.com/studio/), open a *Terminal* inside it, and make sure you're in the root directory, i.e. `Mbed Programs/` if you chose the default name
2. Since this project makes use of git submodules, don't manually download the repository but run `git clone https://github.com/ipener/HomeKit-Mbed` instead
3. Run `./install.sh`, which adds and initializes the **HomeKitADK** (commit [fb201f9](https://github.com/apple/HomeKitADK/commit/fb201f98f5fdc7fef6a455054f08b59cca5d1ec8)) and **mbed-os** (tag [mbed-os-6.15.1](https://github.com/ARMmbed/mbed-os/releases/tag/mbed-os-6.15.1)) submudules and applies the necessary patches
4. Select **HomeKit-Mbed** as the active program, make sure to choose the **ARDUINO_NANO33BLE** target and trigger the build

> Note: If unable to execute the shell script, run `chmod u+x ./install.sh` to obtain the execute permissions.

## Flashing the Board
Enable *bootloader mode* on your Arduino by double-tapping the reset button. You should see the yellow LED fading in and out indefinetly. To deploy the program to your Arduino, run the following command in a *Terminal*:
```sh
"{path-to-arduino-bossac}/bossac" -d --port={port-name}  -U -i -e -w "HomeKit-Mbed/BUILD/ARDUINO_NANO33BLE/ARMC6/HomeKit-Mbed.bin" -R
```
 To find out the path to the `bossac` binary as well as the port, simply open the *Arduino IDE* and flash any sketch to your Arduino. You should see something like the following line in the output console:
 ```sh
 "/Users/{username}/Library/Arduino15/packages/arduino/tools/bossac/1.9.1-arduino2/bossac" -d --port=cu.usbmodem143201  -U -i -e -w "{path-to-sketch-ino}.bin" -R
 ```
> Note: Make sure no other process such as `screen` or the *Serial Monitor* is monitoring the serial connection.

## Connecting to an iOS Device
After flashing the program on to the board and pushing the reset button you should be able to connect it to an iOS/iPadOS device.
1. Turn on Bluetooth and open Apple's [Home](https://apps.apple.com/us/app/home/id1110145103) app
2. Tap on *Add Accessory* and select *More options...*
3. You should see **Acme Light Bulb** listed as a nearby accessory
4. Tap on it and wait for an alert to appear, stating that this is an uncertified accessory
5. Tap on *Add Anyway* and enter the following 8-digit setup code **111 22 333**
6. Wait for the pair setup and verification procedure to complete
7. Tap *Continue* on the card giving you the option to change the accessory's name
8. Tap *Continue* on the card giving you the option to select a room for the accessory
9. Tap *Done* on the final screen
10. You can now turn the light bulb on or off using this iOS device

To remove the accessory, long press on the card and select *Remove Accessory* from the bottom of the displayed options.
> Note: You can specify your own setup code by changing the `HAP_SETUP_CODE` macro in [mbed_app.json](./mbed_app.json).

## Logging and Debugging
The easiest way to debug the Arduino without external microcontrollers is to enable logging in [mbed_app.json](./mbed_app.json) by setting `HAP_LOG_LEVEL` to `1`, `2` or `3`. Sensitive logs such as the generated private keys can be enabled by setting `HAP_LOG_SENSITIVE` to `1`.
To view the logs, first make sure the board is reset after flashing, i.e. only the green LED is on, then use the following command inside a *Terminal* in Mbed Studio to [open a serial terminal](https://os.mbed.com/docs/mbed-os/v6.15/program-setup/serial-communication.html#viewing-output-in-a-serial-terminal):
```sh
mbed sterm -p /dev/cu.usbmodem143201 -b 115200
```
Alternatively, you can use the following command:
```sh
screen /dev/cu.usbmodem143201 115200
```
Lastly, you can also use *Serial Monitor* inside the Arduino IDE but it doesn't support colored text.

> Note: The [`USBDevice`](https://github.com/ARMmbed/mbed-os/blob/48b1b8ec7801641498f9a4622398bf0dd9ce6f25/drivers/usb/source/USBDevice.cpp) implementation hardcodes the manufacturer name, serial number, etc. which can change the USB device descriptor when running your binary. This simply means that before running `screen` or `mbed sterm` you have to find out the port name again. Alternatively, if you only have one accessory connected to your computer, you can specify the port as `/dev/cu.usb*`.

In addition, you can inspect all Host Controller Interface (HCI) events/commands and Attribute Protocol (ATT) requests/responses using Apple's *PacketLogger* tool. For that, you need to have an Apple Developer Account, download these [iOS profiles](https://developer.apple.com/bug-reporting/profiles-and-logs/?name=bluetooth) on your iOS device and follow the instructions in this [official blog post](https://www.bluetooth.com/blog/a-new-way-to-debug-iosbluetooth-applications/).

## Key-Value Store
The HAP specification requires an accessory to persist information such as cryptographic keys, accessory state, etc. across reboots. This implementation uses the Mbed OS [kvstore_global_api](https://os.mbed.com/docs/mbed-os/v6.15/apis/static-global-api.html) to persist key-value pairs in the internal flash memory. However, writing and erasing wears out flash memory over time. The nrf52840 SoC can handle about 10000 write/erase cycles which should be plenty for a few years of standard operation. If this is still a concern for you, I recommend modifying the [HAPPlatformKeyValueStore.cpp](./HAPPlatformKeyValueStore.cpp) implementation and keep all information in-memory (RAM). The downside is that you'd have to go through the pair-setup procedure every time you reboot the Arduino.

When flashing the board with a much greater binary than before, the previously stored key-value pairs can become currupted so it's best to reset the data with this simple program:
```c
#include "kvstore_global_api.h"

int main(void) {
    return kv_reset(MBED_CONF_NANOSTACK_HAL_KVSTORE_PATH);
}
```
The *HomeKitADK* doesn't implement a default accessory reset function but you can easily add somthing like the following code to [HomeKitADK/Applications/Main.c](./HomeKitADK/Applications/Main.c):
```c
void ResetAccessory(void) {
    requestedFactoryReset = true;

    HAPAccessoryServerStop(&accessoryServer);
}
```
The function can be invoked as a result of pressing a button or writing to a custom Generic Attribute Profile (GATT) characteristic.

## Adding HAP Services and Characteristics
By default, this implementation sets up a simple HAP *Light Bulb* service with one *On* characteristic. You can modify the accessory's behavior following the HomeKit ADK examples in the [HomeKitADK/Applications](https://github.com/apple/HomeKitADK/tree/master/Applications) directory. However, should `kAttributeCount` exceed `32`, make sure to increase the value of the following configuration entry in [mbed_app.json](./mbed_app.json):
```json
"ble-api-implementation.max-characteristic-authorisation-count": 32
```
Keep in mind that all HAP services and charactersitics must comply with the HomeKit ADK for the accessory to even start advertising using the Bluetooth Generic Access Profile (GAP).

## Example Dimmer Application
If you're interested in trying out an out-of-the-box implementation for a 3-channel dimmer, you can run the following commands in a *Terminal* from the [HomeKit-Mbed](./) directory:
```sh
git apply patches/HomeKit-Mbed-3-lights.patch
cd HomeKitADK
git apply ../patches/HomeKitADK-3-lights.patch
```
This adds a *Brightness* characteristic to the existing HAP *Light Bulb* service and links two additional services to it. The files in the [HomeKitADK/Applications/Lightbulb](./HomeKitADK/Applications/Lightbulb) directory are modified to use an interrupt service routine (ISR) for detecting a zero-cross and a hardware timer `TIMER3_IRQn` to periodically trigger digital on/off signals depending on the desired brightness levels. The implementation can be used with any 3.3V, 50/60 Hz AC dimmer module and has been tested with a [RobotDyn® 4-channel AC dimmer](https://robotdyn.com/ac-light-dimmer-module-4-channel-3-3v-5v-logic-ac-50-60hz-220v-110v.html). Connect the Arduino as follows:

| Arduino | Dimmer |
|---|---|
| GND | GND |
| 3.3V | VCC |
| D2 | Z-C |
| D3 | D1 |
| D4 | D2 |
| D5 | D3 |

> Note: Comment out `#define NETWORK_FREQUENCY_50HZ` if your network frequency is 60Hz or leave it as is for 50Hz.

Lastly, I recommend to disable all asserts and preconditions to reduce the binary size once you decide to install your accessory. For that, set the `HAP_DISABLE_ASSERTS` and `HAP_DISABLE_PRECONDITIONS` macros in [mbed_app.json](./mbed_app.json) to `1`.