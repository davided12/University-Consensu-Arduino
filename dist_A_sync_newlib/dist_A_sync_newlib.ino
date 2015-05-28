#include <DHT.h>
#include <espduino.h>
#include <mqtt.h>

// dati mqtt
#define APssid              "Telecom-75286748"              //wifi network ssid
#define APpsw               "routerpassw"          //wifi netwrok password
#define MQTTid              "a"        //id of this mqtt client
#define MQTTip              "suxsem.dlinkddns.com"     //ip address or hostname of the mqtt broker
#define MQTTport            1883                    //port of the mqtt broker
#define MQTTuser            "test"               //username of this mqtt client
#define MQTTpsw             "test"             //password of this mqtt client
#define MQTTalive           30                     //mqtt keep alive interval (seconds)
//#define MQTTretry           10                      //time to wait before reconnect if connection drops (seconds)
//#define MQTTqos             2                       //quality of service for subscriptions and publishes
#define esp8266reset        3                      //arduino pin connected to esp8266 reset pin (analog pin suggested due to higher impedance)
//#define esp8266alive        40                      //esp8266 keep alive interval (reset board if fail) (seconds)
#define esp8266serial       Serial3                //Serial port to use to communicate to the esp8266 (Serial, Serial1, Serial2, etc)
#define debugPort Serial

ESP esp(&esp8266serial, &debugPort, 3);
MQTT mqtt(&esp);

boolean wifiConnected = false;

#define DHTPIN 2
#define DHTTYPE DHT22
DHT dht(DHTPIN, DHTTYPE);

// numero in - neig.
const unsigned int in_n = 2;

char my_label = MQTTid[0];

// lista degli in - neig.
//char in_label[] = {'b','e'};
char in_label[] = {'a','e'};

// contenitore dei messaggi in arrivo
float data[in_n];

// stato corrente
float state = 0;




void wifiCb(void* response)
{
  uint32_t status;
  RESPONSE res(response);

  if(res.getArgc() == 1) {
    res.popArgs((uint8_t*)&status, 4);
    if(status == STATION_GOT_IP) {
      debugPort.println("WIFI CONNECTED");
      mqtt.connect(MQTTip, MQTTport, false);
      wifiConnected = true;
    } else {
      wifiConnected = false;
      mqtt.disconnect();
    }

  }

}

void mqttConnected(void* response)
{

  debugPort.println("Connected");

  for (unsigned int i = 0; i < in_n; i++) {
    debugPort.print("sottoscrivo " );
    debugPort.println(in_label[i]);
    mqtt.subscribe("test/distributed/" + in_label[i], 2);
  }  

  for (long time = millis(); time + 10000 < millis();) {
    esp.process();
  }
  
//    state = dht.readTemperature();    
//      state = 20;
  state = 40;
  char buf[20];
  sprintf(buf, "%f", state);  
  mqtt.publish("test/distributed/" + my_label, buf, 2, 1); 

}
void mqttDisconnected(void* response)
{

}
void mqttData(void* response)
{
  RESPONSE res(response);

  debugPort.print("Received: topic=");
  String topic = res.popString();
  debugPort.println(topic);

  debugPort.print("message=");
  String message = res.popString();
  debugPort.println(message);

  int last_slash_pos = topic.lastIndexOf("/");
  String sender = topic.substring(last_slash_pos + 1);
  char sender_c[2];
  sender.toCharArray(sender_c, 2);
  for (unsigned int i = 0; i < in_n; i++) {
    if (in_label[i] == sender_c[0]) {
      data[i] = message.toFloat();      
    }
  }

}
void mqttPublished(void* response)
{

}

void setup() {  
  
    debugPort.begin(19200);
    debugPort.println("buongiorno");

    for (unsigned int i = 0; i < in_n; i++) {
      data[i] = -274;
    }
        
    esp8266serial.begin(19200);                          //

    debugPort.println("1");

    esp.enable();
    delay(4000);
    
    debugPort.println("2");        
    esp.reset();
    delay(500);    
    
    debugPort.println("3");    
    while(!esp.ready());
    
    debugPort.println("ARDUINO: setup mqtt client");
    if(!mqtt.begin(MQTTid, MQTTuser, MQTTpsw, MQTTalive, 1)) {
      debugPort.println("ARDUINO: fail to setup mqtt");
      while(1);
    }
    
    debugPort.println("ARDUINO: setup mqtt lwt");
    mqtt.lwt("test/distributed/" + my_label, "-274", 2, 1);
    
  mqtt.connectedCb.attach(&mqttConnected);
  mqtt.disconnectedCb.attach(&mqttDisconnected);
  mqtt.publishedCb.attach(&mqttPublished);
  mqtt.dataCb.attach(&mqttData);

  //setup wifi
  debugPort.println("ARDUINO: setup wifi");
  esp.wifiCb.attach(&wifiCb);

  esp.wifiConnect(APssid, APpsw);

  debugPort.println("ARDUINO: system started");

}


void loop() { 

  esp.process();
  if(wifiConnected) {
    
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
      
    char buf[20];
    sprintf(buf, "%f", state); 
    mqtt.publish("test/distributed/" + my_label, buf, 2, 1);
    
    for (unsigned int i = 0; i < in_n; i++) {
      if (data[i] > -274)
        data[i] = -275;
    }  
  }

}
