#define LED_BUILTIN 2
#include "BLEDevice.h"
#include <WiFi.h>
#include "time.h"
//#include "BLEScan.h"

// The remote service we wish to connect to.
static BLEUUID serviceUUID((uint16_t)0x181A);
// The characteristic of the remote service we are interested in.
//static BLEUUID    charUUID((uint16_t)0x2A6F);
static BLEUUID temperatureUUID((uint16_t)0x2A6E);
static BLEUUID humidityUUID((uint16_t)0x2A6F);

static boolean doConnect = false;
static boolean connected = false;
static boolean doScan = false;
//static BLERemoteCharacteristic* pRemoteCharacteristic;
static BLERemoteCharacteristic* temperatureCharacteristic;
static BLERemoteCharacteristic* humidityCharacteristic;
static BLEAdvertisedDevice* BLE_Device;

const char* ssid       = "SSID";
const char* password   = "PASS";
const char* ntpServer = "tock.stdtime.gov.tw";
const long  gmtOffset_sec = 3600*8;
const int   daylightOffset_sec = 0;
struct tm timeinfo;

float t = 0, h = 0;
uint8_t HourOnDisplay = 0;

String Tr_DoW(uint8_t d){
	switch(d){
		case 0:
			return "Sunday";
			break;
		case 1:
			return "Monday";
			break;
		case 2:
			return "Tuesday";
			break;
		case 3:
			return "Wednesday";
			break;
		case 4:
			return "Thursday";
			break;
		case 5:
			return "Friday";
			break;
		case 6:
			return "Saturday";
			break;
	}
}

