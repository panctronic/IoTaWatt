#include "IotaWatt.h"

/*
  This WebServer code is incorporated with very little modification.
  Very simple yet powerful.

  A few new handlers were added at the end, and appropriate server.on
  declarations define them in the Setup code.

  The server supports reading and writing files to/from the SDcard.
  It also serves up HTML files to a browser.  The configuration utility is
  index.htm in the root directory of the SD card.
  The server also came with a great editor utility which, if placed on the SD,
  can be used to edit the web pages or any other text file on the SDcard.
  
  Small parts of the code are imbedded elsewhere as needed in the preamble and Setup sections.
  and "handleClient()" is invoked as often as practical in Loop to keep it running.
  
  The author's copyright and license info follows:

  --------------------------------------------------------------------------------------------------
 
  SDWebServer - Example WebServer with SD Card backend for esp8266

  Copyright (c) 2015  . All rights reserved.
  This file is part of the ESP8266WebServer library for Arduino environment.

  This library is free software; you can redistribute it and/or
  modify it under the terms of the GNU Lesser General Public
  License as published by the Free Software Foundation; either
  version 2.1 of the License, or (at your option) any later version.

  This library is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  Lesser General Public License for more details.

  You should have received a copy of the GNU Lesser General Public
  License along with this library; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA

  Have a FAT Formatted SD Card connected to the SPI port of the ESP8266
  The web root is the SD Card root folder
  File extensions with more than 3 charecters are not supported by the SD Library
  File Names longer than 8 charecters will be truncated by the SD library, so keep filenames shorter
  index.htm is the default index (works on subfolders as well)

  upload the contents of SdRoot to the root of the SDcard and access the editor by going to http://esp8266sd.local/edit

  ----------------------------------------------------------------------------------------------------------------
*/

const char txtPlain_P[] PROGMEM = "text/plain";
const char appJson_P[]  PROGMEM = "application/json";
const char txtJson_P[]  PROGMEM = "text/json";

bool authenticate(authLevel level){
  if(auth(level)){
    return true;
  } 
  requestAuth();
  return false;
}

    /* handleRequest - basic gatekeeper to administrate authorization */

void handleRequest(){
  String uri = server.uri();
      
  if(serverOn(authAdmin, F("/status"),HTTP_GET, handleStatus)) return;
  if(serverOn(authAdmin, F("/vcal"),HTTP_GET, handleVcal)) return;
  if(serverOn(authAdmin, F("/command"), HTTP_GET, handleCommand)) return;
  if(serverOn(authUser, F("/list"), HTTP_GET, printDirectory)) return;
  if(serverOn(authAdmin, F("/config"), HTTP_GET, handleGetConfig)) return;
  if(serverOn(authAdmin, F("/edit"), HTTP_DELETE, handleDelete)) return;
  if(serverOn(authAdmin, F("/edit"), HTTP_PUT, handleCreate)) return;
  if(serverOn(authUser, F("/feed/list.json"), HTTP_GET, handleGetFeedList)) return;
  if(serverOn(authUser, F("/feed/data.json"), HTTP_GET, handleGetFeedData)) return;
  if(serverOn(authAdmin, F("/graph/create"),HTTP_POST, handleGraphCreate)) return;
  if(serverOn(authAdmin, F("/graph/update"),HTTP_POST, handleGraphCreate)) return;
  if(serverOn(authAdmin, F("/graph/delete"),HTTP_POST, handleGraphDelete)) return;
  if(serverOn(authUser, F("/graph/getall"), HTTP_GET, handleGraphGetall)) return;
  if(serverOn(authAdmin, F("/auth"), HTTP_POST, handlePasswords)) return;
  if(serverOn(authUser, F("/nullreq"), HTTP_GET, returnOK)) return;
  if(serverOn(authUser, F("/query"), HTTP_GET, handleQuery)) return;
  if(serverOn(authUser, F("/DSTtest"), HTTP_GET, handleDSTtest)) return;


  if(loadFromSdCard(uri)){
    return;
  }
  
  trace(T_WEB,12); 
  String message = "Not found: ";
  message += (server.method() == HTTP_GET)?"GET":"POST";
  message += ", URI: ";
  message += server.uri();
  server.send(404, txtPlain_P, message);
}

