#include <Arduino.h>
#include <SPIFFS.h>            
#include <WiFi.h>              
#include <WiFiMulti.h>
#include <ESPmDNS.h>           
#include <AsyncTCP.h>          
#include <ESPAsyncWebServer.h>
#include <SD.h>
#include <SPI.h>
#include <Update.h>

#include "esp_system.h"        
#include "esp_spi_flash.h"     
#include "esp_wifi_types.h"    
#include "esp_bt.h"            
#include "FS.h"
#include "Network.h"


#define SD_CS_pin 5
#define  FS SPIFFS
#define serverName "fileserver"
    bool espShouldReboot;

AsyncWebServer server(80);

//################  VERSION  ###########################################
String    Version = "1.0";   // Programme version, see change log at end
//################ VARIABLES ###########################################

typedef struct
{
  String filename;
  String ftype;
  String fsize;
} fileinfo;

bool SD_present = false;
String   webpage, MessageLine;
fileinfo Filenames[200]; // Enough for most purposes!
bool     StartupErrors = false;
int      start, downloadtime = 1, uploadtime = 1, downloadsize, uploadsize, downloadrate, uploadrate, numfiles;
float    Temperature = 21.34; // for example new page, amend in a snesor function if required
String   Name = "Dave";
WiFiMulti wifiMulti;

void Dir(AsyncWebServerRequest *request);
void Dir_sd(AsyncWebServerRequest *request);
void Directory();
void Directory_sd();
void UploadFileSelect();
void UploadFileSelect_sd();
void UploadFirmewareSelect();
void Format();
void handleFileUpload(AsyncWebServerRequest *request, const String &filename, size_t index, uint8_t *data, size_t len, bool final);
void handleFileUpload_sd(AsyncWebServerRequest *request, const String &filename, size_t index, uint8_t *data, size_t len, bool final);
void handleFirmwareUpload(AsyncWebServerRequest *request, const String &filename, size_t index, uint8_t *data, size_t len, bool final);
void notFound(AsyncWebServerRequest *request);
void Handle_File_Download();
String getContentType(String filenametype);
void Select_File_For_Function(String title, String function);
void Select_File_For_Function_sd(String title, String function);
void SelectInput(String Heading, String Command, String Arg_name);
int GetFileSize(String filename);
void Home();
void Get() ;
void Send();
void Page_Not_Found();
String ConvBinUnits(size_t bytes, byte resolution);
bool StartMDNSservice(const char *Name);
bool StartMDNSservice(const char *Name);
String HTML_Header();
String HTML_Footer();
void FileUploadSucceed();

void File_Rename();
void Display_System_Info();
void LogOut();
void Display_New_Page();
String EncryptionType(wifi_auth_mode_t encryptionType);






