/*
 * LGAirConditionerSendDemo.cpp
 *
 *  Sending LG air conditioner IR codes controlled by Serial input
 *  Based on he old IRremote source from https://github.com/chaeplin
 *
 * For Arduino Uno, Nano etc., an IR LED must be connected to PWM pin 3 (IR_SEND_PIN).
 *
 *
 *  This file is part of Arduino-IRremote https://github.com/z3t0/Arduino-IRremote.
 *
 ************************************************************************************
 * MIT License
 *
 * Copyright (c) 2020-2021 Armin Joachimsmeyer
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is furnished
 * to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A
 * PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
 * HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF
 * CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE
 * OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 ************************************************************************************
 */

#include <IRremote.h>
#include "LongUnion.h"

#if defined(__AVR_ATtiny25__) || defined(__AVR_ATtiny45__) || defined(__AVR_ATtiny85__) || defined(__AVR_ATtiny87__) || defined(__AVR_ATtiny167__)
#include "ATtinySerialOut.h"
#endif

// On the Zero and others we switch explicitly to SerialUSB
#if defined(ARDUINO_ARCH_SAMD)
#define Serial SerialUSB
#endif

bool ACIsWallType = false;      // false : TOWER, true : WALL
boolean ACIsHeating = false;    // false : cooling, true : heating
bool ACPowerIsOn = false;
bool ACStateIsAirClean = false; // false : off, 1 : true --> power on
uint8_t ACRequestedFanIntensity = 1;   // 0 : low, 1 : mid, 2 : high - if ACIsWallType==Wall then 3 -> cycle
uint8_t ACRequestedTemperature = 25;    // temperature : 18 ~ 30

const int AC_FAN_TOWER[3] = { 0, 4, 6 };
const int AC_FAN_WALL[4] = { 0, 2, 4, 5 }; // 5 -> cycle

void ACSendCode(uint16_t aCommand) {
    Serial.print("Send code=");
    Serial.print(aCommand, HEX);
    Serial.print(" | ");
    Serial.println(aCommand, BIN);
    IrSender.sendLG((uint8_t) 0x88, aCommand, 0);
}

void sendCommand(uint8_t aTemperature, uint8_t aFanIntensity) {

    Serial.print("Send temperature=");
    Serial.print(aTemperature);
    Serial.print(" fan intensity=");
    Serial.println(aFanIntensity);

    WordUnion tCommand;
    tCommand.UWord = 0;
    if (ACIsHeating) {
        // heating
        tCommand.UByte.HighByte = 0x4; // maybe cooling is 0x08????
    }
    tCommand.UByte.LowByte = ((aTemperature - 15) << 4); // 18 -> 3, 30 -> F

    if (ACIsWallType) {
        tCommand.UByte.LowByte |= AC_FAN_WALL[aFanIntensity];
    } else {
        tCommand.UByte.LowByte |= AC_FAN_TOWER[aFanIntensity];
    }

    ACSendCode(tCommand.UWord);
    ACPowerIsOn = true;
    ACRequestedTemperature = aTemperature;
    ACRequestedFanIntensity = aFanIntensity;
}

void sendAirSwing(bool aSwing) {
    Serial.print("Send air swing=");
    Serial.println(aSwing);
    if (ACIsWallType) {
        if (aSwing) {
            ACSendCode(0x1314);
        } else {
            ACSendCode(0x1315);
        }
    } else {
        if (aSwing) {
            ACSendCode(0x1316);
        } else {
            ACSendCode(0x1317);
        }
    }
}

void SendPowerDown() {
    Serial.println("Send power down");
    IrSender.sendLGRaw(0x88C0051);
    ACPowerIsOn = false;
}

void sendAirClean(bool aStateAirClean) {
    Serial.print("Send air clean=");
    Serial.println(aStateAirClean);
    if (aStateAirClean) {
        ACSendCode(0xC000);
    } else {
        ACSendCode(0xC008);
    }
    ACStateIsAirClean = aStateAirClean;
}

void sendJet(bool aJetOn) {
    Serial.print("Send jet on=");
    Serial.println(aJetOn);
    if (aJetOn) {
        ACSendCode(0x1008);
    } else {
        ACSendCode(0x0834);
    }
}

