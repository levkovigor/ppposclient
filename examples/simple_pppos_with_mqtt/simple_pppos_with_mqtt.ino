#include <PPPOS.h>
#include <PPPOSClient.h>
#include <PubSubClient.h>

#define SERIAL_BR           115200
#define GSM_SERIAL          1
#define GSM_RX              16
#define GSM_TX              17
#define GSM_BR              115200

char* server = "example.com";
char* ppp_user = "";
char* ppp_pass = "";
String APN = "internet";

#define WEB_SERVER "www.w3.org"
#define WEB_URL "https://www.w3.org/TR/PNG/iso_8859-1.txt"
static const char *REQUEST = "GET " WEB_URL "\ HTTP/1.1\r\n"
    "Host: "WEB_SERVER"\r\n"
    "Connection: keep-alive\r\n"
    "User-Agent: esp/1.0 esp32\r\n"
    "\r\n";

String buffer = "";
char *data = (char *) malloc(1024); 
bool atMode = true;

void callback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");
  for (int i=0;i<length;i++) {
    Serial.print((char)payload[i]);
  }
  Serial.println();
}

PPPOSClient ppposClient;
PubSubClient client(ppposClient);

bool sendCommandWithAnswer(String cmd, String ans){
         PPPOS_write((char *)cmd.c_str());
         unsigned long _tg = millis();
         while(true){
          data = PPPOS_read();
          if (data != NULL){
            char* command = strtok(data, "\n");
            while (command != 0)
            {
              buffer = String(command);
              buffer.replace("\r", "");
              command = strtok(0, "\n");
              if (buffer != "") { Serial.println(buffer); }
              if (buffer == ans) {buffer = ""; return true; }
              buffer = "";
            } 
          }
            if (millis() > (_tg + 5000)) { buffer = ""; return false; } 
         }
         buffer = "";
         return false;
}

bool startPPPOS(){  
      String apnSet = "AT+CGDCONT=1,\"IP\",\"" + APN + "\"\n";
      if (!sendCommandWithAnswer(apnSet, "OK")) { return false; }
      if (!sendCommandWithAnswer("AT+CGDATA=\"PPP\",1\n", "CONNECT")) { return false; }
      atMode = false;
      PPPOS_start(); 
      unsigned long _tg = millis();
      while(!PPPOS_isConnected()) {
        if (millis() > (_tg + 10000)) { PPPOS_stop();  atMode = true; return false; }
      }
      Serial.println("PPPOS Started");
      return true;
}

bool enterATModePPPOS(){
      if (PPPOS_isConnected() && !atMode){
        if (sendCommandWithAnswer("+++", "OK")) { atMode = true; return true;}
      }
      return false;
}

bool cancelATModePPPOS(){
      if (PPPOS_isConnected() && atMode){
        if (sendCommandWithAnswer("ATO\n", "CONNECT")) { atMode = false; return true;}
      }
      return false;
}

void simpleGetRequest(){
  if (!ppposClient.connected() ) {
          Serial.println("Connecting...");
           ppposClient.connect(WEB_SERVER, 80);
  }
  if (ppposClient.connected() ) {
           Serial.println("Connected");
           Serial.println(ppposClient.write(REQUEST, strlen(REQUEST)));
           while(ppposClient.available()){
            Serial.print((char)ppposClient.read());
           }
  }
}

void reconnect() {
  // Loop until we're reconnected
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Attempt to connect
    if (client.connect("arduinoClient")) {
      Serial.println("connected");
      // Once connected, publish an announcement...
      client.publish("outTopic","hello world");
      // ... and resubscribe
      client.subscribe("inTopic");
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}

void setup()
{
  Serial.begin(SERIAL_BR);
  PPPOS_init(GSM_TX, GSM_RX, GSM_BR, GSM_SERIAL, ppp_user, ppp_pass);
  client.setServer(server, 1883);
  client.setCallback(callback);

  Serial.println("1) Start GSM Communication: AT\\n");
  Serial.println("2) Start PPP Protocol: ppp\\n");
  Serial.println("3) Test GET Request: get\\n");
  Serial.println("4) Switch to AT Mode: +++\\n");
  Serial.println("5) Switch to Data Mode: ATO\\n");
  Serial.println("6) Stop PPP Protocol: stop\\n");
}

void loop()
{ 
  if (!PPPOS_isConnected() || atMode){
    data = PPPOS_read();
    if (data != NULL){
      Serial.println(data);  
    }
  }
  
  if (Serial.available()){
    char c = Serial.read();
    if (c == '\n'){
      if (buffer == "ppp") {          
        Serial.println("Starting PPPOS...");
        if (startPPPOS()) { Serial.println("Starting PPPOS... OK"); } else { Serial.println("Starting PPPOS... Failed"); }
      } else if (buffer == "stop") {
        PPPOS_stop(); 
      } else if (buffer == "+++") {
        if (enterATModePPPOS()) { Serial.println("Entering ATMode... OK"); } else { Serial.println("Entering ATMode... Failed"); }
      } else if (buffer == "ATO") {
        if (cancelATModePPPOS()) { Serial.println("Canceling ATMode... OK"); } else { Serial.println("Canceling ATMode... Failed"); }
      } else if (buffer == "get") {
        simpleGetRequest();
      } else { 
        buffer += "\n";
        PPPOS_write((char *)buffer.c_str());
        Serial.println(buffer);
      }
      buffer = "";
    } else {
      buffer += c;
    }
  }

  if (PPPOS_isConnected() && !atMode) {
    if (!client.connected()) {
      reconnect();
    }
    client.loop();
  }
}