void setup() {
  Serial.begin(115200);
  // while (!Serial);
  // Serial.println(__FILE__);
  // WiFi.mode(WIFI_STA);
  // WiFi.begin(ssid, password);
  // if (WiFi.waitForConnectResult() != WL_CONNECTED) {
  //   Serial.printf("STA: Failed!\n");
  //   WiFi.disconnect(false);
  //   delay(500);
  //   WiFi.begin(ssid, password);
  // }
  // Serial.println("IP Address: " + WiFi.localIP().toString());

  if (!WiFi.config(local_IP, gateway, subnet, dns))
  { 
    Serial.println("WiFi STATION Failed to configure Correctly");
  }

  wifiMulti.addAP(ssid_1, password_1); // add Wi-Fi networks you want to connect to, it connects strongest to weakest
  wifiMulti.addAP(ssid_2, password_2);  // Adjust the values in the Network tab
  wifiMulti.addAP(ssid_3, password_3);
  wifiMulti.addAP(ssid_4, password_4);  

  Serial.println("Connecting ...");
  while (wifiMulti.run() != WL_CONNECTED)
  { // Wait for the Wi-Fi to connect: scan for Wi-Fi networks, and connect to the strongest of the networks above
    delay(250);
    Serial.print('.');
  }
  Serial.println("\nConnected to " + WiFi.SSID() + " Use IP address: " + WiFi.localIP().toString()); // Report which SSID and IP is in use

  if (WiFi.scanComplete() == -2) WiFi.scanNetworks(true); // Complete an initial scan for WiFi networks, otherwise = 0 on first display!
  if (!StartMDNSservice(ServerName)) {
    Serial.println("Error starting mDNS Service...");;
    StartupErrors = true;
  }
  if (!FS.begin(true)) {
    Serial.println("Error preparing Filing System...");
    StartupErrors = true;
  }
  Serial.print(F("Initializing SD card..."));
  if (!SD.begin(SD_CS_pin))
  { // see if the card is present and can be initialised. Wemos SD-Card CS uses D8
    Serial.println(F("Card failed or not present, no SD Card data logging possible..."));
    SD_present = false;
  }
  else
  {
    Serial.println(F("Card initialised... file access enabled..."));
    SD_present = true;
  }

  // ##################### HOMEPAGE HANDLER ###########################
  server.on("/", HTTP_GET, [](AsyncWebServerRequest * request) {
    Serial.println("Home Page...");
    Home(); // Build webpage ready for display
    request->send(200, "text/html", webpage);
  });

  // ##################### Send Page ###########################
  server.on("/Send", HTTP_GET, [](AsyncWebServerRequest *request)
            {
    Serial.println("Send Page...");
    Send(); // Build webpage ready for display
    request->send(200, "text/html", webpage); });

  // ##################### Get Page ###########################
  server.on("/Get", HTTP_GET, [](AsyncWebServerRequest *request)
            {
    Serial.println("Get Page...");
    Get(); // Build webpage ready for display
    request->send(200, "text/html", webpage); });

  // ##################### DOWNLOAD HANDLER ##########################
  server.on("/download_mem", HTTP_GET, [](AsyncWebServerRequest * request) {
    Serial.println("Downloading file...");
    Select_File_For_Function("[DOWNLOAD]", "downloadhandler"); // Build webpage ready for display
    request->send(200, "text/html", webpage);
  });

  // ##################### DOWNLOAD HANDLER SD ##########################
  server.on("/downloadSD", HTTP_GET, [](AsyncWebServerRequest *request)
            {
    Serial.println("Downloading file...");
    Select_File_For_Function_sd("[DOWNLOAD]", "downloadhandler"); // Build webpage ready for display
    request->send(200, "text/html", webpage); });

  // ##################### UPLOAD Firmware HANDLERS ###########################
  server.on("/update", HTTP_GET, [](AsyncWebServerRequest *request)
            {
    Serial.println("Update...");
    UploadFirmewareSelect(); // Build webpage ready for display
    request->send(200, "text/html", webpage); });

  // ##################### UPLOAD HANDLERS ###########################
  server.on("/upload_mem", HTTP_GET, [](AsyncWebServerRequest * request) {
    Serial.println("Uploading file...");
    UploadFileSelect(); // Build webpage ready for display
    request->send(200, "text/html", webpage);
  });

  // ##################### UPLOAD SD HANDLERS ###########################
  server.on("/uploadSD", HTTP_GET, [](AsyncWebServerRequest *request)
            {
    Serial.println("Uploading file...");
    UploadFileSelect_sd(); // Build webpage ready for display
    request->send(200, "text/html", webpage); });


  // Set handler for '/handleuploadfirmawre'
  server.on("/firmware", HTTP_POST, [](AsyncWebServerRequest *request) {},
      [](AsyncWebServerRequest *request, const String &filename, size_t index, uint8_t *data,
         size_t len, bool final)
      {
        handleFirmwareUpload(request, filename, index, data, len, final);
      });

  // Set handler for '/handleupload'
  server.on("/handleupload", HTTP_POST, [](AsyncWebServerRequest * request) {espShouldReboot = !Update.hasError();},
  [](AsyncWebServerRequest * request, const String & filename, size_t index, uint8_t *data,
     size_t len, bool final) {
    handleFileUpload(request, filename, index, data, len, final);
  });

  // Set handler for '/handleuploadSD'
  server.on("/handleupload_sd", HTTP_POST, [](AsyncWebServerRequest *request) {},
      [](AsyncWebServerRequest *request, const String &filename, size_t index, uint8_t *data,
         size_t len, bool final)
      {
        handleFileUpload_sd(request, filename, index, data, len, final);
      });

  // Set handler for '/handleformat'
  server.on("/handleformat", HTTP_GET, [](AsyncWebServerRequest * request) {
    Serial.println("Processing Format Request of File System...");
    if (request->getParam("format")->value() == "YES") {
      Serial.print("Starting to Format Filing System...");
      FS.end();
      bool formatted = FS.format();
      if (formatted) {
        Serial.println(" Successful Filing System Format...");
      }
      else         {
        Serial.println(" Formatting Failed...");
      }
    }
    request->redirect("/dir");
  });

  

  

  // ##################### DIR HANDLER ###############################
  server.on("/dir", HTTP_GET, [](AsyncWebServerRequest * request) {
    Serial.println("File Directory...");
    Dir(request); // Build webpage ready for display
    request->send(200, "text/html", webpage);
  });

  // ##################### DIR SD HANDLER ###############################
  server.on("/dirSD", HTTP_GET, [](AsyncWebServerRequest *request)
            {
    Serial.println("File Directory...");
    Dir_sd(request); // Build webpage ready for display
    request->send(200, "text/html", webpage); });

  // ##################### FORMAT HANDLER ############################
  server.on("/format", HTTP_GET, [](AsyncWebServerRequest * request) {
    Serial.println("Request to Format File System...");
    Format(); // Build webpage ready for display
    request->send(200, "text/html", webpage);
  });

  // ##################### upload succeed ############################
  server.on("/valid", HTTP_GET, [](AsyncWebServerRequest *request)
            {
    Serial.println("Request to Format File System...");
    FileUploadSucceed(); // Build webpage ready for display
    request->send(200, "text/html", webpage); });

  // ##################### NOT FOUND HANDLER #########################
  server.onNotFound(notFound);

  // TO add new pages add a handler here, make  sure the HTML Header has a menu item for it, create a page for it using a function. Copy this one, rename it.
  

  server.begin();  // Start the server
  if (!StartupErrors)
    Serial.println("System started successfully...");
  else
    Serial.println("There were problems starting all services...");
  Directory();     // Update the file list
}

