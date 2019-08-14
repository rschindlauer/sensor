/*

LED MODULE

Copyright (C) 2017 by Xose Pérez <xose dot perez at gmail dot com>

*/

// -----------------------------------------------------------------------------
// METHODS
// -----------------------------------------------------------------------------

void ledOn() {
    digitalWrite(LED_PIN, LOW);
}

void ledOff() {
    digitalWrite(LED_PIN, HIGH);
}

void blink(unsigned int delayms, unsigned char times = 1) {
    for (unsigned char i=0; i<times; i++) {
        if (i>0) delay(delayms);
        ledOn();
        delay(delayms);
        ledOff();
    }
}

void ledSetup() {
    pinMode(LED_PIN, OUTPUT);
    ledOff();
}
