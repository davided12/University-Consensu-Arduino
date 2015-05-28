#include <DHT.h>

#define debugSerial Serial

#define DHTPIN 2
#define DHTTYPE DHT22
DHT dht(DHTPIN, DHTTYPE);

// numero in - neig.
const unsigned int in_n = 2;

char my_label = 'a';
//char my_label = 'b';

// lista degli in - neig.
char in_label[] = {'b','e'};
//char in_label[] = {'a','e'};


// contenitore dei messaggi in arrivo
float data[in_n];

// stato corrente
float state = 0;

// ultimo valore letto
float value = 0;

// controllo se è la prima iterazione
boolean firstSend = true;

// dati mqtt
#define APssid              "SemeraroWIFI"              //wifi network ssid
#define APpsw               "sempassword"          //wifi netwrok password
#define MQTTid              my_label        //id of this mqtt client
#define MQTTip              "suxsem.dlinkddns.com"     //ip address or hostname of the mqtt broker
#define MQTTport            1883                    //port of the mqtt broker
#define MQTTuser            "test"               //username of this mqtt client
#define MQTTpsw             "test"             //password of this mqtt client
#define MQTTalive           30                     //mqtt keep alive interval (seconds)
#define MQTTretry           10                      //time to wait before reconnect if connection drops (seconds)
#define MQTTqos             2                       //quality of service for subscriptions and publishes
#define esp8266reset        3                      //arduino pin connected to esp8266 reset pin (analog pin suggested due to higher impedance)
#define esp8266alive        40                      //esp8266 keep alive interval (reset board if fail) (seconds)
#define esp8266serial       Serial3                //Serial port to use to communicate to the esp8266 (Serial, Serial1, Serial2, etc)
boolean connected = false;

//  ####    DO NOT TOUCH THIS CODE!    ####

void onConnected();
void onDisconnected();
void onMessage();

#define buffer_l 50
#define replyTimeout 5000
char in_buffer[buffer_l + 1];
char cb[1];
boolean success;
boolean messageQueued = false;
unsigned long lastAliveCheck = 0;
void checkComm() {
    if (millis() - lastAliveCheck > esp8266alive * 2000UL || lastAliveCheck == 0) {
        pinMode(esp8266reset, OUTPUT);
        delay(50);
        pinMode(esp8266reset, INPUT);
        lastAliveCheck = millis();
        connected = false;        
    }
    if (esp8266serial.find("[(")) {
        esp8266serial.readBytes(cb, 1);
        if (cb[0] == 'r') {
            //ready
            if (connected) {
                connected = false;
                onDisconnected();
            }
            lastAliveCheck = millis();            
            esp8266serial.println("startAlive(" + String(esp8266alive) + ")");
            esp8266serial.flush();
            esp8266serial.println("connectAP(\"" + String(APssid) + "\", \"" + String(APpsw) + "\")");
            esp8266serial.flush();
        } else if (cb[0] == 'a') {
            lastAliveCheck = millis();
            checkComm();
        } else if (cb[0] == 'w') {
            //wifi connected
            esp8266serial.println("mqttInit(\"" + String(MQTTid) + "\", \"" + String(MQTTip) + "\", " + MQTTport + ", \"" + String(MQTTuser)
                            + "\", \"" + String(MQTTpsw) + "\", " + MQTTalive + ", " + MQTTretry + ")");
            esp8266serial.flush();
        } else if (cb[0] == 'c') {
            //mqtt connected
            connected = true;
            onConnected();
        } else if (cb[0] == 'd') {
            //disconnected
            connected = false;
            onDisconnected();
        } else if (cb[0] == 'm') {
            //new message
            if (messageQueued)
                return;
            if (!success)
                messageQueued = true;
            memset(in_buffer, 0, sizeof(in_buffer));
            esp8266serial.readBytesUntil('|', in_buffer, buffer_l);
            String topic = String(in_buffer);
            memset(in_buffer, 0, sizeof(in_buffer));
            esp8266serial.readBytesUntil('|', in_buffer, buffer_l);
            String message = String(in_buffer);
            waitForSuccess();
            onMessage(topic, message);
            messageQueued = false;
        } else if (cb[0] == 'p' || cb[0] == 's') {
            success = true;
        }
    }
}
void waitForSuccess() {
    unsigned long started = millis();
    while (!success) {
        if (!connected || millis() - started > replyTimeout) {
            success = true;
            break;
        }
        checkComm();
    }
}
void mqttPublish(String topic, String message, byte retain) {
    if (!connected)
        return;
    success = false;
    esp8266serial.println("mqttPublish(\"" + topic + "\", \"" + message + "\",  " + MQTTqos + ", " + retain + ")");                
    esp8266serial.flush();
    waitForSuccess();
}
void mqttSubscribe(String topic) {
    if (!connected)
        return;
    success = false;
    esp8266serial.println("mqttSubscribe(\"" + String(topic) + "\", " + MQTTqos + ")");
    esp8266serial.flush();
    waitForSuccess();
}
//  ####    END OF UNTOUCHABLE CODE    ####