void loop() {
  // Nothing to do here yet ... add your requirements to do things!
  if (espShouldReboot)
  {
    Serial.println(F("Esp reboot ..."));
    delay(10000);
    ESP.restart();
  }
}
//#############################################################################################
void Dir(AsyncWebServerRequest * request) {
  String Fname1, Fname2;
  int index = 0;
  Directory(); // Get a list of the current files on the FS
  webpage  = HTML_Header();
  webpage += "<h3>Filing System Content</h3><br>";
  if (numfiles > 0) {
    webpage += "<table class='center'>";
    webpage += "<tr><th>Type</th><th>File Name</th><th>File Size</th><th class='sp'></th><th>Type</th><th>File Name</th><th>File Size</th></tr>";
    while (index < numfiles) {
      Fname1 = Filenames[index].filename;
      Fname2 = Filenames[index + 1].filename;
      webpage += "<tr>";
      webpage += "<td style = 'width:5%'>" + Filenames[index].ftype + "</td><td style = 'width:25%'>" + Fname1 + "</td><td style = 'width:10%'>" + Filenames[index].fsize + "</td>";
      webpage += "<td class='sp'></td>";
      if (index < numfiles - 1) {
        webpage += "<td style = 'width:5%'>" + Filenames[index + 1].ftype + "</td><td style = 'width:25%'>" + Fname2 + "</td><td style = 'width:10%'>" + Filenames[index + 1].fsize + "</td>";
      }
      webpage += "</tr>";
      index = index + 2;
    }
    webpage += "</table>";
    webpage += "<p style='background-color:yellow;'><b>" + MessageLine + "</b></p>";
    MessageLine = "";
  }
  else
  {
    webpage += "<h2>No Files Found</h2>";
  }
  webpage += HTML_Footer();
  request->send(200, "text/html", webpage);
}

//#############################################################################################
void Dir_sd(AsyncWebServerRequest *request)
{
  String Fname1, Fname2;
  int index = 0;
  Directory_sd(); // Get a list of the current files on the FS
  webpage = HTML_Header();
  webpage += "<h3>Filing System Content</h3><br>";
  if (numfiles > 0)
  {
    webpage += "<table class='center'>";
    webpage += "<tr><th>Type</th><th>File Name</th><th>File Size</th><th class='sp'></th><th>Type</th><th>File Name</th><th>File Size</th></tr>";
    while (index < numfiles)
    {
      Fname1 = Filenames[index].filename;
      Fname2 = Filenames[index + 1].filename;
      webpage += "<tr>";
      webpage += "<td style = 'width:5%'>" + Filenames[index].ftype + "</td><td style = 'width:25%'>" + Fname1 + "</td><td style = 'width:10%'>" + Filenames[index].fsize + "</td>";
      webpage += "<td class='sp'></td>";
      if (index < numfiles - 1)
      {
        webpage += "<td style = 'width:5%'>" + Filenames[index + 1].ftype + "</td><td style = 'width:25%'>" + Fname2 + "</td><td style = 'width:10%'>" + Filenames[index + 1].fsize + "</td>";
      }
      webpage += "</tr>";
      index = index + 2;
    }
    webpage += "</table>";
    webpage += "<p style='background-color:yellow;'><b>" + MessageLine + "</b></p>";
    MessageLine = "";
  }
  else
  {
    webpage += "<h2>No Files Found</h2>";
  }
  webpage += HTML_Footer();
  request->send(200, "text/html", webpage);
}

