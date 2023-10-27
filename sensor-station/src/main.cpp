/*
  Rui Santos
  Complete project details at https://RandomNerdTutorials.com/esp32-esp-now-wi-fi-web-server/

  Permission is hereby granted, free of charge, to any person obtaining a copy
  of this software and associated documentation files.

  The above copyright notice and this permission notice shall be included in all
  copies or substantial portions of the Software.
*/

#include <esp_now.h>
#include <esp_wifi.h>
#include <WiFi.h>
#include "Adafruit_Si7021.h"

// Set your Board ID (ESP32 Sender #1 = BOARD_ID 1, ESP32 Sender #2 = BOARD_ID 2, etc)
#define BOARD_ID 1

#define LED_PACKET_SENT_PIN 5

Adafruit_Si7021 sensor = Adafruit_Si7021();

// MAC Address of the receiver
uint8_t broadcastAddress[] = {0xB0, 0xA7, 0x32, 0x28, 0x07, 0x80};

// Structure example to send data
// Must match the receiver structure
typedef struct struct_message
{
    int id;
    float temp;
    float hum;
    int readingId;
} struct_message;

// Create a struct_message called myData
struct_message myData;

unsigned long previousMillis = 0; // Stores last time temperature was published
const long interval = 10000;      // Interval at which to publish sensor readings

unsigned int readingId = 0;

// Insert your SSID
constexpr char WIFI_SSID[] = "Bbox-C5E9926D";

int32_t getWiFiChannel(const char *ssid)
{
    if (int32_t n = WiFi.scanNetworks())
    {
        for (uint8_t i = 0; i < n; i++)
        {
            if (!strcmp(ssid, WiFi.SSID(i).c_str()))
            {
                return WiFi.channel(i);
            }
        }
    }
    return 0;
}

float readTemperature()
{
    // Sensor readings may also be up to 2 seconds 'old' (its a very slow sensor)
    // Read temperature as Celsius (the default)
    float t = sensor.readTemperature();
    // Read temperature as Fahrenheit (isFahrenheit = true)
    // float t = dht.readTemperature(true);
    // Check if any reads failed and exit early (to try again).
    if (isnan(t))
    {
        Serial.println("Failed to read from sensor!");
        return 0;
    }
    else
    {
        // Serial.println(t);
        return t;
    }
}

float readHumidity()
{
    float h = sensor.readHumidity();
    if (isnan(h))
    {
        Serial.println("Failed to read from sensor!");
        return 0;
    }
    else
    {
        // Serial.println(h);
        return h;
    }
}

// callback when data is sent
void OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status)
{
    Serial.print("\r\nLast Packet Send Status:\t");
    Serial.println(status == ESP_NOW_SEND_SUCCESS ? "Delivery Success" : "Delivery Fail");
    if (status == ESP_NOW_SEND_SUCCESS)
    {
        digitalWrite(LED_PACKET_SENT_PIN, HIGH);
        delay(100);
        digitalWrite(LED_PACKET_SENT_PIN, LOW);
    }
}

esp_now_peer_info_t peerInfo;

void reduce_cpu_frequency()
{
    uint32_t XTAL_FREQ = getXtalFrequencyMhz();
    switch (XTAL_FREQ)
    {
    case 40:
        setCpuFrequencyMhz(40);
        break;
    case 26:
        setCpuFrequencyMhz(26);
        break;

    case 24:
        setCpuFrequencyMhz(24);
        break;
    }
}

void setup()
{
    // Init Serial Monitor
    Serial.begin(115200);

    uint32_t XTAL_FREQ = getXtalFrequencyMhz();
    Serial.println("XTAL_FREQ: " + String(XTAL_FREQ));
    // reduce_cpu_frequency();
    // setCpuFrequencyMhz(10);

    uint32_t CPU_FREQ = getCpuFrequencyMhz();
    Serial.println("CPU_FREQ: " + String(XTAL_FREQ));
    pinMode(LED_PACKET_SENT_PIN, OUTPUT);
    if (!sensor.begin())
    {
        Serial.println("Did not find Si7021 sensor!");
        while (true)
            ;
    }
    Serial.println("Si7021 sensor found!");

    // Set device as a Wi-Fi Station and set channel
    WiFi.mode(WIFI_STA);

    int32_t channel = getWiFiChannel(WIFI_SSID);

    WiFi.printDiag(Serial); // Uncomment to verify channel number before
    esp_wifi_set_promiscuous(true);
    esp_wifi_set_channel(channel, WIFI_SECOND_CHAN_NONE);
    esp_wifi_set_promiscuous(false);
    WiFi.printDiag(Serial); // Uncomment to verify channel change after

    // Init ESP-NOW
    if (esp_now_init() != ESP_OK)
    {
        Serial.println("Error initializing ESP-NOW");
        return;
    }

    // Once ESPNow is successfully Init, we will register for Send CB to
    // get the status of Trasnmitted packet
    esp_now_register_send_cb(OnDataSent);

    // Register peer

    memcpy(peerInfo.peer_addr, broadcastAddress, 6);
    peerInfo.encrypt = false;

    // Add peer
    if (esp_now_add_peer(&peerInfo) != ESP_OK)
    {
        Serial.println("Failed to add peer");
        return;
    }
}

void loop()
{
    unsigned long currentMillis = millis();
    if (currentMillis - previousMillis >= interval)
    {
        // Save the last time a new reading was published
        previousMillis = currentMillis;
        // Set values to send
        myData.id = BOARD_ID;
        myData.temp = readTemperature();
        myData.hum = readHumidity();
        myData.readingId = readingId++;

        // Send message via ESP-NOW
        esp_err_t result = esp_now_send(broadcastAddress, (uint8_t *)&myData, sizeof(myData));
        // if (result == ESP_OK)
        // {
        //     Serial.println("Sent with success");
        // }
        // else
        // {
        //     Serial.println("Error sending the data");
        // }
    }
    // Wait a bit before going to sleep for printing to serial
    delay(interval - (millis() - previousMillis));
    // Wait a bit before scanning again

    // Real sleep seems to break esp-now
    // esp_sleep_enable_timer_wakeup((interval - (millis() - previousMillis)) * 1000);
    // esp_light_sleep_start();
}