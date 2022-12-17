#include <ESP8266WiFi.h>
#include "FS.h"
#include <ESPAsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClient.h>
#include <string.h>

bool initWiFi(String ssid, String password, int timeOut = 60000);
void reconnection(String ssid, String password, int timeOut = 300000);
void logSystem(String str, String func = "");

const char *ssid = "ESP8266";
const char *password = "987654321";
IPAddress local_IP(192,168,4,22);
IPAddress gateway(192,168,4,9);
IPAddress subnet(255,255,255,0);

String dad[200];
String son[200];
int son_id[200];
int sonID_x_dad[200];

int son_index = 0;
int dad_index = 1;

String checkBox = "";

bool ap_mode = true;
bool wifi_seting = false;
int recon_try_count = 0;
int recon_try_count_max = 30;

//Variables to save values from HTML form
String ssid_;
String pass_;

// File paths to save input values permanently
const char* ssidPath = "/ssid.txt";
const char* passPath = "/pass.txt";
const char* displayPath = "/display.json";

// Search for parameter in HTTP POST request
const char* PARAM_INPUT_1 = "ssid";
const char* PARAM_INPUT_2 = "pass";
const char* PARAM_INPUT_3 = "scan";
const char* PARAM_INPUT_4 = "";

const char* host = "10.0.0.189";

String displayData = "";
String file_three_html = "";
String file_three_leaf_init_1 = "<input type=\"checkbox\" value=\"";
String file_three_leaf_init_2 = "\">";
String file_three_leaf_end = "<br>";
long long int  startTimeToRevifyConnection;
long long int time_to_request;
long long int interval_request_rewrite = 60000;
long long int  mills;
int time_add_ap_mode_startTimeToRevifyConnection = 300000; // 5 min
unsigned long intervalTimeToVerifyConnection = 60000; // 1 min

bool restart = false;

String scan = "";
String listSSID = "";



AsyncWebServer  server(80);
WiFiClient client;

void setup()
{
 	Serial.begin(115200);

  if(!SPIFFS.begin()){
    logSystem("An Error has occurred while mounting SPIFFS", "setup");
    return;
  }

  // Load values saved in SPIFFS
  wifi_seting = loadWiFiSettings();

  displayData = readFile(displayPath);

  logSystem("Display: " + displayData,  "setup");

  if ( !wifi_seting || !initWiFi(ssid_, pass_)){
    startTimeToRevifyConnection = millis() + time_add_ap_mode_startTimeToRevifyConnection;
    if (!configWiFiMode()){
      ESP.restart();
    }
  }else{
    startTimeToRevifyConnection = millis();
  }
  time_to_request =  0;

  
}

void loop() {
 	if(restart){
    ESP.restart();
  }
  reconnection(ssid_, pass_);
  if(millis() - time_to_request > interval_request_rewrite){
    time_to_request = millis();
    //requestData();
  }
}

bool loadWiFiSettings(){
  ssid_ =  "";

  ssid_ = readFile(ssidPath);
  pass_ = readFile(passPath);
  
  if (ssid_ == ""){
    return false;
  }else{
    logSystem("Network name saved: " + String(ssid_), "loadWiFiSettings");
  }

  return true;
}

//log system
void logSystem(String str, String func){

  unsigned long runMillis= millis();
  unsigned long allSeconds=millis()/1000;
  int runHours= allSeconds/3600;
  int secsRemaining=allSeconds%3600;
  int runMinutes=secsRemaining/60;
  int runSeconds=secsRemaining%60;

  char buf[21];
  sprintf(buf,"%02d:%02d:%02d",runHours,runMinutes,runSeconds);
  Serial.print(buf);
  Serial.print("> ");
  Serial.print(str);
  Serial.print(" :");
  Serial.println(func);
}