//#############################################################################################
void Directory() {
  numfiles  = 0; // Reset number of FS files counter
  File root = FS.open("/");
  if (root) {
    root.rewindDirectory();
    File file = root.openNextFile();
    while (file) { // Now get all the filenames, file types and sizes
      Filenames[numfiles].filename = (String(file.name()).startsWith("/") ? String(file.name()).substring(1) : file.name());
      Filenames[numfiles].ftype    = (file.isDirectory() ? "Dir" : "File");
      Filenames[numfiles].fsize    = ConvBinUnits(file.size(), 1);
      file = root.openNextFile();
      numfiles++;
    }
    root.close();
  }
}

//#############################################################################################
void Directory_sd()
{
  if (SD_present) {
    numfiles = 0; // Reset number of FS files counter
    File root = SD.open("/");
    if (root)
    {
      root.rewindDirectory();
      File file = root.openNextFile();
      while (file)
      { // Now get all the filenames, file types and sizes
        Filenames[numfiles].filename = (String(file.name()).startsWith("/") ? String(file.name()).substring(1) : file.name());
        Filenames[numfiles].ftype = (file.isDirectory() ? "Dir" : "File");
        Filenames[numfiles].fsize = ConvBinUnits(file.size(), 1);
        file = root.openNextFile();
        numfiles++;
      }
      root.close();
    }
  }
}

//#############################################################################################
void UploadFirmewareSelect()
{
  webpage = HTML_Header();
  webpage += "<h3>Upload firmware</h3>";
  webpage += "<form method = 'POST' action = '/firmware' enctype='multipart/form-data'>";
  webpage += "<input class='buttons' style='width:40%' type='file' name='fupload_firm' id='fupload_firm' value=''><br><br>";
  webpage += "<button class='buttons' style='width:10%' type='submit'>Upload File</button><br><a href='/send'><br>[Back]</a><br><br>";
  webpage += HTML_Footer();
}
//#############################################################################################
void UploadFileSelect() {
  webpage  = HTML_Header();
  webpage += "<h3>Select File to Upload in flash memory</h3>";
  webpage += "<form method = 'POST' action = '/handleupload' enctype='multipart/form-data'>";
  webpage += "<input class='buttons' style='width:40%' type='file' name='fupload_mem' id='fupload_mem' value=''><br><br>";
  webpage += "<button class='buttons' style='width:10%' type='submit'>Upload File</button><br><a href='/send'><br>[Back]</a><br><br>";
  webpage += "</form>";
  webpage += HTML_Footer();
}

//#############################################################################################
void UploadFileSelect_sd(){
  webpage = HTML_Header();
  webpage += "<h3>Select File to Upload in the SD Card</h3>";
  webpage += "<form method = 'POST' action = '/handleupload_sd' enctype='multipart/form-data'>";
  webpage += "<input class='buttons' style='width:40%' type='file' name='fupload' id='fupload' value=''><br><br>";
  webpage += "<button class='buttons' style='width:10%' type='submit'>Upload File</button><br><a href='/send'><br>[Back]</a><br><br>";
  webpage += "</form>";
  webpage += HTML_Footer();
}
//#############################################################################################
void Format() {
  webpage  = HTML_Header();
  webpage += "<h3>***  Format Filing System on this device ***</h3>";
  webpage += "<form action='/handleformat'>";
  webpage += "<input type='radio' id='YES' name='format' value = 'YES'><label for='YES'>YES</label><br><br>";
  webpage += "<input type='radio' id='NO'  name='format' value = 'NO' checked><label for='NO'>NO</label><br><br>";
  webpage += "<input type='submit' value='Format?'>";
  webpage += "</form>";
  webpage += HTML_Footer();
}
//#############################################################################################
void handleFileUpload(AsyncWebServerRequest *request, const String& filename, size_t index, uint8_t *data, size_t len, bool final) {
  if (!index) {
    String file = filename;
    if (!filename.startsWith("/")) file = "/" + filename;
    request->_tempFile = FS.open(file, "w");
    if (!request->_tempFile) Serial.println("Error creating file for upload...");
    start = millis();
  }
  if (request->_tempFile) {
    if (len) {
      request->_tempFile.write(data, len); // Chunked data
      Serial.println("Transferred : " + String(len) + " Bytes");
    }
    if (final) {
      uploadsize = request->_tempFile.size();
      request->_tempFile.close();
      uploadtime = millis() - start;
      request->redirect("/valid");
    }
  }
}



    //#############################################################################################