void printMenu() {
    Serial.println();
    Serial.println();
    Serial.println("Type command and optional parameter without a separator"
            "");
    Serial.println("0 Off");
    Serial.println("1 On");
    Serial.println("s Swing <0 or 1>");
    Serial.println("c Air clean <0 or 1>");
    Serial.println("j Jet <0 or 1>");
    Serial.println("f Fan <0 to 2 or 3 for cycle>");
    Serial.println("t Temperature <18 to 30>");
    Serial.println("+ Temperature + 1");
    Serial.println("- Temperature - 1");
    Serial.println("e.g. \"s1\" or \"t23\" or \"+\"");
    Serial.println();

}

#define SIZE_OF_RECEIVE_BUFFER 10

char sRequestString[SIZE_OF_RECEIVE_BUFFER];

void setup() {
    pinMode(LED_BUILTIN, OUTPUT);

    Serial.begin(115200);
#if defined(__AVR_ATmega32U4__) || defined(SERIAL_USB) || defined(SERIAL_PORT_USBVIRTUAL)  || defined(ARDUINO_attiny3217)
delay(2000); // To be able to connect Serial monitor after reset or power up and before first printout
#endif
// Just to know which program is running on my Arduino
    Serial.println(F("START " __FILE__ " from " __DATE__ "\r\nUsing library version " VERSION_IRREMOTE));
    Serial.print(F("Ready to send IR signals at pin "));
    Serial.println(IR_SEND_PIN);

    /*
     * The IR library setup. That's all!
     * The Output pin is board specific and fixed at IR_SEND_PIN.
     * see https://github.com/Arduino-IRremote/Arduino-IRremote#hardware-specifications
     */
    IrSender.begin(true); // Enable feedback LED,

    delay(1000);

// test
//     sendCommand(25, 1);
//     delay(5000);
//     sendCommand(27, 2);
//     delay(5000);

    printMenu();
}

void loop() {

// Test
//    sendCommand(25, 1);
//    delay(5000);
//    sendCommand(27, 0);
//    delay(5000);

    if (Serial.available()) {
        uint8_t tNumberOfBytesReceived = Serial.readBytesUntil('\n', sRequestString, SIZE_OF_RECEIVE_BUFFER - 1);
        sRequestString[tNumberOfBytesReceived] = '\0'; // terminate as string
        char tCommand = sRequestString[0];
        uint8_t tParameter = 0;
        if (tNumberOfBytesReceived >= 2) {
            tParameter = sRequestString[1] - '0';
        }

        Serial.print("Command=");
        Serial.println(tCommand);

        switch (tCommand) {
        case 0: // off
            SendPowerDown();
            break;
        case 1: // on
            sendCommand(ACRequestedTemperature, ACRequestedFanIntensity);
            break;
        case 's':
            sendAirSwing(tParameter);
            break;
        case 'c': // 1  : clean on, power on
            sendAirClean(tParameter);
            break;
        case 'j':
            sendJet(tParameter);
            break;
        case 'f':
            if (tParameter <= 2) {
                sendCommand(ACRequestedTemperature, tParameter);
            }
            break;
        case 't':
            tParameter = atoi(&sRequestString[1]);
            if (18 <= tParameter && tParameter <= 30) {
                sendCommand(tParameter, ACRequestedFanIntensity);
            }
            break;
        case '+':
            if (18 <= ACRequestedTemperature && ACRequestedTemperature <= 29) {
                sendCommand((ACRequestedTemperature + 1), ACRequestedFanIntensity);
            }
            break;
        case '-':
            if (19 <= ACRequestedTemperature && ACRequestedTemperature <= 30) {
                sendCommand((ACRequestedTemperature - 1), ACRequestedFanIntensity);
            }
            break;
        default:
            Serial.print(F("Error: unknown command \""));
            Serial.print(sRequestString);
            Serial.println('\"');
        }

        if (tParameter != 0) {
            Serial.print("Parameter=");
            Serial.println(tParameter);
        }

        printMenu();
    }
    delay(100);
}