bool serverOn(authLevel level, const __FlashStringHelper* uri, HTTPMethod method, genericHandler fn){
  if(strcmp_P(server.uri().c_str(),(PGM_P)uri) == 0 && server.method() == method){
    if( ! authenticate(level)) return true;
    fn();
    return true;
  }
  return false;
}

void returnOK() {
  server.send(200, txtPlain_P, "");
}

void returnFail(String msg) {
  server.send(500, txtPlain_P, msg + "\r\n");
}

bool loadFromSdCard(String path){
  trace(T_WEB,13);
  if( ! path.startsWith("/")) path = '/' + path;
  String dataType = txtPlain_P;
  if(path.endsWith("/")) path += F("index.htm");
  if(path == F("/edit") || path == F("/graph")){
    path += F(".htm");
  }
  
  if(path.endsWith(F(".src"))) path = path.substring(0, path.lastIndexOf("."));
  else if(path.endsWith(F(".htm"))) dataType = F("text/html");
  else if(path.endsWith(F(".css"))) dataType = F("text/css");
  else if(path.endsWith(F(".js")))  dataType = F("application/javascript");
  else if(path.endsWith(F(".png"))) dataType = F("image/png");
  else if(path.endsWith(F(".gif"))) dataType = F("image/gif");
  else if(path.endsWith(F(".jpg"))) dataType = F("image/jpeg");
  else if(path.endsWith(F(".ico"))) dataType = F("image/x-icon");
  else if(path.endsWith(F(".xml"))) dataType = F("text/xml");
  else if(path.endsWith(F(".pdf"))) dataType = F("application/pdf");
  else if(path.endsWith(F(".zip"))) dataType = F("application/zip");

  if(path.startsWith(F("/esp_spiffs/"))){
    return loadFromSpiffs(path.substring(11), dataType);
  }

  File dataFile = SD.open(path.c_str());
  if(dataFile.isDirectory()){
    path += F("/index.htm");
    dataType = F("text/html");
    dataFile = SD.open(path.c_str());
  }

 

          // If reading user directory,
          // authenticate as user
          // otherwise require admin.

  authLevel level = authAdmin;
  if(path.startsWith(F("/user/"))){
    level = authUser;
  }
  if( ! authenticate(level)) return true;

  if (server.hasArg(F("download"))){
    if(server.arg(F("download")) == "true") dataType = F("application/octet-stream");  
    else if(server.arg(F("download")) == "yes"){
      handleQuery();
      return true;
    }
  } 

  if (!dataFile){
    return false;
  }

  if(server.hasArg(F("textpos"))){
    sendMsgFile(dataFile, server.arg(F("textpos")).toInt());
  }

  else {
    if(path.equalsIgnoreCase(F("/config.txt"))){
      server.sendHeader(F("X-configSHA256"), base64encode(configSHA256, 32));
    }
    size_t sent = server.streamFile(dataFile, dataType);
    if ( sent != dataFile.size()) {
      Serial.printf_P(PSTR("Server: sent less data than expected. file %s, sent %d, expected %d\r\n"), dataFile.name(), sent, dataFile.size());
    }
  }
  dataFile.close();
  return true;
}

bool loadFromSpiffs(String path, String dataType){
  if( ! spiffsFileExists(path.c_str())){
    server.send(404, txtPlain_P, "Not Found");
    return false;
  }
  String contents = spiffsRead(path.c_str());
  server.send(200, dataType, contents);
  return true;
}