void handleFirmwareUpload(AsyncWebServerRequest *request, const String &filename, size_t index, uint8_t *data, size_t len, bool final)
{
  if (!index)
        {
          Serial.printf("Update Start: %s\n", filename.c_str());
          if (!Update.begin((ESP.getFreeSketchSpace() - 0x1000) & 0xFFFFF000))
          {
            Update.printError(Serial);
          }
        }
        if (!Update.hasError())
        {
          if (Update.write(data, len) != len)
          {
            Update.printError(Serial);
          }
        }
        if (final)
        {
          if (Update.end(true))
          {
            Serial.printf("Update Success: %uB\n", index + len);
            request->redirect("/valid");
          }
          else
          {
            Update.printError(Serial);
          }
}
}

//#############################################################################################
void handleFileUpload_sd(AsyncWebServerRequest *request, const String &filename, size_t index, uint8_t *data, size_t len, bool final)
{
  if (!index)
  {
    String file = filename;
    if (!filename.startsWith("/"))
      file = "/" + filename;
    request->_tempFile = SD.open(file, "w");
    if (!request->_tempFile)
      Serial.println("Error creating file for upload...");
    start = millis();
  }
  if (request->_tempFile)
  {
    if (len)
    {
      request->_tempFile.write(data, len); // Chunked data
      Serial.println("Transferred : " + String(len) + " Bytes");
    }
    if (final)
    {
      uploadsize = request->_tempFile.size();
      request->_tempFile.close();
      uploadtime = millis() - start;
      
     request->redirect("/valid");
    }
    
  }
}

//#############################################################################################
void File_Delete() {
  SelectInput("Select a File to Delete", "handledelete", "filename");
}


