#include <RFM69Manager.h>
#include <RFM69.h>
#include <SPI.h>
#include <SPIFlash.h>
#include <Wire.h>
#include <LowPower.h>
#include <secrets.h>
//#include <WirelessHEX69.h>

// -----------------------------------------------------------------------------
// Configuration
// -----------------------------------------------------------------------------

#define NODEID              80
#define GATEWAYID           1
#define NETWORKID           100
#define FREQUENCY           RF69_915MHZ
#define IS_RFM69HW          0
#define DEV_MODE            0       // turns on blinking, serial, and serial prints

#define SERIAL_BAUD         115200

#define LED_PIN             9       // Moteinos have LEDs on D9
#define FLASH_SS            8       // and FLASH SS on D8
#define FLASH_ID            0xEF30  // EF30 for 4mbit Windbond chip (W25X40CL)
#define BATTERYSENSE        A2      // We are reading battery voltage on A2

#define MOTION_PIN     3  // D3
#define MOTION_IRQ     1  // hardware interrupt 1 (D3) - where motion sensors OUTput is connected, this will generate an interrupt every time there is MOTION
#define DUPLICATE_INTERVAL 10000
#define BATT_INTERVAL  300000  // read and report battery voltage every this many ms (approx)

// -----------------------------------------------------------------------------
// Globals
// -----------------------------------------------------------------------------

SPIFlash flash(FLASH_SS, FLASH_ID);
RFM69Manager radio;

volatile boolean motionDetected=false;
unsigned int batteryReading;

uint32_t batteryLastReadingTime = 0;
uint32_t motionLastDetectionTime = 0;
uint32_t time = 0, now = 0;
byte motionRecentlyCycles=0;

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

void motionIRQ() {
    motionDetected = true;
    #if DEV_MODE
        Serial.println("IRQ");
    #endif
}

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
        flash.sleep();
    } else {
        #if DEV_MODE
        Serial.println("SPI Flash MEM not found (is chip soldered?)...");
        Serial.println(flash.readDeviceId());
        blink(5, 50);
        #endif
    }
}

void radioSetup() {
    delay(10);
    radio.initialize(FREQUENCY, NODEID, NETWORKID, ENCRYPTKEY, GATEWAYID);
    radio.sleep();
}

// -----------------------------------------------------------------------------
// Common methods
// -----------------------------------------------------------------------------

void setup() {
    blink(1, 500);
    #if DEV_MODE
        Serial.begin(SERIAL_BAUD);
        Serial.println("Starting serial");
    #endif
    flashSetup();
    delay(500);
    blink(1, 500);
    radioSetup();
    blink(1, 500);
    pinMode(MOTION_PIN, INPUT);
    attachInterrupt(MOTION_IRQ, motionIRQ, RISING);
    blink(1, 500);
    #if DEV_MODE
        Serial.println("Setup completed.");
        Serial.flush();
    #endif
}


void loop() {
    now = millis();

    // is it time to send a battery measurement?
    if (time - batteryLastReadingTime > BATT_INTERVAL)
    {
        unsigned int readings = 0;
        batteryLastReadingTime = time;

        // take samples, and average
        for (byte i = 0; i < 5; i++)
            readings += analogRead(BATTERYSENSE);

        batteryReading = readings / 5.0;

        #if DEV_MODE
            Serial.print("Battery: "); Serial.println(batteryReading);
        #endif

        batteryLastReadingTime = time;

        char payload[30];
        sprintf(payload, "B=%i", batteryReading);
        radio.send((char *) payload, (uint8_t) 2);
        radio.sleep();

        // the battery reading/reporting could have triggered the PIR
        // (see LowPowerLab forum, could be VCC swings), so just reset the flag
        // here; if there is motion it will be set right away again:
        delay(10);
        motionDetected = false;

        #if DEV_MODE
            blink(1, 500);
        #endif
    }

    // is there new motion to report?
    if (motionDetected && (time - motionLastDetectionTime > DUPLICATE_INTERVAL))
    {
        motionLastDetectionTime = time;

        #if DEV_MODE
            Serial.println("Motion detected");
        #endif

        char payload[30];

        sprintf(payload, "MOT=1");
        radio.send((char *) payload, (uint8_t) 2);
        radio.sleep();

        #if DEV_MODE
            blink(1, 500);
        #endif
    }

    // while motion recently happened sleep for small slots of time to better
    // approximate last motion event; this helps with debouncing a "MOTION"
    // event more accurately for sensors that fire the IRQ very rapidly (ie
    // panasonic sensors)
    if (motionDetected || motionRecentlyCycles > 0)
    {
        if (motionDetected)
            motionRecentlyCycles = 8;
        else
            motionRecentlyCycles--;

        motionDetected = false; //do NOT move this after the SLEEP line below or motion will never be detected

        // update time explicitly, as millis() will not be accurate with powerDown
        time = time + 250 + millis() - now;
        radio.sleep();
        LowPower.powerDown(SLEEP_250MS, ADC_OFF, BOD_OFF);
    }
    else
    {
        // update time explicitly, as millis() will not be accurate with powerDown
        time = time + 8000 + millis() - now;
        radio.sleep();
        LowPower.powerDown(SLEEP_8S, ADC_OFF, BOD_OFF);
    }
    #if DEV_MODE
        Serial.flush();
    #endif
}