void printLocalTime(){
	if(!getLocalTime(&timeinfo)){
		Serial.println("Failed to obtain time");
		return;
	}
	uint8_t tm_hour_range = 
	1*((timeinfo.tm_hour > 0)&&(timeinfo.tm_hour < 12)) + 
	2*(timeinfo.tm_hour == 12) +
	3*((timeinfo.tm_hour > 12)&&(timeinfo.tm_hour <= 23));
	Serial.printf("%d/%2d/%2d %s", timeinfo.tm_year + 1900, timeinfo.tm_mon + 1, timeinfo.tm_mday, Tr_DoW(timeinfo.tm_wday));
	Serial.write(13);
	switch (tm_hour_range){
		case 0:
			Serial.printf("AM 12:%02d:%02d", timeinfo.tm_min, timeinfo.tm_sec);
			break;
		case 1:
			Serial.printf("AM %2d:%02d:%02d", timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
			break;
		case 2:
			Serial.printf("PM %2d:%02d:%02d", timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
			break;
		case 3:
			Serial.printf("PM %2d:%02d:%02d", timeinfo.tm_hour - 12, timeinfo.tm_min, timeinfo.tm_sec);
			break;
	}
	Serial.write(13);
}

static void notifyCallback(
	BLERemoteCharacteristic* temperatureCharacteristic,
	uint8_t* pData,
	size_t length,
	bool isNotify) {
	}

class MyClientCallback : public BLEClientCallbacks {
	void onConnect(BLEClient* pclient) {}

	void onDisconnect(BLEClient* pclient) {
		connected = false;
		HourOnDisplay = 0;
		Serial.println("onDisconnect");
	}
};

bool connectToServer() {
	Serial.print("Forming a connection to ");
	Serial.println(BLE_Device->getAddress().toString().c_str());
	
	BLEClient*  pClient  = BLEDevice::createClient();
	Serial.println(" - Created client");
	
	pClient->setClientCallbacks(new MyClientCallback());

	// Connect to the remove BLE Server.
	pClient->connect(BLE_Device);  // if you pass BLEAdvertisedDevice instead of address, it will be 	recognized type of peer device address (public or private)
	Serial.println(" - Connected to server");

	// Obtain a reference to the service we are after in the remote BLE server.
	BLERemoteService* pRemoteService = pClient->getService(serviceUUID);
	if (pRemoteService == nullptr) {
		Serial.print("Failed to find our service UUID: ");
		Serial.println(serviceUUID.toString().c_str());
		pClient->disconnect();
		return false;
	}
	Serial.println(" - Found our service");


	// Obtain a reference to the characteristic in the service of the remote BLE server.
	//pRemoteCharacteristic = pRemoteService->getCharacteristic(charUUID);
	temperatureCharacteristic = pRemoteService->getCharacteristic(temperatureUUID);
	humidityCharacteristic = pRemoteService->getCharacteristic(humidityUUID);
	if (temperatureCharacteristic == nullptr | humidityCharacteristic == nullptr) {
		Serial.println("Failed to find our characteristic UUIDs");
		//Serial.println(charUUID.toString().c_str());
		pClient->disconnect();
		return false;
	}
	Serial.println(" - Found our characteristic");
	
	// Read the value of the characteristic.
	if((temperatureCharacteristic->canRead()) && (humidityCharacteristic->canRead())) {
		
	}
	
	if((temperatureCharacteristic->canNotify()) && (humidityCharacteristic->canNotify())){
		//pRemoteCharacteristic->registerForNotify(notifyCallback);
		temperatureCharacteristic->registerForNotify(notifyCallback);
		humidityCharacteristic->registerForNotify(notifyCallback);
	}
	connected = true;
	return true;
}
/**
 * Scan for BLE servers and find the first one that advertises the service we are looking for.
 */
class MyAdvertisedDeviceCallbacks: public BLEAdvertisedDeviceCallbacks {
 /**
   * Called for each advertising BLE server.
   */
	void onResult(BLEAdvertisedDevice advertisedDevice) {
		//Serial.print("BLE Advertised Device found: ");
		//Serial.println(advertisedDevice.toString().c_str());

    // We have found a device, let us now see if it contains the service we are looking for.
		if (advertisedDevice.haveServiceUUID() && advertisedDevice.isAdvertisingService(serviceUUID)) {

		BLEDevice::getScan()->stop();
		BLE_Device = new BLEAdvertisedDevice(advertisedDevice);
		doConnect = true;
		doScan = true;
		
		} // Found our server
	} // onResult
}; // MyAdvertisedDeviceCallbacks

void updateValues(void){
	std::string valueT = (temperatureCharacteristic->readValue());
	std::string valueH = (humidityCharacteristic->readValue());
	t = (float)(((int16_t)valueT[1] << 8) | (uint16_t)valueT[0])/100;
	h = (float)(((int16_t)valueH[1] << 8) | (uint16_t)valueH[0])/100;
}

void setup() {
	pinMode(LED_BUILTIN, OUTPUT);
	Serial.begin(115200);
	
	//connect to WiFi
	Serial.printf("Connecting to %s ", ssid);
	WiFi.begin(ssid, password);
	while (WiFi.status() != WL_CONNECTED) {
		delay(500);
		Serial.print(".");
	}
	Serial.println(" CONNECTED");
  
	//init and get the time
	configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
	printLocalTime();

	//disconnect WiFi as it's no longer needed
	WiFi.disconnect(true);
	WiFi.mode(WIFI_OFF);
	
	Serial.println("Starting Arduino BLE Client application...");
	BLEDevice::init("");
	
	// Retrieve a Scanner and set the callback we want to use to be informed when we
	// have detected a new device.  Specify that we want active scanning and start the
	// scan to run for 5 seconds.
	BLEScan* pBLEScan = BLEDevice::getScan();
	pBLEScan->setAdvertisedDeviceCallbacks(new MyAdvertisedDeviceCallbacks());
	pBLEScan->setInterval(1349);
	pBLEScan->setWindow(449);
	pBLEScan->setActiveScan(true);
	pBLEScan->start(5, false);
} // End of setup.


// This is the Arduino main loop function.
void loop() {

	// If the flag "doConnect" is true then we have scanned for and found the desired
	// BLE Server with which we wish to connect.  Now we connect to it.  Once we are 
	// connected we set the connected flag to be true.
	if (doConnect == true) {
		if (connectToServer()) {
			Serial.println("Connected.");
		}
		else {
			Serial.println("Failed to connect.");
		}
		doConnect = false;
	}
	
	// If we are connected to a peer BLE Server, update the characteristic each time we are reached
	// with the current time since boot.
	if (connected) {
		//Serial.println("Notify callback for characteristic ");
		uint8_t i;
		updateValues();
		if(HourOnDisplay != timeinfo.tm_hour){
			Serial.write(27);
			Serial.print("[2J");
			HourOnDisplay = timeinfo.tm_hour;
		}
		for (i=0; i<8; i++){
			Serial.write(27); Serial.print("[A"); //Up
			Serial.write(27); Serial.print("[A"); //Up
			Serial.write(27); Serial.print("[A"); //Up
			printLocalTime();
			delay(100);
		}
		Serial.write(13);
		Serial.print("Temperature: "); Serial.print(t); Serial.print("*C");
		Serial.write(13);
		Serial.print("Humidity:    "); Serial.print(h); Serial.println("%");
		digitalWrite(LED_BUILTIN, HIGH);
		for (i=0; i<5; i++){
			Serial.write(27); Serial.print("[A"); //Up
			delay(2);
		}
		digitalWrite(LED_BUILTIN, LOW);	
	}
	else if(doScan){
		BLEDevice::getScan()->start(0);  // this is just eample to start scan after disconnect, most likely there is better way to do it in arduino
	}
	delay(190); // Delay a second between loops.
} // End of loop