//#############################################################################################
// Not found handler is also the handler for 'delete', 'download' and 'stream' functions
void notFound(AsyncWebServerRequest *request) { // Process selected file types
  String filename;
  if (request->url().startsWith("/downloadhandler") ||
      request->url().startsWith("/streamhandler")   ||
      request->url().startsWith("/deletehandler")   ||
      request->url().startsWith("/renamehandler"))
  {
    // Now get the filename and handle the request for 'delete' or 'download' or 'stream' functions
    if (!request->url().startsWith("/renamehandler")) filename = request->url().substring(request->url().indexOf("~/") + 1);
    start = millis();
    if (request->url().startsWith("/downloadhandler"))
    {
      Serial.println("Download handler started...");
      MessageLine = "";
      File file = FS.open(filename, "r");
      String contentType = getContentType("download");
      AsyncWebServerResponse *response = request->beginResponse(contentType, file.size(), [file](uint8_t *buffer, size_t maxLen, size_t total) mutable -> size_t {
        File filecopy = file;
        int bytes = filecopy.read(buffer, maxLen);
        if (bytes + total == filecopy.size()) filecopy.close();
        return max(0, bytes); 
      });
      response->addHeader("Server", "ESP Async Web Server");

      request->send(response);
      downloadtime = millis() - start;
      downloadsize = GetFileSize(filename);
      request->redirect("/dir");
    }
    
  }
  else
  {
    Page_Not_Found();
    request->send(200, "text/html", webpage);
  }
}
//#############################################################################################
void Handle_File_Download() {
  String filename = "";
  int index = 0;
  Directory(); // Get a list of files on the FS
  webpage = HTML_Header();
  webpage += "<h3>Select a File to Download</h3>";
  webpage += "<table>";
  webpage += "<tr><th>File Name</th><th>File Size</th></tr>";
  while (index < numfiles) {
    webpage += "<tr><td><a href='" + Filenames[index].filename + "'></a><td>" + Filenames[index].fsize + "</td></tr>";
    index++;
  }
  webpage += "</table>";
  webpage += "<p>" + MessageLine + "</p>";
  webpage += HTML_Footer();
}
//#############################################################################################
String getContentType(String filenametype) { // Tell the browser what file type is being sent
  if (filenametype == "download") {
    return "application/octet-stream";
  } else if (filenametype.endsWith(".txt"))  {
    return "text/plainn";
  } else if (filenametype.endsWith(".htm"))  {
    return "text/html";
  } else if (filenametype.endsWith(".html")) {
    return "text/html";
  } else if (filenametype.endsWith(".css"))  {
    return "text/css";
  } else if (filenametype.endsWith(".js"))   {
    return "application/javascript";
  } else if (filenametype.endsWith(".png"))  {
    return "image/png";
  } else if (filenametype.endsWith(".gif"))  {
    return "image/gif";
  } else if (filenametype.endsWith(".jpg"))  {
    return "image/jpeg";
  } else if (filenametype.endsWith(".ico"))  {
    return "image/x-icon";
  } else if (filenametype.endsWith(".xml"))  {
    return "text/xml";
  } else if (filenametype.endsWith(".pdf"))  {
    return "application/x-pdf";
  } else if (filenametype.endsWith(".zip"))  {
    return "application/x-zip";
  } else if (filenametype.endsWith(".gz"))   {
    return "application/x-gzip";
  }
  return "text/plain";
}
//#############################################################################################
void Select_File_For_Function(String title, String function) {
  String Fname1, Fname2;
  int index = 0;
  Directory(); // Get a list of files on the FS
  webpage = HTML_Header();
  webpage += "<h3>Select a File to " + title + " from this device</h3>";
  webpage += "<table class='center'>";
  webpage += "<tr><th>File Name</th><th>File Size</th><th class='sp'></th><th>File Name</th><th>File Size</th></tr>";
  while (index < numfiles) {
    Fname1 = Filenames[index].filename;
    Fname2 = Filenames[index + 1].filename;
    if (Fname1.startsWith("/")) Fname1 = Fname1.substring(1);
    if (Fname2.startsWith("/")) Fname1 = Fname2.substring(1);
    webpage += "<tr>";
    webpage += "<td style='width:25%'><button><a href='" + function + "~/" + Fname1 + "'>" + Fname1 + "</a></button></td><td style = 'width:10%'>" + Filenames[index].fsize + "</td>";
    webpage += "<td class='sp'></td>";
    if (index < numfiles - 1) {
      webpage += "<td style='width:25%'><button><a href='" + function + "~/" + Fname2 + "'>" + Fname2 + "</a></button></td><td style = 'width:10%'>" + Filenames[index + 1].fsize + "</td>";
    }
    webpage += "</tr>";
    index = index + 2;
  }
  webpage += "</table>";
  webpage += HTML_Footer();
}

//****************************************************************//
void Select_File_For_Function_sd(String title, String function)
{
  String Fname1, Fname2;
  int index = 0;
  Directory_sd(); // Get a list of files on the FS
  webpage = HTML_Header();
  webpage += "<h3>Select a File to " + title + " from this device</h3>";
  webpage += "<table class='center'>";
  webpage += "<tr><th>File Name</th><th>File Size</th><th class='sp'></th><th>File Name</th><th>File Size</th></tr>";
  while (index < numfiles)
  {
    Fname1 = Filenames[index].filename;
    Fname2 = Filenames[index + 1].filename;
    if (Fname1.startsWith("/"))
      Fname1 = Fname1.substring(1);
    if (Fname2.startsWith("/"))
      Fname1 = Fname2.substring(1);
    webpage += "<tr>";
    webpage += "<td style='width:25%'><button><a href='" + function + "~/" + Fname1 + "'>" + Fname1 + "</a></button></td><td style = 'width:10%'>" + Filenames[index].fsize + "</td>";
    webpage += "<td class='sp'></td>";
    if (index < numfiles - 1)
    {
      webpage += "<td style='width:25%'><button><a href='" + function + "~/" + Fname2 + "'>" + Fname2 + "</a></button></td><td style = 'width:10%'>" + Filenames[index + 1].fsize + "</td>";
    }
    webpage += "</tr>";
    index = index + 2;
  }
  webpage += "</table>";
  webpage += HTML_Footer();
}
//#############################################################################################
void SelectInput(String Heading, String Command, String Arg_name) {
  webpage = HTML_Header();
  webpage += "<h3>" + Heading + "</h3>";
  webpage += "<form  action='/" + Command + "'>";
  webpage += "Filename: <input type='text' name='" + Arg_name + "'><br><br>";
  webpage += "<input type='submit' value='Enter'>";
  webpage += "</form>";
  webpage += HTML_Footer();
}
//#############################################################################################
int GetFileSize(String filename) {
  int filesize;
  File CheckFile = FS.open(filename, "r");
  filesize = CheckFile.size();
  CheckFile.close();
  return filesize;
}
//#############################################################################################
void Home() {
  webpage = HTML_Header();
  webpage += "<div1 class='home'>";
  webpage += "<h1>Send / Get files </h1>";
  webpage += "</div1>";

  webpage += HTML_Footer();
}

