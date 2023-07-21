
#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>
#include <Preferences.h>

#include "RGB.h"

Preferences preferences;

// LED GLOBAL CONFIG
RGB currentColor, targetColor;

#define RED_PIN 23
#define GREEN_PIN 22
#define BLUE_PIN 21

#define RED_CHANNEL 0
#define GREEN_CHANNEL 1
#define BLUE_CHANNEL 2

#define LED_PWM_FREQ 5000
#define LED_CHANNEL_RES 8
#define PROG_PER_TICK 0.01f

bool transition = false;
float interpolationProgress, currentBrightness;

void setLEDsToRGB(RGB colour)
{
  ledcWrite(RED_CHANNEL, colour.r * currentBrightness);
  ledcWrite(GREEN_CHANNEL, colour.g * currentBrightness);
  ledcWrite(BLUE_CHANNEL, colour.b * currentBrightness);
}

// BLUETOOTH LE GLOBAL CONFIG
#define SERVICE_UUID "be0bae1d-3625-4362-81e3-3b96f709bd66"
#define COLOR_CHARACTERISTIC_UUID "d6d92a13-44a1-4fe9-a868-e9a3ebaeafed"
#define BRIGHTNESS_CHARACTERISTIC_UUID "3ce1e5b3-16fb-473b-85b4-e3389ca4558c"
#define BLE_RW_PROPERTIES (BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_WRITE)

BLEServer *pServer;
BLECharacteristic *pColorCharacteristic, *pBrightnessCharacteristic;

bool deviceConnected, oldDeviceConnected;

class ServerCallbacks : public BLEServerCallbacks
{
  void onConnect(BLEServer *pServer)
  {
    deviceConnected = true;
    pServer->startAdvertising(); // restart advertising
  };

  void onDisconnect(BLEServer *pServer)
  {
    deviceConnected = false;
  }
};

class ColorCallbacks : public BLECharacteristicCallbacks
{
  void onWrite(BLECharacteristic *pCharacteristic)
  {
    BLEColorData *rawData = (BLEColorData *)pCharacteristic->getData();
    targetColor = RGB(rawData->r, rawData->g, rawData->b);
    preferences.putBytes("lastColor", rawData, sizeof(BLEColorData));
    transition = true;
#ifdef DEBUG
    Serial.printf("COLOR: [ R: %d, G: %d, B: %d ]\n", rawData->r, rawData->g, rawData->b);
#endif
  }
};

class BrightnessCallbacks : public BLECharacteristicCallbacks
{
  void onWrite(BLECharacteristic *pCharacteristic)
  {
    uint16_t brightnessValue = *(uint16_t *)pCharacteristic->getData();
    if (brightnessValue > 100)
      brightnessValue = 100;
    currentBrightness = brightnessValue / 100.0f;
    preferences.putFloat("lastBrightness", currentBrightness);
    setLEDsToRGB(currentColor);
#ifdef DEBUG
    Serial.printf("BRIGHTNESS: %u%%\n", (uint)brightnessValue);
#endif
  }
};

void setup()
{
#ifdef DEBUG
  Serial.begin(115200);
#endif
  preferences.begin("rgbstrip", false);

  // LED setup
  ledcSetup(RED_CHANNEL, LED_PWM_FREQ, LED_CHANNEL_RES);
  ledcAttachPin(RED_PIN, RED_CHANNEL);
  ledcSetup(GREEN_CHANNEL, LED_PWM_FREQ, LED_CHANNEL_RES);
  ledcAttachPin(GREEN_PIN, GREEN_CHANNEL);
  ledcSetup(BLUE_CHANNEL, LED_PWM_FREQ, LED_CHANNEL_RES);
  ledcAttachPin(BLUE_PIN, BLUE_CHANNEL);

  // Initialize Color
  BLEColorData initialColorBuffer;
  preferences.getBytes("lastColor", &initialColorBuffer, sizeof(BLEColorData));
  currentColor = RGB::fromBLEColorData(initialColorBuffer);

  currentBrightness = preferences.getFloat("lastBrightness", 0.5f);
  uint16_t initialBrightnessValue = (uint16_t)ceilf(currentBrightness * 100);

#ifdef DEBUG
  Serial.printf("INITIAL_COLOR: [ R: %d, G: %d, B: %d ]\n", currentColor.r, currentColor.g, currentColor.b);
  Serial.printf("INITIAL_BRIGHTNESS: %u%%\n", (uint)initialBrightnessValue);
#endif

  // BLE Server setup
  BLEDevice::init("RGB Strip");
  pServer = BLEDevice::createServer();
  pServer->setCallbacks(new ServerCallbacks());
  BLEService *pService = pServer->createService(SERVICE_UUID);

  // BLE Color Characteristic
  pColorCharacteristic = pService->createCharacteristic(COLOR_CHARACTERISTIC_UUID, BLE_RW_PROPERTIES);
  pColorCharacteristic->setCallbacks(new ColorCallbacks());
  pColorCharacteristic->setValue((uint8_t *)&initialColorBuffer, sizeof(BLEColorData));

  // BLE Brightness Characteristic
  pBrightnessCharacteristic = pService->createCharacteristic(BRIGHTNESS_CHARACTERISTIC_UUID, BLE_RW_PROPERTIES);
  pBrightnessCharacteristic->setCallbacks(new BrightnessCallbacks());
  pBrightnessCharacteristic->setValue(initialBrightnessValue);

  pService->start();
  pServer->startAdvertising();
}

void loop()
{
  if (deviceConnected && transition)
  {
    RGB nextColorStep = RGB::interpolate(&currentColor, &targetColor, interpolationProgress);
    setLEDsToRGB(nextColorStep);
    interpolationProgress += PROG_PER_TICK;
    if (nextColorStep.equals(&targetColor) || interpolationProgress >= 1.0)
    {
      transition = false;
      currentColor = targetColor;
      interpolationProgress = 0;
    }
    delay(50);
  }
  // disconnecting
  if (!deviceConnected && oldDeviceConnected)
  {
    // give the bluetooth stack the chance to get things ready
    delay(500);
    // restart advertising
    pServer->startAdvertising();
    oldDeviceConnected = deviceConnected;
  }
  // connecting
  if (deviceConnected && !oldDeviceConnected)
  {
    oldDeviceConnected = deviceConnected;
  }
}