#include<ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <EEPROM.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <PubSubClient.h>
#include <WiFiClientSecure.h>




ESP8266WebServer webServer(80);
const char* mqtt_server = "cd287656d61e4f4b860ef5c1a61bcabe.s2.eu.hivemq.cloud";
const int mqtt_port = 8883;
const char* mqtt_username = "DA2_SmartHome";
const char* mqtt_password = "DA2_SmartHome";
char* ssid_ap = "Config_WiFi";
char* pass_ap = "12345678";
String ssid="";
String pass="";
IPAddress ip_ap(192,168,1,1);
IPAddress gateway_ap(192,168,1,1);
IPAddress subnet_ap(255,255,255,0);
const char MainPage[] PROGMEM = R"=====(
<!DOCTYPE html>
<html>
 <head> 
     <title>CONFIG WIFI FOR NODE</title> 
     <style> 
        body {text-align:center;background-color:#222222; color:white}
        input {
          height:25px; 
          width:270px;
          font-size:20px;
          margin: 10px auto;
        }
        #content {
          border: white solid 1px; 
          padding:5px;  
          height:600px; 
          width:330px; 
          border-radius:20px;
          margin: 0 auto
        }

        .button_wifi{
          height:50px; 
          width:90px; 
          margin:10px 0;
          outline:none;
          color:white;
          font-size:15px;
          font-weight: bold;
        }
        #button_save {
          background-color:#00BB00;
          border-radius:5px;
        }
        #button_restart {
          background-color:#FF9900;
          border-radius:5px;
        }
        #button_reset {
          background-color:#CC3300;
          border-radius:5px;
        }
     </style>
     <!-- <meta name="viewport" content="width=device-width,user-scalable=0" charset="UTF-8"> -->
 </head>
 <body> 
    <div><h1>CONFIG WIFI FOR NODE</h1></div>
    <div id="content"> 
      <div id="wifisetup" style="height:340px; font-size:20px; display:block";>
        <div style="text-align:left; width:270px; margin:5px 25px">SSID: </div>
        <div><input id="ssid"/></div>
        <div style="text-align:left; width:270px; margin:5px 25px">Password: </div>
        <div><input id="pass"/></div>
        <div>
          <button id="button_save" class="button_wifi" onclick="writeEEPROM()">SAVE</button>
          <button id="button_restart" class="button_wifi" onclick="restartESP()">RESTART</button>
          <button id="button_reset" class="button_wifi" onclick="clearEEPROM()">RESET</button>
        </div>
        <div>IP connected: <b><span id="ipconnected"></span></b></div>
        <div id="reponsetext"></div>
      </div>
    </div>
    <script>
      //-----------Hàm khởi tạo đối tượng request----------------
      function create_obj(){
        td = navigator.appName;
        if(td == "Microsoft Internet Explorer"){
          obj = new ActiveXObject("Microsoft.XMLHTTP");
        }else{
          obj = new XMLHttpRequest();
        }
        return obj;
      }
      //------------Khởi tạo biến toàn cục-----------------------------
      var xhttp = create_obj();//Đối tượng request cho setup wifi
      //===================Khởi tạo ban đầu khi load trang=============
      window.onload = function(){
        getIPconnect();//Gửi yêu cầu lấy IP kết nối
      }
      //===================IPconnect====================================
      //--------Tạo request lấy địa chỉ IP kết nối----------------------
      function getIPconnect(){
        xhttp.open("GET","/getIP",true);
        xhttp.onreadystatechange = process_ip;//nhận reponse 
        xhttp.send();
      }
      //-----------Kiểm tra response IP và hiển thị------------------
      function process_ip(){
        if(xhttp.readyState == 4 && xhttp.status == 200){
          //------Updat data sử dụng javascript----------
          ketqua = xhttp.responseText; 
          document.getElementById("ipconnected").innerHTML=ketqua;       
        }
      }
      function writeEEPROM(){
        if(Empty(document.getElementById("ssid"), "Please enter ssid!")&&Empty(document.getElementById("pass"), "Please enter password")){
          var ssid = document.getElementById("ssid").value;
          var pass = document.getElementById("pass").value;
          xhttp.open("GET","/writeEEPROM?ssid="+ssid+"&pass="+pass,true);
          xhttp.onreadystatechange = process;//nhận reponse 
          xhttp.send();
        }
      }
      function clearEEPROM(){
        if(confirm("Do you want to delete all saved wifi configurations?")){
          xhttp.open("GET","/clearEEPROM",true);
          xhttp.onreadystatechange = process;//nhận reponse 
          xhttp.send();
        }
      }
      function restartESP(){
        if(confirm("Do you want to reboot the device?")){
          xhttp.open("GET","/restartESP",true);
          xhttp.send();
          alert("Device is restarting! If no wifi is found please press reset!");
        }
      }
      //-----------Kiểm tra response -------------------------------------------
      function process(){
        if(xhttp.readyState == 4 && xhttp.status == 200){
          //------Updat data sử dụng javascript----------
          ketqua = xhttp.responseText; 
          document.getElementById("reponsetext").innerHTML=ketqua;       
        }
      }
     //----------------------------CHECK EMPTY--------------------------------
     function Empty(element, AlertMessage){
        if(element.value.trim()== ""){
          alert(AlertMessage);
          element.focus();
          return false;
        }else{
          return true;
        }
     }

    </script>
 </body> 