//#############################################################################################
void Get()
{
  webpage = HTML_Header();
  webpage += "<div class = 'main-contain'>";
  webpage += "<h1>Get files</ h1><div class = 'div'><div class = 'flex-1'>";
  webpage += "<button class = 'button btn-3' onclick = 'window.location.href ='/downloadSD';'> SD Card</ button>";
  webpage += "<button class = 'button btn-3' onclick = 'window.location.href ='/download_mem';'> Flash Memory</ button>";
  webpage += "</ div></ div></ div>";

          // webpage += "<br><br><br>";
          // webpage += "<a href='/download_mem'/><button class='button1'>Get file from flash memory</button></a> ";
          // webpage += "<a href='/downloadSD'/><button class='button1'>Get file from sd card </button></a> ";
          // webpage += "<br><br>";

  webpage += HTML_Footer();
}

//#############################################################################################
void Send()
{
  webpage = HTML_Header();
  webpage += "<div class = 'main-contain'>";
  webpage += "<h1>Send files</ h1><div class = 'div'><div class = 'flex-1'>";
  webpage += "<button class = 'button btn-3' onclick = 'window.location.href ='/uploadSD';'> SD Card</ button>";
  webpage += "<button class = 'button btn-3' onclick = 'window.location.href ='/upload_mem';'>Flash Memory</button>";
  webpage += "<button class = 'button btn-3' onclick = 'window.location.href ='/update';'>Firmware</button>";
  webpage += "</ div></ div></ div>";
  webpage += HTML_Footer();
}
//#############################################################################################
void FileUploadSucceed(){
  webpage = HTML_Header();
  webpage = HTML_Header();
  webpage += F("<h4>File was successfully uploaded</h4>");
  webpage += HTML_Footer();
}
//#############################################################################################
void Page_Not_Found() {
  webpage = HTML_Header();
  webpage += "<div class='notfound'>";
  webpage += "<h1>Sorry</h1>";
  webpage += "<p>Error 404 - Page Not Found</p>";
  webpage += "</div><div class='left'>";
  webpage += "<p>The page you were looking for was not found, it may have been moved or is currently unavailable.</p>";
  webpage += "<p>Please check the address is spelt correctly and try again.</p>";
  webpage += "<p>Or click <b><a href='/'>[Here]</a></b> for the home page.</p></div>";
  webpage += HTML_Footer();
}

//#############################################################################################
String ConvBinUnits(size_t bytes, byte resolution) {
  if      (bytes < 1024)                 {
    return String(bytes) + " B";
  }
  else if (bytes < 1024 * 1024)          {
    return String(bytes / 1024.0, resolution) + " KB";
  }
  else if (bytes < (1024 * 1024 * 1024)) {
    return String(bytes / 1024.0 / 1024.0, resolution) + " MB";
  }
  else return "";
}
//#############################################################################################
String EncryptionType(wifi_auth_mode_t encryptionType) {
  switch (encryptionType) {
    case (WIFI_AUTH_OPEN):
      return "OPEN";
    case (WIFI_AUTH_WEP):
      return "WEP";
    case (WIFI_AUTH_WPA_PSK):
      return "WPA PSK";
    case (WIFI_AUTH_WPA2_PSK):
      return "WPA2 PSK";
    case (WIFI_AUTH_WPA_WPA2_PSK):
      return "WPA WPA2 PSK";
    case (WIFI_AUTH_WPA2_ENTERPRISE):
      return "WPA2 ENTERPRISE";
    case (WIFI_AUTH_MAX):
      return "WPA2 MAX";
    default:
      return "";
  }
}
//#############################################################################################
bool StartMDNSservice(const char* Name) {
  esp_err_t err = mdns_init();             // Initialise mDNS service
  if (err) {
    printf("MDNS Init failed: %d\n", err); // Return if error detected
    return false;
  }
  mdns_hostname_set(Name);                 // Set hostname
  return true;
}