void handleFileUpload(){
  trace(T_WEB,11);
  if(server.uri() != "/edit") return;
  HTTPUpload& upload = server.upload();
  if( ! upload.filename.startsWith("/")){
    upload.filename = String('/') + upload.filename;
  }
  upload.filename.toLowerCase();
  if(upload.filename.startsWith(F("/esp_spiffs/"))){
    handleSpiffsUpload();
  }
  if(upload.status == UPLOAD_FILE_START){
    if( ! authenticate(authAdmin)) return;
      if(upload.filename.equals(F("/config.txt"))){
      if(server.hasHeader(F("X-configSHA256"))){
        if(server.header(F("X-configSHA256")) != base64encode(configSHA256, 32)){
          server.send(409, txtPlain_P, F("Config not current"));
          return;
        }
      }
    }
    if(SD.exists((char *)upload.filename.c_str())) SD.remove((char *)upload.filename.c_str());
    if(uploadFile = SD.open(upload.filename.c_str(), FILE_WRITE)){
      DBG_OUTPUT_PORT.printf_P(PSTR("Upload: START, filename: %s\r\n"), upload.filename.c_str());
    }

  } else if(upload.status == UPLOAD_FILE_WRITE){
    if(uploadFile) {
      uploadFile.write(upload.buf, upload.currentSize);
    }
    
  } else if(upload.status == UPLOAD_FILE_END){
    if(uploadFile){
      uploadFile.close();
      DBG_OUTPUT_PORT.printf_P(PSTR("Upload: END, Size: %d\r\n"), upload.totalSize);
      if(upload.filename.equals("/config.txt")){
        uploadFile = SD.open(upload.filename.c_str(), FILE_READ);
        hashFile(configSHA256, uploadFile);
        uploadFile.close();
        server.sendHeader("X-configSHA256", base64encode(configSHA256, 32));
      }
    }
  }
}

void handleSpiffsUpload(){
  trace(T_WEB,11);
  HTTPUpload& upload = server.upload();
  if(upload.status == UPLOAD_FILE_START){

    if( ! authenticate(authAdmin)) return;
      DBG_OUTPUT_PORT.printf_P(PSTR("Upload: START, filename: %s\r\n"), upload.filename.c_str());
      spiffsWrite(upload.filename.substring(11).c_str(), "", 0);        // Create a null file

  } else if(upload.status == UPLOAD_FILE_WRITE){
      spiffsWrite(upload.filename.substring(11).c_str(), upload.buf, upload.currentSize, true);   // append to the file (true)

  } else if(upload.status == UPLOAD_FILE_END){
      DBG_OUTPUT_PORT.printf_P(PSTR("Upload: END, Size: %d\r\n"), upload.totalSize);
  }
}

void deleteRecursive(String path){
  File file = SD.open((char *)path.c_str());
  if(!file.isDirectory()){
    file.close();
    SD.remove((char *)path.c_str());
    return;
  }

  file.rewindDirectory();
  while(true) {
    File entry = file.openNextFile();
    if (!entry) break;
    String entryPath = path + "/" +entry.name();
    if(entry.isDirectory()){
      entry.close();
      deleteRecursive(entryPath);
    } else {
      entry.close();
      SD.remove((char *)entryPath.c_str());
    }
    yield();
  }

  SD.rmdir((char *)path.c_str());
  file.close();
}

void handleDelete(){
  trace(T_WEB,9); 
  if(server.args() == 0) return returnFail("BAD ARGS");
  String path = server.arg(0);
  if(path.startsWith(F("/esp_spiffs"))){
    spiffsRemove(path.substring(11).c_str());
    returnOK();
    return;
  } 
    if(path == "/" || !SD.exists((char *)path.c_str())) {
    returnFail("BAD PATH");
    return;
  }
  if(path == F("/config.txt") ||
     path.startsWith(IotaLogFile) ||
     path.startsWith(historyLogFile)){
    returnFail("Restricted File");
    return;
  }
  deleteRecursive(path);
  returnOK();
}

