#include <SPI.h>
#include "DW1000Ranging.h"

// ============================================================
// ESP32 <-> DW1000 PINS
// ============================================================

#define SPI_SCK  18
#define SPI_MISO 19
#define SPI_MOSI 23

#define PIN_RST 27
#define PIN_IRQ 33
#define PIN_SS  4


// ============================================================
// SETUP
// ============================================================

void setup()
{
    Serial.begin(115200);
    delay(2000);

    Serial.println();
    Serial.println("=================================");
    Serial.println("        UWB TAG TEST");
    Serial.println("=================================");

    Serial.println("1: Starting SPI...");

    SPI.begin(SPI_SCK, SPI_MISO, SPI_MOSI);

    Serial.println("2: SPI OK");

    Serial.println("3: Initializing DW1000...");

    DW1000Ranging.initCommunication(
        PIN_RST,
        PIN_SS,
        PIN_IRQ
    );

    Serial.println("4: DW1000 INIT OK");

    Serial.println("5: Attaching range callback...");

    DW1000Ranging.attachNewRange(newRange);

    Serial.println("6: Range callback OK");

    Serial.println("7: Attaching new-device callback...");

    DW1000Ranging.attachNewDevice(newDevice);

    Serial.println("8: New-device callback OK");

    Serial.println("9: Attaching inactive-device callback...");

    DW1000Ranging.attachInactiveDevice(inactiveDevice);

    Serial.println("10: Inactive-device callback OK");

    Serial.println();
    Serial.println("11: Starting TAG...");

    DW1000Ranging.startAsTag(
        "7D:00:22:EA:82:60:3B:9C",
        DW1000.MODE_LONGDATA_RANGE_LOWPOWER
    );

    Serial.println("12: TAG STARTED");

    Serial.println();
    Serial.println("Waiting for anchors...");
    Serial.println("=================================");
}


// ============================================================
// LOOP
// ============================================================

void loop()
{
    DW1000Ranging.loop();
}


// ============================================================
// NEW RANGE
// ============================================================

void newRange()
{
    DW1000Device *device =
        DW1000Ranging.getDistantDevice();

    Serial.print("from: ");

    Serial.print(
        device->getShortAddress(),
        HEX
    );

    Serial.print("\t Range: ");

    Serial.print(
        device->getRange()
    );

    Serial.print(" m");

    Serial.print("\t RX power: ");

    Serial.print(
        device->getRXPower()
    );

    Serial.println(" dBm");
}


// ============================================================
// NEW DEVICE
// ============================================================

void newDevice(DW1000Device *device)
{
    Serial.print("NEW DEVICE: ");

    Serial.println(
        device->getShortAddress(),
        HEX
    );
}


// ============================================================
// INACTIVE DEVICE
// ============================================================

void inactiveDevice(DW1000Device *device)
{
    Serial.print("INACTIVE DEVICE: ");

    Serial.println(
        device->getShortAddress(),
        HEX
    );
}