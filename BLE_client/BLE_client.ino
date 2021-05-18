#define LED_BUILTIN 2
#include "BLEDevice.h"
#include "ArduinoJson.h"
#include "SPIFFS.h"
#include <WiFi.h>
#include "time.h"
//#include "BLEScan.h"

struct tm timeinfo;
struct dataCache{
	uint8_t time_hour;
	uint8_t time_minute;
	float t;
	float h;
};

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

char* ssid;
char* password;
char* ntpServer;
long  gmtOffset_sec = 0;
int   daylightOffset_sec = 0;
const char* config_path = "/config.json";

dataCache dataCache;

float t = 0, h = 0;
uint8_t HourOnDisplay = 0;

unsigned int jsonValueSize(String value){
	return value.length();
}

String Tr_DoW(uint8_t d){
	switch(d){
		case 0:
			return "Sun";
			break;
		case 1:
			return "Mon";
			break;
		case 2:
			return "Tue";
			break;
		case 3:
			return "Wed";
			break;
		case 4:
			return "Thu";
			break;
		case 5:
			return "Fri";
			break;
		case 6:
			return "Sat";
			break;
	}
}

String Tr_mon(uint8_t m){
	switch(m){
		case 0:
			return "Jan";
			break;
		case 1:
			return "Feb";
			break;
		case 2:
			return "Mar";
			break;
		case 3:
			return "Apr";
			break;
		case 4:
			return "May";
			break;
		case 5:
			return "Jun";
			break;
		case 6:
			return "Jul";
			break;
		case 7:
			return "Aug";
			break;
		case 8:
			return "Sep";
			break;
		case 9:
			return "Oct";
			break;
		case 10:
			return "Nov";
			break;
		case 11:
			return "Dec";
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

	switch (tm_hour_range){
		case 0:
			Serial.printf("AM 12:%02d", timeinfo.tm_min);
			break;
		case 1:
			Serial.printf("AM %d:%02d", timeinfo.tm_hour, timeinfo.tm_min);
			break;
		case 2:
			Serial.printf("PM %d:%02d", timeinfo.tm_hour, timeinfo.tm_min);
			break;
		case 3:
			Serial.printf("PM %d:%02d", timeinfo.tm_hour - 12, timeinfo.tm_min);
			break;
	}
	//Serial.write(13);
	Serial.printf(" %s, %s %d  ", Tr_DoW(timeinfo.tm_wday), Tr_mon(timeinfo.tm_mon), timeinfo.tm_mday);
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

	//Read config
	if(!SPIFFS.begin(true)){
		Serial.println("An Error has occurred while mounting SPIFFS");
		return;
  }
	File file = SPIFFS.open("/config.json");
	if(!file){
		Serial.println("There was an error opening the file for writing");
		return;
  }
	if(file){
		unsigned char *data;
		unsigned int size = file.size();
		data = (unsigned char*)malloc(size*sizeof(unsigned char));
		file.read(data, size);
		file.close();
		DynamicJsonDocument doc(size);
		deserializeJson(doc, data);
		free(data);

		const char *jsonData;
		unsigned int fileSize = jsonValueSize(doc[F("ssid")]);
		ssid = (char*)malloc((fileSize+1)*sizeof(char));

		jsonData = doc["ssid"];
		for(unsigned int i=0;i<fileSize;i++)
			ssid[i] = jsonData[i];
		ssid[fileSize] = 0;

		fileSize = jsonValueSize(doc[F("password")]);
		password = (char*)malloc((fileSize+1)*sizeof(char));
		jsonData = doc["password"];
		for(unsigned int i=0;i<fileSize;i++)
			password[i] = jsonData[i];
		password[fileSize] = 0;

		fileSize = jsonValueSize(doc[F("ntpServer")]);
		ntpServer = (char*)malloc((fileSize+1)*sizeof(char));
		jsonData = doc["ntpServer"];
		for(unsigned int i=0;i<fileSize;i++)
			ntpServer[i] = jsonData[i];
		ntpServer[fileSize] = 0;

		gmtOffset_sec = doc["gmtOffset_sec"];
		daylightOffset_sec = doc["daylightOffset_sec"];
	}

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

	dataCache.time_hour = 0;
	dataCache.time_minute = 0;
	dataCache.t = 0;
	dataCache.h = 0;

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
		getLocalTime(&timeinfo);
		/*
		每分鐘充刷一次螢幕
		改變數值時更新螢幕上資料
		*/
		if(dataCache.time_minute != timeinfo.tm_min) {
			digitalWrite(LED_BUILTIN, HIGH);
			Serial.write(27);
			Serial.print("[2J");
			//Esc + [2J
			printLocalTime();
			for (i=0; i<2; i++)
				Serial.write(13);
			Serial.println("Temperature Humidity");
			Serial.printf("  %.2f*C    %.2f",t ,h); Serial.println("%");
			for (i=0; i<6; i++){
				Serial.write(27); Serial.print("[A"); //Up
				delay(2);
			}

			dataCache.time_minute = timeinfo.tm_min;
			dataCache.time_hour = timeinfo.tm_hour;
			dataCache.t = t;
			dataCache.h = h;
		}

		if((dataCache.t != t) || (dataCache.h != h)) {
			digitalWrite(LED_BUILTIN, HIGH);
			for (i=0; i<5; i++){
				Serial.write(27); Serial.print("[B"); //Down
				delay(2);
			}
			Serial.printf("  %.2f*C    %.2f",t ,h); Serial.println("%");
			for (i=0; i<6; i++){
				Serial.write(27); Serial.print("[A"); //Up
				delay(2);
			}
			dataCache.t = t;
			dataCache.h = h;
		}

	}
	else if(doScan){
		BLEDevice::getScan()->start(0);  // this is just eample to start scan after disconnect, most likely there is better way to do it in arduino
	}
	delay(100);
	digitalWrite(LED_BUILTIN, LOW);
} // End of loop
