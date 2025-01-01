#include <WiFiUDP.h>

const char* TELLO_IP = "192.168.10.1";
const int PORT = 8889;

WiFiUDP Udp;
char packetBuffer[255];
String message = "";

String listenMessage();
void sendMessage(char* ReplyBuffer);

void setup() {
    Serial.begin(115200);

    Serial.println("start");
    Udp.begin(PORT);
    
    sendMessage("command");
    message = listenMessage();
    Serial.println(":command");
    Serial.println(message);
    delay(1000);

    //離陸
    sendMessage("takeoff");
    Serial.println(":takeoff");
    message = listenMessage();
    Serial.println(message);
    delay(5000);

    //着陸
    sendMessage("land");
    Serial.println(":land");
    message = listenMessage();
    Serial.println(message);
}

void loop() {}

//Telloからのレスポンスを確認する関数
String listenMessage() {
    int packetSize = Udp.parsePacket();
    if (packetSize) {
        Serial.print("Received packet of size ");
        Serial.println(packetSize);
        Serial.print("From ");
        IPAddress remoteIp = Udp.remoteIP();
        Serial.print(remoteIp);
        Serial.print(", port ");
        Serial.println(Udp.remotePort());

        // read the packet into packetBufffer
        int len = Udp.read(packetBuffer, 255);
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
    Udp.beginPacket(TELLO_IP, PORT);
    Udp.printf(ReplyBuffer);
    Udp.endPacket();
}