</html>
)=====";
WiFiClientSecure espClient;
PubSubClient client(espClient);

unsigned long lastMsg = 0;
#define MSG_BUFFER_SIZE (50)
char msg[MSG_BUFFER_SIZE];

const int out[4] = {5,4,0,2};
const int btn[4] = {14,12,13,15};
unsigned long timeDelay = millis();
boolean updateState=0;

void setup_wifi() {
  delay(10);
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);
  WiFi.begin(ssid, pass);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  randomSeed(micros());
  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
}
//------------Connect to MQTT Broker-----------------------------
void reconnect() {
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    String clientID =  "ESPClient-";
    clientID += String(random(0xffff),HEX);
    if (client.connect(clientID.c_str(), mqtt_username, mqtt_password)) {
      Serial.println("connected");
      client.subscribe("esp8266/client");
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      delay(5000);
    }
  }
}
//-----------Call back Method for Receiving MQTT massage and Switch LED---------
void callback(char* topic, byte* payload, unsigned int length) {
  String incommingMessage = "";
  for(int i=0; i<length;i++) incommingMessage += (char)payload[i];
  Serial.println("Massage arived ["+String(topic)+"]"+incommingMessage);

  DynamicJsonDocument doc(100);
  DeserializationError error = deserializeJson(doc, incommingMessage);
  if (error) {
    Serial.print("deserializeJson() failed: ");
    Serial.println(error.c_str());
    return;
  }
  JsonObject obj = doc.as<JsonObject>();
  if(obj.containsKey("out1")){
    boolean p = obj["out1"];
    digitalWrite(out[0],p);
    Serial.println("out1: "+String(p));
  }
  if(obj.containsKey("out2")){
    boolean p = obj["out2"];
    digitalWrite(out[1],p);
    Serial.println("out2: "+String(p));
  }
   if(obj.containsKey("out3")){
    boolean p = obj["out3"];
    digitalWrite(out[2],p);
    Serial.println("out3: "+String(p));
  }
   if(obj.containsKey("out4")){
    boolean p = obj["out4"];
    digitalWrite(out[3],p);
    Serial.println("out4: "+String(p));
  }
  updateState=1;
}
//-----Method for Publishing MQTT Messages---------
void publishMessage(const char* topic, String payload, boolean retained){
  if(client.publish(topic,payload.c_str(),true))
    Serial.println("Message published ["+String(topic)+"]: "+payload);
}

ICACHE_RAM_ATTR void handleBtn(){
  if(millis()-timeDelay>500){
    for(int i=0;i<4;i++){
      if(digitalRead(btn[i])==HIGH){
        digitalWrite(out[i],!digitalRead(out[i]));
      }
    }
    updateState=1;
    timeDelay=millis();
  }
}

