#include <SPI.h>
#include "DW1000Ranging.h"

#define ANCHOR_ADD "83:17:5B:D5:A9:9A:E2:9C"

#define SPI_SCK 18
#define SPI_MISO 19
#define SPI_MOSI 23
#define DW_CS 4

// Connection pins
const uint8_t PIN_RST = 27;
const uint8_t PIN_IRQ = 33;
const uint8_t PIN_SS = 4;

void setup()
{
    Serial.begin(115200);
    delay(1000);

    // Initialize SPI
    SPI.begin(SPI_SCK, SPI_MISO, SPI_MOSI);

    // Initialize DW1000
    DW1000Ranging.initCommunication(
        PIN_RST,
        PIN_SS,
        PIN_IRQ
    );

    // Callbacks
    DW1000Ranging.attachNewRange(newRange);
    DW1000Ranging.attachBlinkDevice(newBlink);
    DW1000Ranging.attachInactiveDevice(inactiveDevice);

    // Start as anchor
    DW1000Ranging.startAsAnchor(
        ANCHOR_ADD,
        DW1000.MODE_LONGDATA_RANGE_LOWPOWER,
        false
    );
}

void loop()
{
    DW1000Ranging.loop();
}

void newRange()
{
    Serial.print("from: ");

    Serial.print(
        DW1000Ranging
            .getDistantDevice()
            ->getShortAddress(),
        HEX
    );

    Serial.print("\t Range: ");

    Serial.print(
        DW1000Ranging
            .getDistantDevice()
            ->getRange()
    );

    Serial.print(" m");

    Serial.print("\t RX power: ");

    Serial.print(
        DW1000Ranging
            .getDistantDevice()
            ->getRXPower()
    );

    Serial.println(" dBm");
}

void newBlink(DW1000Device *device)
{
    Serial.print("blink; 1 device added ! -> ");

    Serial.print(" short:");

    Serial.println(
        device->getShortAddress(),
        HEX
    );
}

void inactiveDevice(DW1000Device *device)
{
    Serial.print("delete inactive device: ");

    Serial.println(
        device->getShortAddress(),
        HEX
    );
}