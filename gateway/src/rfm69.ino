/*

RFM69 MODULE

Copyright (C) 2016-2017 by Xose Pérez <xose dot perez at gmail dot com>

*/

#include "RFM69Manager.h"

RFM69Manager radio(SPI_CS, IRQ_PIN, IS_RFM69HW, digitalPinToInterrupt(IRQ_PIN));

void processMessage(packet_t * data);

// -----------------------------------------------------------------------------
// RFM69
// -----------------------------------------------------------------------------

void radioSetup() {
    delay(10);

    // Hard reset the RFM module
    pinMode( RFM69_RST, OUTPUT );
    digitalWrite( RFM69_RST, HIGH );
    delay(100);
    digitalWrite( RFM69_RST, LOW );
    delay(100);

    radio.initialize(FREQUENCY, NODEID, NETWORKID, ENCRYPTKEY);
    radio.promiscuous(PROMISCUOUS);
    radio.onMessage(processMessage);
}

void radioLoop() {
    radio.loop();
}