// Start AP mode
bool configWiFiMode(){

  // Set WiFi to station mode
  // Set WiFi to station mode
  WiFi.mode(WIFI_STA);
  // Disconnect from an AP if it was previously connected
  WiFi.disconnect();
  delay(100);

  int numberOfNetworks = WiFi.scanNetworks();

  for(int i =0; i<numberOfNetworks; i++){

      scan += "<tr><th>"+String(WiFi.SSID(i))+"</th><th>"+String(WiFi.RSSI(i))+"</th></tr>";
      listSSID += "<option>"+ String(WiFi.SSID(i))+ "</option>";
      logSystem("Network name: " + String(WiFi.SSID(i)) + "\nSignal strength: " + String(WiFi.RSSI(i)) + "\n-----------------------", "configWiFiMode");

  }

  WiFi.mode(WIFI_AP);
  ap_mode = true;
  String str_aux = "Setting soft-AP configuration ... ";
  if (WiFi.softAPConfig(local_IP, gateway, subnet))
    str_aux += "Ready";
  else
    str_aux += "Fail!";
 	
  logSystem(str_aux, "configWiFiMode");  
  str_aux = "Setting soft-AP ... ";
  if (WiFi.softAP(ssid))
    str_aux += "Ready";
  else
    str_aux += "Fail!";
  logSystem(str_aux, "configWiFiMode");  

  IPAddress ip = WiFi.localIP();
  String Ip = String(ip[0])+'.'+ String(ip[1])+'.'+ String(ip[2])+'.'+ String(ip[3]);
 	logSystem("Soft-AP IP address = " + Ip);

  server.begin();
  logSystem("Servidor iniciado"); 
  IPAddress ip_ap = WiFi.softAPIP();
  String ip_AP = String(ip_ap[0])+'.'+ String(ip_ap[1])+'.'+ String(ip_ap[2])+'.'+ String(ip_ap[3]);
  logSystem("IP para se conectar ao NodeMCU: http://" + ip_AP); 

  String index = readFile("/wifimanager.html");
  if(index == ""){
    return false;
  }

  int indexOf = index.indexOf("</table>");
  String pt1 = index.substring(0, indexOf);
  String pt2 = index.substring(indexOf);
 
  String HTML = pt1 + scan + pt2;
  
  indexOf = HTML.indexOf("</datalist>");
  pt1 = HTML.substring(0, indexOf);
  pt2 = HTML.substring(indexOf);

  HTML = pt1 + listSSID + pt2;

  // Route to load style.css file
  server.on("/style.css", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(SPIFFS, "/style.css", "text/css");
  });
  
  server.on("/", HTTP_GET, [HTML](AsyncWebServerRequest *request){
    request->send(200, "text/html", HTML);
  });

  server.on("/", HTTP_POST, [](AsyncWebServerRequest *request) {
      int params = request->params();
      for(int i=0;i<params;i++){
        AsyncWebParameter* p = request->getParam(i);
        if(p->isPost()){
          // HTTP POST ssid value
          if (p->name() == PARAM_INPUT_1) {
            ssid_ = p->value().c_str();
            logSystem("SSID set to: "+String(ssid_));
            writeFile(SPIFFS, ssidPath, ssid_);
            // Write file to save value

          }
          // HTTP POST pass value
          if (p->name() == PARAM_INPUT_2) {
            pass_ = p->value().c_str();
            logSystem("Password set ");
            writeFile(SPIFFS, passPath, pass_);
            // Write file to save value
          }

          if (p->name() == PARAM_INPUT_3) {
            logSystem("SCAN");
            
            // Write file to save value
          }
          //Serial.printf("POST[%s]: %s\n", p->name().c_str(), p->value().c_str());
        }
      }
      request->redirect("/");
      restart = true;
    });

  server.begin();
  return true;
  
}

// Read File from SPIFFS
String readFile(String path){

  File file = SPIFFS.open(path, "r");
  if(!file){
    logSystem("- failed to open file for reading "+ path );
    return "";
  }
  
  String fileContent;
  while(file.available()){
    fileContent += file.readStringUntil(EOF);
  }
  return fileContent;
}