void handleCreate(){
  trace(T_WEB,10); 
  if(server.args() == 0) return returnFail("BAD ARGS");
  String path = server.arg(0);

  if(path.startsWith(F("/esp_spiffs"))){
    if(spiffsFileExists(path.substring(11).c_str())) return returnFail("BAD PATH");
    spiffsWrite(path.substring(11).c_str(),"");
    returnOK();
    return;
  } 

  if(path == "/" || SD.exists((char *)path.c_str())) {
    returnFail("BAD PATH");
    return;
  }

  if(path.indexOf('.') > 0){
    File file = SD.open((char *)path.c_str(), FILE_WRITE);
    if(file){
      file.write((const char *)0);
      file.close();
    }
  } else {
    SD.mkdir((char *)path.c_str());
  }
  returnOK();
}

void printDirectory() {
  trace(T_WEB,7); 
  if(!server.hasArg("dir")) return returnFail("BAD ARGS");
  String response;
  String path = server.arg("dir");
  if(path.startsWith("/esp_spiffs")){
    response = spiffsDirectory(path.substring(11));
  }
  else {
    if(path != "/" && !SD.exists((char *)path.c_str())) return returnFail("BAD PATH");
    File dir = SD.open((char *)path.c_str());
    if(!dir.isDirectory()){
      dir.close();
      return returnFail("NOT DIR");
    }
    DynamicJsonBuffer jsonBuffer;
    JsonArray& array = jsonBuffer.createArray();
    dir.rewindDirectory();
    File entry;
    while(entry = dir.openNextFile()){
      JsonObject& object = jsonBuffer.createObject();
      object["type"] = (entry.isDirectory()) ? "dir" : "file";
      object["name"] = String(entry.name());
      array.add(object);
      entry.close();
    }
    dir.close();
    if(path == "/"){
      JsonObject& object = jsonBuffer.createObject();
      object["type"] = "dir";
      object["name"] = "esp_spiffs";
      array.add(object);
    }
    array.printTo(response);
  }  
  server.send(200, appJson_P, response);
}

void printSpiffsDirectory(String path) {
  trace(T_WEB,7); 
  if(path != "/" && !SD.exists((char *)path.c_str())) return returnFail("BAD PATH");
  File dir = SD.open((char *)path.c_str());
  path = String();
  if(!dir.isDirectory()){
    dir.close();
    return returnFail("NOT DIR");
  }
  DynamicJsonBuffer jsonBuffer;
  JsonArray& array = jsonBuffer.createArray();
  dir.rewindDirectory();
  File entry;
  while(entry = dir.openNextFile()){
    JsonObject& object = jsonBuffer.createObject();
    object["type"] = (entry.isDirectory()) ? "dir" : "file";
    object["name"] = String(entry.name());
    array.add(object);
    entry.close();
  }
  String response = "";
  array.printTo(response);
  server.send(200, appJson_P, response);
  dir.close();
}

/************************************************************************************************
 * 
 * Following handlers added to WebServer for IotaWatt specific requests
 * 
 **********************************************************************************************/

void handlePasswords(){
  trace(T_WEB,21);
  int len = server.arg("plain").length();
  char* buff = new char[(len + 2) * 3 / 4];
  len = base64_decode_chars(server.arg("plain").c_str(), len, buff);
  buff[len] = 0;
  String body = buff;
  delete[] buff;
  DynamicJsonBuffer Json;
  JsonObject& request = Json.parseObject(body);
  if( ! request.success()){
    server.send(400, txtPlain_P, F("Json parse failed."));
    return;
  }
  if(adminH1){
    String testH1 = calcH1("admin", deviceName, request["oldadmin"].as<char*>());
    if( adminH1 && ! testH1.equals(bin2hex(adminH1,16))){
      server.send(400, txtPlain_P, F("Current admin password invalid."));
      return;
    }
  }
  
  if(request.containsKey(F("newadmin"))){
    delete[] adminH1;
    adminH1 = nullptr;
    delete[] userH1;
    userH1 = nullptr;
    String newAdmin = request[F("newadmin")].as<char*>();
    if(newAdmin.length()){
      String newAdminH1 = calcH1("admin", deviceName, request["newadmin"].as<char*>());
      adminH1 = new uint8_t[16];
      hex2bin(adminH1, newAdminH1.c_str(), 16);
    } 
    if(request.containsKey(F("newuser"))){
      String newAdmin = request["newuser"].as<char*>();
      if(newAdmin.length()){
        String newUserH1 = calcH1("user", deviceName, request["newuser"].as<char*>());
        userH1 = new uint8_t[16];
        hex2bin(userH1, newUserH1.c_str(), 16);
      } 
    }
    if(authSavePwds()){
      server.send(200, txtPlain_P, F("Passwords reset."));
      log("New passwords saved.");
    } else {
      server.send(400, txtPlain_P, F("Error saving passwords."));
      log("Password save failed.");
    } 
  } else {
    server.send(200, txtPlain_P, "");
  } 
  return;
}