void setup() {
  Serial.begin(9600);
  for(int i=0; i<4;i++){
    pinMode(out[i],OUTPUT);
    pinMode(btn[i],INPUT);
    attachInterrupt(btn[i],handleBtn,RISING);
  }
EEPROM.begin(512);       
  delay(100);
  if(read_EEPROM()){
    checkConnection();
  }else{
    WiFi.disconnect();
    WiFi.mode(WIFI_AP);
    WiFi.softAPConfig(ip_ap, gateway_ap, subnet_ap);
    WiFi.softAP(ssid_ap,pass_ap,1,false);
    Serial.println("Soft Access Point mode!");
    Serial.print("Please connect to");
    Serial.println(ssid_ap);
    Serial.print("Web Server IP Address: ");
    Serial.println(ip_ap);
  }
  webServer.begin();
  webServer.on("/",mainpage);
  webServer.on("/getIP",get_IP);
  webServer.on("/writeEEPROM",write_EEPROM);
  webServer.on("/restartESP",restart_ESP);
  webServer.on("/clearEEPROM",clear_EEPROM);
  while(WiFi.status()!=WL_CONNECTED){
  int count=0;
  if(count<=100){
    delay(100);
    count++;
  }
  else{
    Serial.println("Please Reconfig Wifi and Server");
    clear_EEPROM();
    WiFi.disconnect();
    WiFi.mode(WIFI_AP);
    WiFi.softAPConfig(ip_ap, gateway_ap, subnet_ap);
    WiFi.softAP(ssid_ap,pass_ap,1,false);
    Serial.println("Soft Access Point mode!");
    Serial.print("Please connect to");
    Serial.println(ssid_ap);
    Serial.print("Web Server IP Address: ");
    Serial.println(ip_ap);
  }
 }
  espClient.setInsecure();
  client.setServer(mqtt_server, mqtt_port);
  client.setCallback(callback);
}
unsigned long timeUpdata=millis();
void loop() {
  if (!client.connected()) {
    reconnect();
  }
  client.loop();
  if(updateState==1){
    DynamicJsonDocument doc(1024);
    doc["out1"]=digitalRead(out[0]);
    doc["out2"]=digitalRead(out[1]);
    doc["out3"]=digitalRead(out[2]);
    doc["out4"]=digitalRead(out[3]);
    char mqtt_message[1024];
    serializeJson(doc,mqtt_message);
    publishMessage("esp8266/out", mqtt_message, true);
    updateState=0;
  }
}
void mainpage(){
  String s = FPSTR(MainPage);
  webServer.send(200,"text/html",s);
}
void get_IP(){
  String s = WiFi.localIP().toString();
  webServer.send(200,"text/html", s);
}
boolean read_EEPROM(){
  Serial.println("Reading EEPROM...");
  if(EEPROM.read(0)!=0){
    for (int i=0; i<32; ++i){
      ssid += char(EEPROM.read(i));
    }
    Serial.print("SSID: ");
    Serial.println(ssid);
    for (int i=32; i<96; ++i){
      pass += char(EEPROM.read(i));
    }
    Serial.print("PASSWORD: ");
    Serial.println(pass);
    ssid = ssid.c_str();
    pass = pass.c_str();
    return true;
  }else{
    Serial.println("Data wifi not found!");
    return false;
  }
}
void checkConnection() {
  Serial.println();
  WiFi.disconnect();
  WiFi.mode(WIFI_STA);
  Serial.print("Check connecting to ");
  Serial.println(ssid);
  WiFi.begin(ssid,pass);
  int count=0;
  while(count < 50){
    if(WiFi.status() == WL_CONNECTED){
      Serial.println();
      Serial.print("Connected to ");
      Serial.println(ssid);
      Serial.print("Web Server IP Address: ");
      Serial.println(WiFi.localIP());
      return;
    }
    delay(200);
    Serial.print(".");
    count++;
  }
  Serial.println("Timed out.");
  clear_EEPROM();
  WiFi.disconnect();
  WiFi.mode(WIFI_AP);
  WiFi.softAPConfig(ip_ap, gateway_ap, subnet_ap);
  WiFi.softAP(ssid_ap,pass_ap,1,false);
  Serial.println("Soft Access Point mode!");
  Serial.print("Please connect to");
  Serial.println(ssid_ap);
  Serial.print("Web Server IP Address: ");
  Serial.println(ip_ap);
}
void write_EEPROM(){
  ssid = webServer.arg("ssid");
  pass = webServer.arg("pass");
  Serial.println("Clear EEPROM!");
  for (int i = 0; i < 512; ++i) {
    EEPROM.write(i, 0);           
    delay(10);
  }
  for (int i = 0; i < ssid.length(); ++i) {
    EEPROM.write(i, ssid[i]);
  }
  for (int i = 0; i < pass.length(); ++i) {
    EEPROM.write(32 + i, pass[i]);
  }

  EEPROM.commit();
  Serial.println("Writed to EEPROM!");
  Serial.print("SSID: ");
  Serial.println(ssid);
  Serial.print("PASS: ");
  Serial.println(pass);
  String s = "Wifi configuration saved!";
  webServer.send(200, "text/html", s);
}
void restart_ESP(){
  ESP.restart();
}
void clear_EEPROM(){
  Serial.println("Clear EEPROM!");
  for (int i = 0; i < 512; ++i) {
    EEPROM.write(i, 0);           
    delay(10);
  }
  EEPROM.commit();
  String s = "Device has been reset!";
  webServer.send(200,"text/html", s);
}