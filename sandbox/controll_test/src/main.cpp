// [小型ドローンTelloの編隊飛行をESP32でやってみた : DJI Tello drone formation flight ython - Qiita ](https://qiita.com/bishi/items/30ba53eedbb868cb6ddcE4%BD%BF%E3%81%84%E6%96%B9)

// --
// Tello controller for ESP32
// written by bishi 2018.04.06
// --
#include <WiFi.h>
#include <WiFiUdp.h>
#include <FS.h>
#include <SPIFFS.h>

#include <Ps3Controller.h>

int player = 0;
int battery_ps3 = 0;

// -- literal
// pin
const int buttonPin = 0;
const int ledPin = 2;

// WiFi - AP mode (for self)
String ssidAp = "TELLO-Controller";
String passwordAp = "tellocon";
const IPAddress ipAp(192, 168, 4, 1);
const IPAddress gatewayAp = ipAp;
const IPAddress subnetAp(255, 255, 255, 0);
const int udpPortAp = 1060;
const int receiveBufferLen = 64;

// WiFi - Client mode (for Tello)
String ssidTello = "TELLO-CCDE6B";  // overridden later
String passwordTello = "";  // overridden later
String ipTello = "192.168.10.1";
const int udpPortTello = 8889;

// function prototype
void writeSsid(String arg);
void writePass(String arg);
void writeDroneCmd(String arg);
void clearDroneCmd(String arg);

// command table
typedef void (pfunc)(String);
typedef struct
{
  char *cmd;    // command code
  pfunc *func;  // function pointer
} settingCommand_t;

settingCommand_t settingCommandTable[] =
{
  // command code   function pointer
  {"ssid",          writeSsid },      // write ssid to SSID.txt
  {"pass",          writePass },      // write password to PASS.txt
  {"cmd",           writeDroneCmd },  // write command to DRONECMD.txt
  {"clear",         clearDroneCmd },  // clear DRONECMD.txt
  {NULL,            NULL }            // terminator
};

// -- variable
// pin
int buttonState = HIGH;

// WiFi - AP mode (for self)
WiFiUDP udp;
char receiveBuffer[receiveBufferLen];

// WiFi - Client mode (for Tello)
bool connectedTello = false;

// -- function prototype
void setup();
void loop();
// drone
void controlTelloProcess(void);
void connectToWiFi(const char *ssid, const char *password);
void wifiEvent(WiFiEvent_t event);
String listenMessage();
void sendMessage(char* ReplyBuffer);
// controller
void onConnect();
// timer interrupt
hw_timer_t * timer = NULL;
void IRAM_ATTR onTimer();

// -- setup function
void setup() {
    // serial
    Serial.begin(115200);

    // pin
    pinMode(buttonPin, INPUT_PULLUP);
    pinMode(ledPin, OUTPUT);
    delay(100); // required delay
    digitalWrite(ledPin, HIGH);

    // -- client mode
    // WiFi connect to Tello
    Serial.print("SSID Tello : ");
    Serial.println(ssidTello);
    Serial.print("Password Tello : ");
    Serial.println(passwordTello);
    connectToWiFi(ssidTello.c_str(), passwordTello.c_str());
    digitalWrite(ledPin, LOW);

    // コントローラ接続
    // ESP32のMACアドレスを表示
    uint8_t btmac[6];
    esp_read_mac(btmac, ESP_MAC_BT);
    Serial.printf("[Bluetooth] Mac Address = %02X:%02X:%02X:%02X:%02X:%02X\r\n", btmac[0], btmac[1], btmac[2], btmac[3], btmac[4], btmac[5]);
    // 接続
    // Ps3.attach(notify);
    Ps3.attachOnConnect(onConnect);
    Ps3.begin("88:13:BF:0D:6D:A6"); // todo ここにESP32のMACアドレスを入れる

    // タイマー割り込み
    timer = timerBegin(0, 80, true);
    timerAttachInterrupt(timer, &onTimer, true);
    timerAlarmWrite(timer, 10000, true); // 10ms
    timerAlarmEnable(timer);

    Serial.println("Ready.");
}