// timeout de 5 minuto
bool initWiFi(String ssid, String password, int timeOut) {
  unsigned long timeStart = millis();
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  logSystem("Connecting to WiFi ..");
  while (WiFi.status() != WL_CONNECTED && (millis() - timeStart) < timeOut) {
    logSystem(".");
    delay(1000);
  }

  if((millis() - timeStart) >= timeOut){
    logSystem("Connecting to WiFi FAIL");
    return false;
  }
  ap_mode = false;
  logSystem("Connecting to WiFi success");
  IPAddress ip_ap = WiFi.localIP();
  String ip = String(ip_ap[0])+'.'+ String(ip_ap[1])+'.'+ String(ip_ap[2])+'.'+ String(ip_ap[3]);
  logSystem("Device IP: " + ip);
  WiFi.setAutoReconnect(true);
  WiFi.persistent(true);
  requestData();
  String index_ = readFile("/index_origin.html");
  if(index_ == ""){
    logSystem("index_origin dont find",  "error");
  }
  int indexOf_ = index_.indexOf("<!--here-->");
  String pt1_ = index_.substring(0, indexOf_);
  String pt2_ = index_.substring(indexOf_);
  String HTML_ = pt1_ + mountCheckBox() + pt2_;
  writeFile(SPIFFS, "/index.html", HTML_);

  // Route to load style.css file
  server.on("/w.png", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(SPIFFS, "/w.png", "text/png");
  });

  server.on("/i.png", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(SPIFFS, "/i.png", "text/png");
  });

  server.on("/hw.png", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(SPIFFS, "/hw.png", "text/png");
  });

  server.on("/index.css", HTTP_GET, [](AsyncWebServerRequest *request){

    request->send(SPIFFS, "/index.css", "text/css");
  });
  
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
      request->send(SPIFFS, "/index.html", "text/html");
  });

  server.on("/", HTTP_POST, [](AsyncWebServerRequest *request) {
      int params = request->params();
      for(int i=0;i<params;i++){
        AsyncWebParameter* p = request->getParam(i);
        if(p->isPost()){
          logSystem(p->name() +": "+ p->value(), "Server.POST");
          if(p->name() == "restart" && p->value() == "true"){
            ESP.restart();
          }
        }
      }
      request->redirect("/");
    });

  server.on("/display", HTTP_POST, [](AsyncWebServerRequest *request) {
      int params = request->params();
      DynamicJsonDocument doc(1024);
      String str_ant = "";
      doc["nome"] = "display_screen";
      for(int i=0;i<params;i++){
        AsyncWebParameter* p = request->getParam(i);
        if(p->isPost()){
          if(p->value() != "Submit" && p->value() != "Refresh" && p->value() != "0"){
            doc["id"] [i]  = p->value();
          }else if (p->value() == "Refresh"){
            request->redirect("/");
            restart = true;
            return;
          }
        }
      }
      displayData = "";
      serializeJson(doc, displayData);
      writeFile(SPIFFS, "/display.json", displayData);
      logSystem(displayData, "update display data");
      request->redirect("/");
    });

  // Inicializa o servidor
  server.begin();
  logSystem("Web Server iniciado");

  return true;
}

// verify connection status
String connectionStatus() {
  
  switch (WiFi.status()){
    case WL_NO_SSID_AVAIL:
      return "WL_NO_SSID_AVAIL";
      break;
    case WL_CONNECTED:
      return "WL_CONNECTED";
      break;
    case WL_CONNECT_FAILED:
      return "WL_CONNECT_FAILED";
      break;
  }
  return "NULL";

}

// verify the wifi status and handle the response
//  fail to connect to wifi: try more (recon_try_count_max) * times
//  no ssid available: Restart and enter in AP mode.
void reconnection(String ssid, String password, int timeOut){
  mills = millis();
  if( wifi_seting && mills - startTimeToRevifyConnection > intervalTimeToVerifyConnection){
    if(ap_mode){
      logSystem("try to connect", "loop"); 
      if (!initWiFi(ssid_, pass_)){
        if (!configWiFiMode()){
          ESP.restart();
        }
      }
      startTimeToRevifyConnection = millis() + time_add_ap_mode_startTimeToRevifyConnection;
    }else{
      String receiv = connectionStatus();
      if(receiv == "WL_CONNECT_FAILED" ){
        logSystem("try to reconnect", "loop"); 
        //try to reconnect while we receive a fail response or pass the limits
        while (!initWiFi(ssid, password) && recon_try_count < recon_try_count_max){
          recon_try_count ++;
        }
        if(recon_try_count >= recon_try_count_max){
          ESP.restart();
        }else{
          recon_try_count = 0;
        }
      }else if (receiv == "WL_NO_SSID_AVAIL"){
        ESP.restart();
      }
      startTimeToRevifyConnection = millis(); // reset timer
    }
  }
  
}

// Write file to SPIFFS
void writeFile(fs::FS &fs, const char *  path, String  message){
  logSystem("Writing file: "+ String(path));

  File file = fs.open(path, "w");
  if(!file){
    logSystem("- failed to open file for writing");
    return;
  }
  if(file.print(message)){
   logSystem("- file written");
  } else {
   logSystem("- frite failed");
  }
}

void requestData(){
  
    Serial.print("connecting to ");
    Serial.println(host);

    // Use WiFiClient class to create TCP connections
    
    const int httpPort = 8085;
    if (!client.connect(host, httpPort)) {
        Serial.println("connection failed");
        return;
    }

    // We now create a URI for the request
    String url = "/data.json";

    Serial.print("Requesting URL: ");
    Serial.println(url);

    // This will send the request to the server
    client.print(String("GET ") + url + " HTTP/1.1\r\n" + "Host: " + host + "\r\n" + "Connection: close\r\n\r\n");
    skipResponseHeaders();
    
    disconnect();
    
    Serial.println();
    Serial.println("closing connection");
}