void setup() {
    debugSerial.begin(9600);
    debugSerial.println("buongiorno");
    
    esp8266serial.begin(9600);                          //
    esp8266serial.setTimeout(500);                       //start serial

    for (unsigned int i = 0; i < in_n; i++) {
      data[i] = -274;
    }
    
    while(!connected)                                   //
        checkComm();                                    //wait for first connection
    
}

void onConnected() {                                //on connected callback
  debugSerial.println("CONNESSO");
  for (unsigned int i = 0; i < in_n; i++) {
    debugSerial.print("sottoscrivo " );
    debugSerial.println(in_label[i]);
    mqttSubscribe("test/distributed/" + String(in_label[i]));
  }

  // un po' di ritardo per ricevere tutti i retain
  debugSerial.println("1");
  long waitUntil = millis() + 10000;
  while (millis() < waitUntil) {
    do
      checkComm();
    while(!connected);
  }

//    value = dht.readTemperature();    
  value = 20;
//  value = 40;
  state = value;
    
}

void onMessage(String topic, String message) {      //new message callback
  debugSerial.println("arrivato");
  int last_slash_pos = topic.lastIndexOf("/");
  String sender = topic.substring(last_slash_pos + 1);
  char sender_c[2];
  sender.toCharArray(sender_c, 2);
  for (unsigned int i = 0; i < in_n; i++) {
    if (in_label[i] == sender_c[0]) {
      debugSerial.println(message);
      data[i] = message.toFloat();      
    }
  }
}

void onDisconnected() {                             //on disconnected callback
  debugSerial.println("DISCONNESSO");
}


void loop() { 
  
  do                                                  //
    checkComm();                                      //
  while(!connected);                                  //check for incoming messages
  
  unsigned int in_n_online = 0;
  for (unsigned int i = 0; i < in_n; i++) {
    if (data[i] == -275)
      return;
    if (data[i] > -274)
      in_n_online++;
  }
  
  //arrivo qui solo se tutti quelli online hanno inviato
  
  //aggiorno state
  float weight = 1.0 / (in_n_online + 1);
  float old_state = state;
  state = 0;
  for (unsigned int i = 0; i < in_n; i++) {
    if (data[i] > -274)
      state = state + weight * data[i];
  } 
  state = state + weight * old_state;
    
  if (firstSend) {
    mqttPublish("test/distributed/" + String(my_label), String(value), 1);
    firstSend = false;
  }
  
  mqttPublish("test/distributed/" + String(my_label), String(state), 1);
  debugSerial.println("mando: " + String(state));
  
  for (unsigned int i = 0; i < in_n; i++) {
    if (data[i] > -274)
      data[i] = -275;
  }  
  
}
