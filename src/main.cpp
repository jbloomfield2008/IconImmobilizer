//
// A simple server implementation showing how to:
//  * serve static messages
//  * read GET and POST parameters
//  * handle missing pages / 404s
//

#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>
#include <BLEBeacon.h>
#include <WiFi.h>
#include <WebServer.h>
#include <uri/UriBraces.h>
#include <FS.h>
#include <SPIFFS.h>
#include <set>
#include "mcbcomm.h"

#define ENDIAN_CHANGE_U16(x) ((((x)&0xFF00) >> 8) + (((x)&0xFF) << 8))
#define RX 12
#define TX 13
#define CTRL_MODE 0xa06
#define RPM 0x22e
#define TORQUE_MODE 1
#define SPEED_MODE 2
#define SAFE_RPM 100
#define SCAN_TIME 2

BLEScan* pBLEScan;
bool found = false;
uint16_t mode = 0;
WebServer server(80);
// Set your access point network credentials
const char* ssid = "IconLive";
const char* password = "123456789";
FS* filesystem = &SPIFFS;
HardwareSerial mySerial(1);
MCB controller;
std::set<BLEAddress> allowed;

class MyAdvertisedDeviceCallbacks: public BLEAdvertisedDeviceCallbacks {
    void onResult(BLEAdvertisedDevice device) {
        if (found == false && device.getRSSI() > -85)
        {
          found = allowed.count(device.getAddress()) > 0;
          if (found == false && device.haveManufacturerData() == true)
          {
            std::string strManufacturerData = device.getManufacturerData();

            uint8_t cManufacturerData[100];
            strManufacturerData.copy((char *)cManufacturerData, strManufacturerData.length(), 0);

            if (strManufacturerData.length() == 25 && cManufacturerData[0] == 0x4C && cManufacturerData[1] == 0x00)
            {
              BLEBeacon oBeacon = BLEBeacon();
              oBeacon.setData(strManufacturerData);
              uint16_t major = ENDIAN_CHANGE_U16(oBeacon.getMajor());
              uint16_t minor = ENDIAN_CHANGE_U16(oBeacon.getMinor());
              found = major == 94 && minor == 87;
            }
          }
        }
    }
};

void array_to_string(byte array[], unsigned int len, char buffer[])
{
    for (unsigned int i = 0; i < len; i++)
    {
        byte nib1 = (array[i] >> 4) & 0x0F;
        byte nib2 = (array[i] >> 0) & 0x0F;
        buffer[i*2+0] = nib1  < 0xA ? '0' + nib1  : 'A' + nib1  - 0xA;
        buffer[i*2+1] = nib2  < 0xA ? '0' + nib2  : 'A' + nib2  - 0xA;
    }
    buffer[len*2] = '\0';
}

String getContentType(String filename) {
  if (filename.endsWith(".htm")) {
    return "text/html";
  } else if (filename.endsWith(".html")) {
    return "text/html";
  } else if (filename.endsWith(".css")) {
    return "text/css";
  } else if (filename.endsWith(".js")) {
    return "application/javascript";
  } else if (filename.endsWith(".png")) {
    return "image/png";
  } else if (filename.endsWith(".gif")) {
    return "image/gif";
  } else if (filename.endsWith(".jpg")) {
    return "image/jpeg";
  } else if (filename.endsWith(".ico")) {
    return "image/x-icon";
  } else if (filename.endsWith(".xml")) {
    return "text/xml";
  } else if (filename.endsWith(".pdf")) {
    return "application/x-pdf";
  } else if (filename.endsWith(".zip")) {
    return "application/x-zip";
  } else if (filename.endsWith(".gz")) {
    return "application/x-gzip";
  }
  return "text/plain";
}

bool exists(String path){
  bool yes = false;
  File file = SPIFFS.open(path, "r");
  if(!file.isDirectory()){
    yes = true;
  }
  file.close();
  return yes;
}

bool handleFileRead(String path) {
  if (path.endsWith("/")) {
    path += "index.htm";
  }
  String contentType = getContentType(path);
  String pathWithGz = path + ".gz";
  if (exists(pathWithGz) || exists(path)) {
    if (exists(pathWithGz)) {
      path += ".gz";
    }
    File file = SPIFFS.open(path, "r");
    server.streamFile(file, contentType);
    file.close();
    return true;
  }
  return false;
}

void setup() {
  Serial.begin(115200);

  // Add known ble beacon mac address
  //allowed.insert(BLEAddress("xx:xx:xx:xx:xx:xx"));

  BLEDevice::init("");
  pBLEScan = BLEDevice::getScan(); //create new scan
  pBLEScan->setAdvertisedDeviceCallbacks(new MyAdvertisedDeviceCallbacks());
  pBLEScan->setActiveScan(true); //active scan uses more power, but get results faster
  pBLEScan->setInterval(100);
  pBLEScan->setWindow(99);

  WiFi.softAP(ssid, password);
  IPAddress IP = WiFi.softAPIP();
  Serial.print("AP IP address: ");
  Serial.println(IP);
  SPIFFS.begin();

  //called when the url is not defined here
  //use it to load content from FILESYSTEM
  server.onNotFound([]() {
    if (!handleFileRead(server.uri())) {
      server.send(404, "text/plain", "FileNotFound");
    }
  });
  //server.addHandler(new SPIFFSEditor(SPIFFS, "admin","admin"));

  // Send a GET request to <IP>/read/<number>
  server.on("/", HTTP_GET, []() {
    if (!handleFileRead("/index.htm")) {
      server.send(404, "text/plain", "FileNotFound");
    }
  });

  // Send a GET request to <IP>/read/<number>
  server.on(UriBraces("/read/{}"), HTTP_GET, []() {
    String addrStr = server.pathArg(0);
    int addr = strtol(addrStr.c_str(), 0, 16);
    Serial.printf("Reading addr %d\n", addr);
    int value = controller.readValue(addr);
    server.send(200, "text/plain", String(value));
  });

  // Send a GET request to <IP>/read/<number>
  server.on(UriBraces("/write/{}"), HTTP_POST, []() {
    String addrStr = server.pathArg(0);
    String body = server.arg("plain");
    int addr = strtol(addrStr.c_str(), 0, 16);
    int val = strtol(body.c_str(), 0, 10);
    Serial.printf("Writing addr %d with value %d\n", addr, val);
    controller.writeValue(addr, val);
    server.send(200, "text/plain", "Ok");
  });

  server.begin();

  delay(100); // wait for the controller to start
  controller.begin(&mySerial, RX, TX);
  controller.getBoardInfo();
}

void loop() {
  if (found == false)
  {
    if (mode != TORQUE_MODE)
    {
      int rpm = controller.readValue(RPM);
      if (rpm >= 0 && rpm < SAFE_RPM) // security feature to only set mode when stationary
      {
        mode = controller.readValue(CTRL_MODE);
        if (mode != TORQUE_MODE)
        {
          controller.writeValue(CTRL_MODE, TORQUE_MODE);
          mode = TORQUE_MODE;//controller.readValue(CTRL_MODE);
        }
      }
    }
      Serial.println("Token not found, checking...");
    BLEScanResults foundDevices = pBLEScan->start(SCAN_TIME, false);
  }
  else if (found && mode != SPEED_MODE)
  {
      Serial.println("Token found.");
      controller.writeValue(CTRL_MODE, SPEED_MODE);
      mode = SPEED_MODE;//controller.readValue(CTRL_MODE);
  }
  server.handleClient();
  vTaskDelay( 2 / portTICK_PERIOD_MS );
}