void handleStatus(){
  trace(T_WEB,0); 
  DynamicJsonBuffer jsonBuffer;
  JsonObject& root = jsonBuffer.createObject();

  if(server.hasArg(F("device"))){
    JsonObject& device = jsonBuffer.createObject();
    device.set(F("name"), deviceName);
    device.set(F("timediff"), localTimeDiff);
    device.set(F("allowdst"), timezoneRule?true:false);
    device.set(F("update"), updateClass);
    root.set(F("device"),device);
  }
  
  if(server.hasArg(F("stats"))){
    trace(T_WEB,14);
    JsonObject& stats = jsonBuffer.createObject();
    stats.set(F("cyclerate"), samplesPerCycle);
    trace(T_WEB,14);
    stats.set(F("chanrate"),cycleSampleRate);
    trace(T_WEB,14);
    stats.set(F("runseconds"), UTCtime()-programStartTime);
    trace(T_WEB,14);
    stats.set(F("stack"),ESP.getFreeHeap());
    trace(T_WEB,14);
    stats.set(F("version"),IOTAWATT_VERSION);
    trace(T_WEB,14);
    stats.set(F("frequency"),frequency);
    trace(T_WEB,14);
    stats.set(F("lowbat"), RTClowBat);
    root.set(F("stats"),stats);
  }
  
  if(server.hasArg(F("inputs"))){
    trace(T_WEB,15);
    JsonArray& channelArray = jsonBuffer.createArray();
    for(int i=0; i<maxInputs; i++){
      if(inputChannel[i]->isActive()){
        JsonObject& channelObject = jsonBuffer.createObject();
        channelObject.set(F("channel"),inputChannel[i]->_channel);
        if(inputChannel[i]->_type == channelTypeVoltage){
          channelObject.set(F("Vrms"),statRecord.accum1[i]);
          channelObject.set(F("Hz"),statRecord.accum2[i]);
          channelObject.set("phase", inputChannel[i]->getPhase(inputChannel[i]->dataBucket.volts));
        }
        else if(inputChannel[i]->_type == channelTypePower){
          if(statRecord.accum1[i] > -2 && statRecord.accum1[i] < 2) statRecord.accum1[i] = 0;
          channelObject.set(F("Watts"),String(statRecord.accum1[i],0));
          double pf = statRecord.accum2[i];
          if(pf != 0){
            pf = statRecord.accum1[i] / pf;
          }
          channelObject.set("Pf",pf);
          if(inputChannel[i]->_reversed){
            channelObject.set(F("reversed"),true);
          }
          double volts = inputChannel[inputChannel[i]->_vchannel]->dataBucket.volts;
          double amps = (volts < 50) ? 0 : inputChannel[i]->dataBucket.VA / volts;
          channelObject.set("phase", inputChannel[i]->getPhase(amps));
          channelObject.set("lastphase", inputChannel[i]->_lastPhase);
        }
        channelArray.add(channelObject);
      }
    }
    root["inputs"] = channelArray;
  }

  if(server.hasArg(F("outputs"))){
    trace(T_WEB,16);
    JsonArray& outputArray = jsonBuffer.createArray();
    Script* script = outputs->first();
    while(script){
      JsonObject& channelObject = jsonBuffer.createObject();
      channelObject.set(F("name"),script->name());
      channelObject.set(F("units"),script->getUnits());
      double value = script->run((IotaLogRecord*)nullptr, &statRecord, 1.0);
      channelObject.set(F("value"),value);
      outputArray.add(channelObject);
      script = script->next();
    }
    root["outputs"] = outputArray;
  }

  if(server.hasArg(F("influx"))){
    trace(T_WEB,17);
    JsonObject& influx = jsonBuffer.createObject();
    influx.set(F("running"),influxStarted);
    influx.set(F("lastpost"),influxLastPost);  
    root["influx"] = influx;
  }

  if(server.hasArg(F("emon"))){
    trace(T_WEB,22);
    JsonObject& emon = jsonBuffer.createObject();
    emon.set(F("running"),EmonStarted);
    emon.set(F("lastpost"),EmonLastPost);  
    root["emon"] = emon;
  }

  if(server.hasArg(F("pvoutput"))){
    trace(T_WEB,23);
    JsonObject& status = jsonBuffer.createObject();
    if(!pvoutput){
      status.set(F("state"),"stopped");
    } else {
      pvoutput->getStatusJson(status);
    }
    root["pvoutput"] = status;
  }

  if(server.hasArg(F("datalogs"))){
    trace(T_WEB,17);
    JsonObject& datalogs = jsonBuffer.createObject();
    JsonObject& currlog = jsonBuffer.createObject();
    currlog.set(F("firstkey"),currLog.firstKey());
    currlog.set(F("lastkey"),currLog.lastKey());
    currlog.set(F("size"),currLog.fileSize());
    currlog.set(F("interval"),currLog.interval());
    //currlog.set("wrap",currLog._wrap ? true : false);
    datalogs.set(F("currlog"),currlog);
    JsonObject& histlog = jsonBuffer.createObject();
    histlog.set(F("firstkey"),histLog.firstKey());
    histlog.set(F("lastkey"),histLog.lastKey());
    histlog.set(F("size"),histLog.fileSize());
    histlog.set(F("interval"),histLog.interval());
    //histlog.set("wrap",histLog._wrap ? true : false);
    datalogs.set(F("histlog"),histlog);
    root.set(F("datalogs"),datalogs);
  }

  if(server.hasArg(F("passwords"))){
    trace(T_WEB,18);
    JsonObject& passwords = jsonBuffer.createObject();
    passwords.set(F("admin"),adminH1 != nullptr);
    passwords.set(F("user"),userH1 != nullptr);  
    root[F("passwords")] = passwords;
  }

  String response = "";
  root.prettyPrintTo(response);
  server.send(200, txtJson_P, response);  
}

