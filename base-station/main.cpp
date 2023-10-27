#include <Arduino.h>
const int NUM_PINS = 17;
int pinsToCheck[NUM_PINS] = {4, 5, 13, 14, 15, 16, 17, 18, 19, 21, 22, 23, 25, 26, 27, 32, 33};
int lastPinStatus[NUM_PINS] = {};

void setup()
{
    Serial.begin(115200);
    delay(2000); // Allow some delay for serial to initialize
    Serial.println("GPIOs to check:\n");

    for (int i = 0; i < NUM_PINS; i++)
    {
        pinMode(pinsToCheck[i], INPUT_PULLDOWN); // Set pin as input with pull-up resistor
        lastPinStatus[i] = digitalRead(pinsToCheck[i]);
        Serial.printf("GPIO%d is %s\n", pinsToCheck[i], lastPinStatus[i] == HIGH ? "HIGH" : "LOW");
    }
}

void loop()
{
    for (int i = 0; i < NUM_PINS; i++)
    {
        int currentStatus = digitalRead(pinsToCheck[i]);
        if (currentStatus != lastPinStatus[i])
        {
            Serial.printf("GPIO%d changed to %s\n", pinsToCheck[i], currentStatus == HIGH ? "HIGH" : "LOW");
            lastPinStatus[i] = currentStatus; // Update the last status
        }
        delay(10); // Short delay to avoid reading noise
    }
}
