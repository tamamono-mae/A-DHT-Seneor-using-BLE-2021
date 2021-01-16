#include "WiFi.h"
#include "DHT.h"
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
/*
#if !defined(CONFIG_BT_ENABLED) || !defined(CONFIG_BLUEDROID_ENABLED)
#error Bluetooth is not enabled! Please run `make menuconfig` to and enable it
#endif

#define LED_BUILTIN 2
#define LED_EXT_RED 32
#define LED_EXT_GREEN 5
*/
#define DHTPIN 4
#define DHTTYPE DHT22
#define enviornmentService BLEUUID((uint16_t)0x181A)

DHT dht(DHTPIN, DHTTYPE);

bool deviceConnected = false;

BLECharacteristic temperatureCharacteristic(
  BLEUUID((uint16_t)0x2A6E), 
  BLECharacteristic::PROPERTY_READ | 
  BLECharacteristic::PROPERTY_NOTIFY
);
//BLEDescriptor tempDescriptor(BLEUUID((uint16_t)0x2901));

BLECharacteristic humidityCharacteristic(
  BLEUUID((uint16_t)0x2A6F), 
  BLECharacteristic::PROPERTY_READ | 
  BLECharacteristic::PROPERTY_NOTIFY
);

class MyServerCallbacks: public BLEServerCallbacks {
    void onConnect(BLEServer* pServer) {
      deviceConnected = true;
    };

    void onDisconnect(BLEServer* pServer) {
      deviceConnected = false;
    }
};

void setup() {

  // Create the BLE Device
  BLEDevice::init("BLE Seneor");

  // Create the BLE Server
  BLEServer *pServer = BLEDevice::createServer();
  pServer->setCallbacks(new MyServerCallbacks());

  // Create the BLE Service
  BLEService *pEnviornment = pServer->createService(enviornmentService);

  // Create a BLE Characteristic
  pEnviornment->addCharacteristic(&temperatureCharacteristic);
  pEnviornment->addCharacteristic(&humidityCharacteristic);

  // https://www.bluetooth.com/specifications/gatt/viewer?attributeXmlFile=org.bluetooth.descriptor.gatt.client_characteristic_configuration.xml
  // Create a BLE Descriptor
  temperatureCharacteristic.addDescriptor(new BLE2902());
  humidityCharacteristic.addDescriptor(new BLE2902());

  BLEDescriptor TemperatureDescriptor(BLEUUID((uint16_t)0x2901));
  TemperatureDescriptor.setValue("Temperature -40-60°C");
  temperatureCharacteristic.addDescriptor(&TemperatureDescriptor);
  
  BLEDescriptor HumidityDescriptor(BLEUUID((uint16_t)0x2901));
  HumidityDescriptor.setValue("Humidity 0-100 %");
  humidityCharacteristic.addDescriptor(&HumidityDescriptor);

  pServer->getAdvertising()->addServiceUUID(enviornmentService);

  // Start the service
  pEnviornment->start();

  // Start advertising
  pServer->getAdvertising()->start();
  //Serial.println("Waiting a client connection to notify..."); ////
  /*
  pinMode (LED_EXT_RED, OUTPUT);
  pinMode (LED_EXT_GREEN, OUTPUT);
  */
  WiFi.mode(WIFI_OFF);
  pinMode(DHTPIN, INPUT);

  dht.begin();
  
}

void loop() {
  
  delay(1000);
  float h = dht.readHumidity();
  float t = dht.readTemperature();
  float f = dht.readTemperature(true);
  
  if (deviceConnected) {
   int16_t h_value = (100*h);
	 int16_t t_value = (100*t);
    //Serial.println(value);
    // BLETransfer(value);
    temperatureCharacteristic.setValue((uint8_t*)&t_value, 2);
    temperatureCharacteristic.notify();
    humidityCharacteristic.setValue((uint8_t*)&h_value, 2);
    humidityCharacteristic.notify();
  }
  /*
  if(h > 60 || t > 27) {
    //digitalWrite(LED_BUILTIN, LOW);
    digitalWrite(LED_EXT_GREEN, HIGH);
    digitalWrite(LED_EXT_RED, LOW);
    delay(100);
    digitalWrite(LED_EXT_RED, HIGH);
  }
  else {
    digitalWrite(LED_EXT_GREEN, LOW);
    digitalWrite(LED_EXT_RED, HIGH);
    delay(100);
    //digitalWrite(LED_BUILTIN, LOW);
    digitalWrite(LED_EXT_GREEN, HIGH);
  }
  */
}