void handleVcal(){
  trace(T_WEB,1); 
  if( ! (server.hasArg(F("channel")) && server.hasArg("cal"))){
    server.send(400, txtJson_P, F("Missing parameters"));
    return;
  }
  DynamicJsonBuffer jsonBuffer;
  JsonObject& root = jsonBuffer.createObject();
  int channel = server.arg(F("channel")).toInt();
  float Vrms = sampleVoltage(channel, server.arg("cal").toFloat());
  root.set("vrms",Vrms);
  String response = "";
  root.printTo(response);
  server.send(200, txtJson_P, response);  
}

void handleCommand(){
  trace(T_WEB,2); 
  if(server.hasArg(F("restart"))) {
    trace(T_WEB,3); 
    server.send(200, "text/plain", "ok");
    log("Restart command received.");
    delay(500);
    ESP.restart();
  }
  if(server.hasArg(F("vtphase"))){
    trace(T_WEB,4); 
    uint16_t chan = server.arg("vtphase").toInt();
    int refChan = 0;
    if(server.hasArg(F("refchan"))){
      refChan = server.arg(F("refchan")).toInt();
    }
    uint16_t shift = 100;
    if(server.hasArg(F("shift"))){
      shift = server.arg(F("shift")).toInt();
    }
    char response[100];
    sprintf_P(response, PSTR("Phase Shift chan %d ref %d, %.2f"), chan, refChan, samplePhase(refChan, chan, shift));
    server.send(200, txtPlain_P, response);
    return; 
  }
  if(server.hasArg(F("sample"))){
    trace(T_WEB,5); 
    uint16_t chan = server.arg(F("sample")).toInt();
    sampleCycle(inputChannel[0], inputChannel[chan]);
    getSamples();
    return; 
  }
  if(server.hasArg(F("disconnect"))) {
    trace(T_WEB,6); 
    server.send(200, txtPlain_P, "ok");
    log("Disconnect command received.");
    WiFi.disconnect(false);
    return;
  }
  if(server.hasArg(F("deletelog"))) {
    trace(T_WEB,21); 
    String arg = server.arg(F("deletelog"));
    log("deletelog=%s command received.", arg.c_str());
    if(arg == "current"){
      trace(T_WEB,21); 
      currLog.end();
      deleteRecursive(String(IotaLogFile) + ".log");
      deleteRecursive(String(IotaLogFile) + ".ndx");
    } 
    else if(arg == "history"){
      trace(T_WEB,22); 
      histLog.end();
      deleteRecursive(String(historyLogFile) + ".log");
      deleteRecursive(String(historyLogFile) + ".ndx");
    }
    else if(arg == "both"){
      trace(T_WEB,23);
      currLog.end();
      deleteRecursive(String(IotaLogFile) + ".log");
      deleteRecursive(String(IotaLogFile) + ".ndx");
      histLog.end();
      deleteRecursive(String(historyLogFile) + ".log");
      deleteRecursive(String(historyLogFile) + ".ndx");
    }
    else {
      server.send(400, txtPlain_P, F("Specify current, history, or both."));
      return;
    }
    server.send(200, txtPlain_P, "ok");
    delay(1000);
    ESP.restart();
  }
  server.send(400, txtPlain_P, F("Unrecognized request"));
}