// -- main loop function
void loop() {
    // -- client mode
    if (connectedTello == true)
    {
    digitalWrite(ledPin, HIGH);
    buttonState = digitalRead(buttonPin);
    if (buttonState == LOW) {
        controlTelloProcess();
    }
    } else {
    digitalWrite(ledPin, LOW);
    }
}

// -- client(controll Tello) mode process function
void controlTelloProcess(void)
{
    Serial.println("start");
    udp.begin(udpPortTello);

    String message = "";
    
    sendMessage("command");
    message = listenMessage();
    Serial.println(":command");
    Serial.println(message);
    delay(1000);

    // 離陸
    sendMessage("takeoff");
    delay(1000);
    sendMessage("takeoff");
    Serial.println(":takeoff");
    message = listenMessage();
    Serial.println(message);
    sendMessage("takeoff");
    delay(3000);

    // 着陸
    Serial.println(":land...");
    for (uint8_t i = 0; i < 10; i++) {
        sendMessage("land");
        delay(700);
    }
    Serial.println(":...land");
    message = listenMessage();
    Serial.println(message);

    Serial.println("finish!");
}

// start connect to WiFi AP(Tello)
void connectToWiFi(const char *ssid, const char *password){
    Serial.print("Connecting : ");
    Serial.println(ssid);

    // delete old config
    WiFi.disconnect(true);

    //register event handler
    WiFi.onEvent(wifiEvent);
    
    WiFi.begin(ssid, password);
    Serial.println("Waiting for WiFi connection...");
}

//wifi event handler
void wifiEvent(WiFiEvent_t event){

    switch(event) {
        case SYSTEM_EVENT_STA_GOT_IP:
            // connected 
            Serial.println("WiFi connected!");
            Serial.print("IP address : ");
            Serial.println(WiFi.localIP());

            //initialize udp
            udp.begin(WiFi.localIP(), udpPortTello);
            connectedTello = true;
            break;

        case SYSTEM_EVENT_STA_DISCONNECTED:
            // disconnected
            Serial.println("WiFi lost connection");
            connectedTello = false;
            break;
    }
}

//Telloからのレスポンスを確認する関数
String listenMessage() {
    char packetBuffer[255];
    int packetSize = udp.parsePacket();
    if (packetSize) {
        Serial.print("Received packet of size ");
        Serial.println(packetSize);
        Serial.print("From ");
        IPAddress remoteIp = udp.remoteIP();
        Serial.print(remoteIp);
        Serial.print(", port ");
        Serial.println(udp.remotePort());

        // read the packet into packetBufffer
        int len = udp.read(packetBuffer, 255);
        if (len > 0) {
        packetBuffer[len] = 0;
        }
        Serial.println("Contents:");
        Serial.println(packetBuffer);
    }
    // this only works as tello's API doesn't return responses greater than 255 char
    return (char*) packetBuffer;
}

//UDPでTelloに命令を送る関数
void sendMessage(char* ReplyBuffer) {
    udp.beginPacket(ipTello.c_str(), udpPortTello);
    udp.printf(ReplyBuffer);
    udp.endPacket();
}

// -- controller function
void onConnect(){
    Serial.println("Connected.");
}

// -- timer interrupt function
uint32_t sub_counter = 0;
void IRAM_ATTR onTimer(){
    // Serial.println(battery_ps3);
    if (battery_ps3 < 20) {
        Ps3.setRumble(1.0, 1000);
    }

    sub_counter++;
    if (sub_counter >= 100) { // 1000ms
        battery_ps3 = Ps3.data.status.battery;
        switch (battery_ps3) {
        case ps3_status_battery_full:
            Ps3.setPlayer(10);
            break;
        case ps3_status_battery_high:
            Ps3.setPlayer(9);
            break;
        case ps3_status_battery_low:
            Ps3.setPlayer(7);
            break;
        case ps3_status_battery_dying:
            Ps3.setPlayer(4);
            break;
        case ps3_status_battery_shutdown:
            Ps3.setPlayer(0);
            break;
        }
        sub_counter = 0;
    }
}

