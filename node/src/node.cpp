#include <RFM69Manager.h>
#include <RFM69.h>
#include <SPI.h>
#include <SPIFlash.h>
#include <Wire.h>
#include <SI7021.h>
#include <LowPower.h>
#include <secrets.h>
//#include <WirelessHEX69.h>

// -----------------------------------------------------------------------------
// Configuration
// -----------------------------------------------------------------------------

#define NODEID              4
#define GATEWAYID           1
#define NETWORKID           100
#define FREQUENCY           RF69_915MHZ
#define IS_RFM69HW          0
#define PERIOD              300     // wakeup interval in seconds
#define DEV_MODE            0       // turns on blinking, serial, and serial prints

#define SERIAL_BAUD         115200

#define LED_PIN             9       // Moteinos have LEDs on D9
#define FLASH_SS            8       // and FLASH SS on D8
#define FLASH_ID            0xEF30  // EF30 for 4mbit Windbond chip (W25X40CL)
#define BATTERYSENSE        A2      // We are reading battery voltage on A2

// -----------------------------------------------------------------------------
// Globals
// -----------------------------------------------------------------------------

SPIFlash flash(FLASH_SS, FLASH_ID);
RFM69Manager radio;
SI7021 sensor;

void blink(byte times, uint16_t mseconds) {
    pinMode(LED_PIN, OUTPUT);
    for (byte i=0; i<times; i++) {
        if (i>0) delay(mseconds);
        digitalWrite(LED_PIN, HIGH);
        delay(mseconds);
        digitalWrite(LED_PIN, LOW);
    }
    pinMode(LED_PIN, INPUT);
}

void radioSend() {
    int batteryReading = analogRead(BATTERYSENSE);
    int temperature = sensor.getFahrenheitHundredths();
    int humidity = sensor.getHumidityPercent();
    #if DEV_MODE
    Serial.print("Temp: "); Serial.println(temperature);
    Serial.print("Hum: "); Serial.println(humidity);
    Serial.print("Batt: "); Serial.println(batteryReading);
    #endif
    char payload[30];
    sprintf(payload, "T=%i;H=%i;B=%i", temperature, humidity, batteryReading);
    radio.send((char *) payload, (uint8_t) 2);
    radio.sleep();
    #if DEV_MODE
    blink(1, 500);
    #endif
}

// -----------------------------------------------------------------------------
// Flash
// -----------------------------------------------------------------------------

void flashSetup() {

    if (flash.initialize()) {
        #if DEV_MODE
        Serial.print("SPI Flash Init OK. Unique MAC = [");
        #endif
        flash.readUniqueId();
        #if DEV_MODE
        for (byte i=0;i<8;i++) {
            Serial.print(flash.UNIQUEID[i], HEX);
            if (i!=8) Serial.print(':');
        }
        Serial.println(']');
        blink(1, 50);
        #endif
    } else {
        #if DEV_MODE
        Serial.println("SPI Flash MEM not found (is chip soldered?)...");
        Serial.println(flash.readDeviceId());
        blink(5, 50);
        #endif
    }
}

// -----------------------------------------------------------------------------
// RFM69
// -----------------------------------------------------------------------------

void radioSetup() {
    delay(10);
    radio.initialize(FREQUENCY, NODEID, NETWORKID, ENCRYPTKEY, GATEWAYID);
    radio.sleep();
}

void radioLoop() {
    // wireless programming token check
    // DO NOT REMOVE, or this node will not be wirelessly programmable any more!
    //CheckForWirelessHEX(radio, flash, true);

    // If we want to listen to incoming packets:
    // radio.loop();

    radioSend();
}

// -----------------------------------------------------------------------------
// Common methods
// -----------------------------------------------------------------------------

void setup() {
    blink(1, 500);
#if DEV_MODE
    Serial.begin(SERIAL_BAUD);
#endif
    sensor.begin();
    flashSetup();
    delay(500);
    blink(1, 500);
    radioSetup();
}

void loop() {
    radioLoop();
    int var = 0;
    int loops = PERIOD / 8;
#if DEV_MODE
    Serial.print("Will sleep 8s for "); Serial.print(loops); Serial.print(" cycles: ");
#endif
    while(var++ < loops) {
#if DEV_MODE
      Serial.print(".");
      Serial.flush();
      blink(1, 10);
#endif
      LowPower.powerDown(SLEEP_8S, ADC_OFF, BOD_OFF);
#if DEV_MODE
      Serial.flush();
#endif
    }
#if DEV_MODE
    Serial.println();
#endif
    // delay(1);
}