void handleGetFeedList(){ 
  trace(T_WEB,18);
  DynamicJsonBuffer jsonBuffer;
  JsonArray& array = jsonBuffer.createArray();
  for(int i=0; i<maxInputs; i++){
    if(inputChannel[i]->isActive()){
      if(inputChannel[i]->_type == channelTypeVoltage){
        JsonObject& voltage = jsonBuffer.createObject();
        voltage["id"] = String("IV") + String(inputChannel[i]->_name);
        voltage["tag"] = F("Voltage");
        voltage["name"] = inputChannel[i]->_name;
        array.add(voltage);
      } 
      else
        if(inputChannel[i]->_type == channelTypePower){
        JsonObject& power = jsonBuffer.createObject();
        power["id"] = String("IP") + String(inputChannel[i]->_name);
        power["tag"] = F("Power");
        power["name"] = inputChannel[i]->_name;
        array.add(power);
        JsonObject& energy = jsonBuffer.createObject();
        energy["id"] = String("IE") + String(inputChannel[i]->_name);
        energy["tag"] = F("Energy");
        energy["name"] = inputChannel[i]->_name;
        array.add(energy);
      }
    }
  }
  trace(T_WEB,18);
  Script* script = outputs->first();
  int outndx = 100;
  while(script){
    if(String(script->name()).indexOf(' ') == -1){
      String units = script->getUnits();
      if(units.equalsIgnoreCase("volts")){
        JsonObject& voltage = jsonBuffer.createObject();
        voltage["id"] = String("OV") + String(script->name());
        voltage["tag"] = F("Voltage");
        voltage["name"] = script->name();
        array.add(voltage);
      } 
      else if(units.equalsIgnoreCase("watts")) {
        JsonObject& power = jsonBuffer.createObject();
        power["id"] = String("OP") + String(script->name());
        power["tag"] = F("Power");
        power["name"] = script->name();
        array.add(power);
        JsonObject& energy = jsonBuffer.createObject();
        energy["id"] = String("OE") + String(script->name());
        energy["tag"] = F("Energy");
        energy["name"] = script->name();
        array.add(energy);
      }
      else {
        JsonObject& other = jsonBuffer.createObject();
        other["id"] = String("OO") + String(script->name());
        other["tag"] = F("Outputs");
        other["name"] = script->name();
        array.add(other);
      }
    }
    script = script->next();
  }
  
  String response;
  array.printTo(response);
  server.send(200, appJson_P,response);
}

