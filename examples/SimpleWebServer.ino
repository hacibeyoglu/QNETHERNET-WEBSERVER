#include "QNEthernet.h"
#include <WebServer.h>
#include <SD.h>

#define DECK_IP IPAddress({192, 168, 1, 100})
const uint8_t DECK_MAC[] = {0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED};
#define DECK_DNS IPAddress({192, 168, 1, 1})
#define DECK_GW IPAddress({192, 168, 1, 1})
#define DECK_MASK IPAddress(255, 255, 255, 0)

using namespace qindesign::network;

WebServer server(80);

constexpr uint32_t kDHCPTimeout = 10000;

String imageList;
File uploadFile;

void prepareImageList()
{
  imageList = "[";
  File root = SD.open("/images");
  if (root.isDirectory())
  {
    while (true)
    {
      File entry = root.openNextFile();
      if (!entry)
      {
        break;
      }
      else
      {
        imageList.append("\"").append(entry.name()).append("\"").append(",");
        entry.close();
      }
    }
  }
  root.close();
  imageList.remove(imageList.lastIndexOf(","), 1);
  imageList.append("]");
}

void handleNotFound()
{
  String message = F("File Not Found\n\n");

  message += F("URI: ");
  message += server.uri();
  message += F("\nMethod: ");
  message += (server.method() == HTTP_GET) ? F("GET") : F("POST");
  message += F("\nArguments: ");
  message += server.args();
  message += F("\n");

  for (uint8_t i = 0; i < server.args(); i++)
  {
    message += " " + server.argName(i) + ": " + server.arg(i) + "\n";
  }

  server.send(404, F("text/plain"), message);
}

void handleFileUpload()
{
  if (server.uri() != "/upload")
  {
    return;
  }
  HTTPUpload &upload = server.upload();
  if (upload.status == UPLOAD_FILE_START)
  {
    if (SD.exists((char *)upload.filename.c_str()))
    {
      SD.remove((char *)upload.filename.c_str());
    }
    uploadFile = SD.open(upload.filename.c_str(), FILE_WRITE);
    Serial.print("Upload: START, filename: ");
    Serial.println(upload.filename);
  }
  else if (upload.status == UPLOAD_FILE_WRITE)
  {
    if (uploadFile)
    {
      uploadFile.write(upload.buf, upload.currentSize);
    }
    Serial.print("Upload: WRITE, Bytes: ");
    Serial.println(upload.currentSize);
  }
  else if (upload.status == UPLOAD_FILE_END)
  {
    if (uploadFile)
    {
      uploadFile.close();
    }
    Serial.print("Upload: END, Size: ");
    Serial.println(upload.totalSize);
    prepareImageList();
  }
}
void returnFail(String msg)
{
  server.send(500, "text/plain", msg + "\r\n");
}
void returnOK()
{
  server.send(200, "text/plain", "");
}

void handleDelete()
{
  if (server.args() == 0)
  {
    return returnFail("BAD ARGS");
  }
  String path = server.arg(0);
  Serial.printf("Path to delete:%s\n", path.c_str());
  if (path == "/" || !SD.exists((char *)path.c_str()))
  {
    returnFail("BAD PATH");
    return;
  }
  File file = SD.open((char *)path.c_str());
  if (!file.isDirectory())
  {
    file.close();
    SD.remove((char *)path.c_str());
  }

  returnOK();
}

void setup()
{
  Serial.begin(115200);

  while (!Serial)
    ;

  Serial.print("Initialize Ethernet using DHCP => ");
  SD.begin(BUILTIN_SDCARD);
  prepareImageList();
  Ethernet.begin(DECK_IP, DECK_MAC, DECK_GW, DECK_DNS);
  // give the Ethernet shield minimum 1 sec for DHCP and 2 secs for staticP to initialize:
  delay(5000);
  if (kDHCPTimeout > 0)
  {
    if (!Ethernet.waitForLocalIP(kDHCPTimeout))
    {
      printf("Failed to get IP address from DHCP\r\n");
      // We may still get an address later, after the timeout,
      // so continue instead of returning
    }
  }

  Serial.print("IP Address = ");
  Serial.println(Ethernet.localIP());
  server.serveStatic("/config/", SD, "/config/");
  server.serveStatic("/images/", SD, "/images/");
  server.serveStatic("/admin/", SD, "/web/");
  server.on("/delete", HTTP_DELETE, handleDelete);
  server.on(
      "/upload", HTTP_POST, []()
      { returnOK(); },
      handleFileUpload);
  server.on(F("/flist"), []()
            { server.send(200, F("application/json"), imageList); });

  server.onNotFound(handleNotFound);
  server.begin();

  Serial.print(F("HTTP EthernetWebServer is @ IP : "));
  Serial.println(Ethernet.localIP());
}

void heartBeatPrint()
{
  static int num = 1;

  Serial.print(F("."));

  if (num == 80)
  {
    Serial.println();
    num = 1;
  }
  else if (num++ % 10 == 0)
  {
    Serial.print(F(" "));
  }
}

void check_status()
{
  static unsigned long checkstatus_timeout = 0;

#define STATUS_CHECK_INTERVAL 10000L

  // Send status report every STATUS_REPORT_INTERVAL (60) seconds: we don't need to send updates frequently if there is no status change.
  if ((millis() > checkstatus_timeout) || (checkstatus_timeout == 0))
  {
    heartBeatPrint();
    checkstatus_timeout = millis() + STATUS_CHECK_INTERVAL;
  }
}

void loop()
{
  server.handleClient();
  check_status();
}