String HTML_Header()
{
  String page;
  page = "<!DOCTYPE html>";
  page += "<html lang = 'en'>";
  page += "<head>";
  page += "<title>Web Server</title>";
  page += "<meta charset='UTF-8'>"; // Needed if you want to display special characters like the ° symbol
  page += "<style>";
  page += "@import url('https://fonts.googleapis.com/css?family=Bungee Shade');";
  page += "body {width:65em;margin-left:auto;margin-right:auto;font-family:Arial,Helvetica,sans-serif;font-size:16px;color:blue;background-color:#e1e1ff;text-align:center;}";
  page += "footer {padding:0.8em;background-color:cyan;font-size:1.2em;margin-top: 4.5em;}";
  page += "table {font-family:arial,sans-serif;border-collapse:collapse;width:70%;}"; // 70% of 75em!
  page += "table.center {margin-left:auto;margin-right:auto;}";
  page += "td, th {border:1px solid #dddddd;text-align:left;padding:8px;}";
  page += "tr:nth-child(even) {background-color:#dddddd;}";
  page += ".home h1{color:#2c3e50;margin-top: 100px;font-size:60px;text-align: center;}";
  page += "h2{color:orange;font-size:1.0em;}";
  page += "h4 {color:slateblue;font:0.8em;text-align:left;font-style:oblique;text-align:center;font-family: 'Bungee Shade';}";
  page += ".center {margin-left:auto;margin-right:auto;}";
  page += ".button1{border-radius:0.5em;background:#558ED5;padding:0.3em 0.3em;width:20%;color:white;font-size:130%;}";
  page += ".topnav {overflow: hidden;background-color:lightcyan;margin-top: 5.5em;}";
  page += ".topnav a {float:left;color:blue;text-align:center;padding:1.0em 2.0em;text-decoration:none;font-size:1.6em;}";
  page += ".topnav a:hover {background-color:deepskyblue;color:white;}";
  page += ".topnav a.active {background-color:lightblue;color:blue;}";
  page += ".notfound {padding:0.8em;text-align:center;font-size:1.5em;}";
  page += ".left {text-align:left;}";
  page += ".medium {font-size:1.4em;padding:0;margin:0}";
  page += ".ps {font-size:0.7em;padding:0;margin:0}";
  page += ".sp {background-color:silver;white-space:nowrap;width:2%;}";
  page += ".main-contain {display : flex; flex-direction: column; justify-content: center; max-width: 180rem; margin:auto;}";
  page += ".main-contain h1 {color: #2c3e50;margin-top : 20px;margin-bottom : 20px;font-size: 50px;text-align: center; font-family : 'Bungee Shade';}";
  page += "button {border: none; text-decoration: none;color: rgba(255, 255, 255, 0.95); cursor: pointer;}";
  page += ".div {padding-top: 10px;display:flex; flex-wrap: wrap;justify-content: space-around;text-align: center;position: relative;max-width: 1200px;margin: auto;}";
  page += ".flex-1 {flex: 1;min-width: 250px; margin: 0 auto 50px;}";
  page += ".button { margin: 8px;width: 220px;height: 80px;background: white;text-align: center;display: inline-block; font-size: 1.3rem; text-transform: uppercase; font-weight: 700; position: relative;will-change: transform;}";
  page += ".btn-3 { letter-spacing: .05rem; position: relative; background: white; color: #401aff; overflow: hidden; transition: .3s ease-in-out;border-radius: .3rem;z-index: 1;box-shadow: 0 19px 38px rgba(0, 0, 0, 0.3), 0 15px 12px rgba(0, 0, 0, 0.22);}";
  page += ".btn-3:hover { box-shadow : 0 10px 20px rgba(0, 0, 0, 0.19),0 6px 6px rgba(0, 0, 0, 0.23);transform:scale(0.95);}";
  page += ".buttons {border-radius : 0.5em; background:# 558ED5; padding: 0.5em 0.5em; width:15 % ; color: white; font-size : 100 % ;}";
  page += ".form-container{ margin-top : 50px;";
  page += ".form-container h3 { color: #2c3e50; font-size: 1.8em;}";
  page += "</style>";
  page += "</head>";
  page += "<body>";
  page += "<div class = 'topnav'>";
  page += "<a href='http://localhost/testphpjs/index.html'>Home</a>";
  page += "<a href='http://localhost/testphpjs/one/get.html'>GET</a>";
  page += "<a href='http://localhost/testphpjs/one/send.html'>SEND</a> ";
  page += "</div>";
return page;
}
//#############################################################################################
String HTML_Footer()
{
  String page;
  page += "<br><br><footer>";
  page += "<p class='medium'>PFE : Sfax Industry 4.0 Center</p>";
  page += "</footer>";
  page += "</body>";
  page += "</html>";
  return page;
}