void handleGetFeedData(){
  serverAvailable = false;
  getFeedData();
  return;
}

// Had to roll our own streamFile function so we can set the actual partial
// file length rather than the total file length.  Safari won't work otherwise.
// No big deal.  BTW/ This instance of Client.send is depricated in the newer
// ESP8266WiFiClient, so probably change at some point. (Remove buffer size parameter).

void sendMsgFile(File &dataFile, int32_t relPos){
    trace(T_WEB,20);
    int32_t absPos = relPos;
    if(relPos < 0) absPos = dataFile.size() + relPos;
    dataFile.seek(absPos);
    while(dataFile.available()){
      if(dataFile.read() == '\n') break;
    }
    absPos = dataFile.position();
    server.setContentLength(dataFile.size() - absPos);
    server.send(200, txtPlain_P, "");
    WiFiClient _client = server.client();
    _client.write(dataFile);
}

void handleGetConfig(){
  trace(T_WEB,8); 
  if(server.hasArg(F("update"))){
    if(server.arg(F("update")) == "restart"){
      server.send(200, F("text/plain"), "OK");
      log("Restart command received.");
      delay(500);
      ESP.restart();
    }
    else if(server.arg(F("update")) == "reload"){
      getConfig(); 
      server.send(200, txtPlain_P, "OK");
      return;  
    }
  }
  server.send(400, txtPlain_P, "Bad Request.");
}

void handleQuery(){
  CSVquery* query = new CSVquery();
  if( ! query->setup()){
    server.send(400, txtPlain_P, "Bad Request.");
  } else {
    server.setContentLength(CONTENT_LENGTH_UNKNOWN);
    if(server.hasArg(F("download"))){
      server.send(200,"application/octet-stream","");
    }
    else if(query->isJson()){
      server.send(200, appJson_P, "");
    }
    else {
      server.send(200, txtPlain_P, "");
    }
      
    uint8_t* buf = new uint8_t[1460];
    int read = 0;
    size_t size = 0;
    while(read = query->readResult(buf+6, 1460-8)){
      sendChunk((char*)buf, read+6);
      size += read;
      yield();
    }
    sendChunk((char*)buf, 6);
    delete buf;
  }
  delete query;
}

void handleDSTtest(){
    uint32_t begin = server.arg(F("begin")).toInt();
    uint32_t end = server.arg(F("end")).toInt();
    xbuf buf;
    while(begin <= end){
      buf.printf_P(PSTR("UTC %d, %s, Local %d, %s, UTC %d, %s"),
        begin, datef(begin).c_str(),
        localTime(begin), datef(localTime(begin)).c_str(),
        UTCtime(localTime(begin)), datef(UTCtime(localTime(begin))).c_str());
      if(begin != UTCtime(localTime(begin))){
        buf.print(" FAILED");
      }
      buf.println();
      begin += 60;
      if(buf.available() > 8000){
        buf.println("too big");
        break;
      }
    }
    server.send(200, txtPlain_P, buf.readString(buf.available()).c_str());
}
        // Seems to work better when sending chunk as a single write
        // including chunk header, body, and footer (\r\n).
        // This function accepts a char* buffer and length to send.
        // Buffer must have 6 bytes free at start for header and
        // must be long enough to add two byte footer.
        // bufPos is end of body (chunksize+6).

size_t sendChunk(char* buf, size_t bufPos){
  sprintf(buf,"%04x\r",bufPos-6);
  *(buf+5) = '\n';
  memcpy(buf+bufPos,"\r\n",2);
  return server.client().write(buf,bufPos+2);
}