bool skipResponseHeaders() {
  // HTTP headers end with an empty line
  char endOfHeaders[] = "\r\n\r\n";

  client.setTimeout(1000);
  bool ok = client.find(endOfHeaders);

  if (!ok) {
    Serial.println("No response or invalid response!");
  }else{
    Serial.println("OK");
    DynamicJsonDocument doc(18384);
    DeserializationError  err = deserializeJson(doc, client ,DeserializationOption::NestingLimit(20));
    if (err) {
        Serial.print(F("deserializeJson() failed: "));
        Serial.println(err.c_str());
    }else{
        String output;
        JsonObject obj = doc.as<JsonObject>();
        file_three_html = "";
        //file_three_html = file_three_root_init;
        mountFileThree(obj, "");
        //file_three_html += file_three_root_end;
        //logSystem(mountCheckBox());
    }
  }
  return ok;
}

// Close the connection with the HTTP server
void disconnect() {
  Serial.println("Disconnect");
  client.stop();
}


void showJSON(JsonObject obj){
  for(int i = 0;  ;i++){
    String text = obj["Children"][i]["Text"];
    if (text == "null"){
      return;
    }else{
      logSystem(text, "json");
      showJSON(obj["Children"][i]);
    }
  }
}

void mountFileThree(JsonObject obj, String pai){
  for(int i = 0;  ;i++){
    String text = obj["Children"][i]["Text"];
    String min = obj["Min"];
    String max = obj["Max"];
    String value = obj["Value"];
    if (text == "null" ){
      if ( min != "Min" && min != "" && max != "Max" && max != "" && value != "Value" && value != "" ){
        int dad_i = findDadId(pai);
        if(dad_i < 0 ){
          dad_index = addDad(pai, dad_index);
          dad_i = dad_index - 1;
        }
        son_index = addSon(String(obj["Text"]), obj["id"], son_index);
        sonID_x_dad[(int)obj["id"]] = dad_i;
      }
      return;
    }else{
      mountFileThree(obj["Children"][i], String(pai) + "/"  + String(obj["Text"]));
    }
  }
}

int findDadId( String dad_aux){
  for(int i = 0; i< 200 ; i ++){
    if( dad_aux == dad[i]){
        return i;
    }
  }
  return -1;
}

int addDad( String dad_STR, int index){
  dad[index] = dad_STR;
  return index +1;
}

int addSon(String son_name_STR, int son_id_INT, int index){
  son[index] = son_name_STR;
  son_id[index] = son_id_INT;
  return index +1;
}

int findSon( int son_id_aux){
  for(int i = 0; i< 200 ; i ++){
    if( son_id_aux == son_id[i]){
        return i;
    }
  }
  return -1;
}

void printDadSon(){
  for(int i = 0; i < 200; i++){
    if(dad[i] != ""){
      logSystem(dad[i], "dad");
      //procurar todos os seus filhos
      for(int j = 0; j < 200; j ++){
      //sonID_x_dad tem gravado o id do pai, e o index do sonID_x_dad é o id do sensor
        if(sonID_x_dad[j] == i){
          //j é o id do sensor;
          int posicao_do_nome_do_sensor = findSon(j);
          logSystem(son[posicao_do_nome_do_sensor], "son");
        }
      }
    }
  }
}


String mountCheckBox(){
  String checkBox = "";
  bool empt = true;
  for(int i = 0; i < 200; i++){
    if(dad[i] != ""){
      //logSystem(dad[i], "dad");
      checkBox += "<img class=\"iconFile\" src=\"hw.png\">" + dad[i] + "<div class=\"p1\">";
      //procurar todos os seus filhos
      for(int j = 0; j < 200; j ++){
      //sonID_x_dad tem gravado o id do pai, e o index do sonID_x_dad é o id do sensor
        if(sonID_x_dad[j] == i){
          //j é o id do sensor;
          int posicao_do_nome_do_sensor = findSon(j);
          //logSystem(son[posicao_do_nome_do_sensor], "son");
          checkBox += "<input type=\"checkbox\" name=\"item\" value=\""+ String(j) +"\">"+son[posicao_do_nome_do_sensor]+"<br>";
        }
      }
      checkBox += "</div>";
    }
  }
  return checkBox;
}