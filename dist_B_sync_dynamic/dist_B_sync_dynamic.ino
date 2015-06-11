/*  ######################### DEFINIZIONI TECNICHE ######################### */

  #include <DHT.h>
  #define DHTPIN 2
  #define DHTTYPE DHT22
  DHT dht(DHTPIN, DHTTYPE);
  
  #define LEDPIN 4
  #define INPIN 5
  
  // dati mqtt
  #define APssid              "Telecom-75286748"              //wifi network ssid
  #define APpsw               "routerpassw"          //wifi netwrok password
  #define MQTTid              my_label        //id of this mqtt client
  #define MQTTip              "192.168.10.1"     //ip address or hostname of the mqtt broker
  #define MQTTport            1883                    //port of the mqtt broker
  #define MQTTuser            ""               //username of this mqtt client
  #define MQTTpsw             ""             //password of this mqtt client
  #define MQTTalive           30                     //mqtt keep alive interval (seconds)
  #define MQTTretry           10                      //time to wait before reconnect if connection drops (seconds)
  #define MQTTqos             2                       //quality of service for subscriptions and publishes
  #define esp8266reset        3                      //arduino pin connected to esp8266 reset pin (analog pin suggested due to higher impedance)
  #define esp8266alive        40                      //esp8266 keep alive interval (reset board if fail) (seconds)
  #define esp8266serial       Serial2                //Serial port to use to communicate to the esp8266 (Serial, Serial1, Serial2, etc)
  #define debugSerial Serial
  boolean connected = false;

/*  ######################### DEFINIZIONI ALGORITMO ######################### */

#define my_label 'b'
//#define sensor 100

// numero in - neig.
#define in_n 1

// lista degli in - neig.
const char in_label[] = {'e'};

// contenitore dei messaggi in arrivo
float data[in_n];
char  sync[in_n];

// stato corrente
float state;

// ultimo valore letto (r(t-h))
float input;

void setup() {
    debugSerial.begin(9600);
    debugSerial.println("-- STARTING...");

    dht.begin();
    pinMode(LEDPIN, OUTPUT);
    pinMode(INPIN, OUTPUT);    
    
    delay(3000);
    
    esp8266serial.begin(9600);                          //
    esp8266serial.setTimeout(500);                       //start serial

    for (unsigned int i = 0; i < in_n; i++) {
      sync[i] = 'F';
    }

    input = readInput();
    state = input;
    
    while(!connected)                                   //
        checkComm();                                    //wait for first connection    
}

void onConnected() {                                //on connected callback
  debugSerial.println("-- CONNECTED");
  for (unsigned int i = 0; i < in_n; i++) {
    debugSerial.print("subscribing: " );
    debugSerial.println(in_label[i]);
    mqttSubscribe(String(in_label[i]) + "_d");
    mqttSubscribe(String(in_label[i]) + "_s");    
  }
  
  //informo i vicini che sono online
  mqttPublish(String(my_label) + "_s", "B", 1);
  
  // un po' di ritardo per ricevere tutti i retains
  debugSerial.println("-- WAIT RETAINS...");
  long waitUntil = millis() + 10000;
  while (millis() < waitUntil) {
    do
      checkComm();
    while(!connected);
  }

  digitalWrite(LEDPIN, HIGH);
  
  mqttPublish(String(my_label) + "_d", String(state), 1);  
  mqttPublish(String(my_label) + "_s", "R", 1);

/*
  for (unsigned int i = 0; i < in_n; i++) {
    if (data[i] > -274)
      data[i] = -275;
  }
*/
}

void onMessage(String topic, String message) {      //new message callback
  debugSerial.print("incoming from '");
  int last_slash_pos = topic.lastIndexOf("/");
  String sender = topic.substring(last_slash_pos + 1);
  debugSerial.print(sender);
  debugSerial.print("': ");
  debugSerial.println(message);
  char sender_c[4];
  sender.toCharArray(sender_c, 4);
  for (unsigned int i = 0; i < in_n; i++) {
    if (sender_c[0] == in_label[i]) {
      if (sender_c[2] == 'd') {
        data[i] = message.toFloat();
      } else if (sender_c[2] == 's') {
        char sync_msg[2];
        message.toCharArray(sync_msg, 2);
        sync[i] = sync_msg[0];
      }
    }
  }
}

void onDisconnected() {                             //on disconnected callback
  debugSerial.println("-- DISCONNECTED");
  digitalWrite(LEDPIN, LOW);
  digitalWrite(INPIN, LOW);  
}

float readInput() {
  #ifdef sensor
    return sensor;
  #else
    return dht.readTemperature();
  #endif
}

unsigned int count = 0;
void loop() { 
    
  debugSerial.println("all data received, start new step");
    
  //aggiorno state
  unsigned int in_n_online = 0;
  for (unsigned int i = 0; i < in_n; i++) {
    if (sync[i] == 'R')
      in_n_online++;
  }
  digitalWrite(INPIN, in_n_online > 0 ? HIGH : LOW);
  
  float weight = 1.0 / (in_n_online + 1);
  
  // newState: x(t+h)
  float newState = state;
  for (unsigned int i = 0; i < in_n; i++) {
    if (sync[i] == 'R')
      newState += weight * (data[i] - state);
  }
  // delta r: newInput - input (=dato letto)
  float newInput = readInput();
  state = newState + newInput - input;
  input = newInput;
  count++;
  
  debugSerial.println("end step");
      
  mqttPublish(String(my_label) + "_d", String(state), 1);
  mqttPublish(String(my_label) + "_s", "R", 1);  
  
  debugSerial.println("sending: " + String(state));
  
  for (unsigned int i = 0; i < in_n; i++) {
    if (sync[i] == 'R')
      sync[i] = 'B';
  }

  checkReady:
  
  do                                                  //
    checkComm();                                      //
  while(!connected);                                  //check for incoming messages
  
  for (unsigned int i = 0; i < in_n; i++) {
    if (sync[i] == 'B')
      goto checkReady;
  }
  
}


























//  ####    DO NOT TOUCH THIS CODE!    ####

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
        if (connected) {
          connected = false;          
          onDisconnected(); 
        }       
    }
    if (esp8266serial.find("[(")) {
        esp8266serial.readBytes(cb, 1);
        if (cb[0] == 'r') {
            //ready
            debugSerial.println("-- ESP RESET");
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
    delay(500);
}
void mqttSubscribe(String topic) {
    if (!connected)
        return;
    success = false;
    esp8266serial.println("mqttSubscribe(\"" + String(topic) + "\", " + MQTTqos + ")");
    esp8266serial.flush();
    waitForSuccess();
    delay(500);    
}
//  ####    END OF UNTOUCHABLE CODE    ####

