/*
 * Dr.HD 500 Combo Satellite Finder -> ESP32 Wireless Bridge
 * 
 * Hardware: Standard ESP32 (WROOM-32 / DevKitC)
 * 
 * Function:
 *   1. Connects via Bluetooth Classic SPP as a Master to Dr.HD 500 (MAC: 01:AC:78:00:04:55, PIN: 0000).
 *   2. Creates a standalone Wi-Fi Access Point: "DrHD-500-Bridge" (Password: "12345678", IP: 192.168.4.1).
 *   3. Starts a TCP Socket Server on Port 8080.
 *   4. Seamlessly relays telemetry frames between the physical sat finder and the DrHD500 iOS App.
 */

#include <Arduino.h>
#include <WiFi.h>
#include "BluetoothSerial.h"

// Check if Bluetooth Classic is enabled in ESP32 configuration
#if !defined(CONFIG_BT_ENABLED) || !defined(CONFIG_BLUEDROID_ENABLED)
#error "Bluetooth is not enabled! Please use an ESP32 board that supports Bluetooth Classic."
#endif

// Dr.HD 500 Satellite Finder Hardware Settings
const char* DRHD_PIN = "0000";
uint8_t drhdAddress[6] = {0x01, 0xAC, 0x78, 0x00, 0x04, 0x55};

// Wi-Fi Access Point Settings
const char* AP_SSID = "DrHD-500-Bridge";
const char* AP_PASS = "12345678";
const uint16_t TCP_PORT = 8080;

// Onboard Status LED (GPIO 2 on most ESP32 DevKits)
const int LED_PIN = 2;

BluetoothSerial SerialBT;
WiFiServer tcpServer(TCP_PORT);
WiFiClient tcpClient;

bool btConnected = false;
unsigned long lastConnectAttempt = 0;
const unsigned long RECONNECT_INTERVAL = 3000; // Retry every 3 seconds

void connectToDrHD() {
    Serial.println("[BT] Attempting connection to Dr.HD 500 (01:AC:78:00:04:55)...");
    digitalWrite(LED_PIN, LOW);
    
    SerialBT.setPin(DRHD_PIN);
    
    // 1. First attempt: Insecure SPP (matches Android APK's createInsecureRfcommSocketToServiceRecord)
    btConnected = SerialBT.connect(drhdAddress, 0, ESP_SPP_SEC_NONE, ESP_SPP_ROLE_MASTER);
    
    // 2. Fallback: Authenticated SPP with PIN 0000
    if (!btConnected) {
        Serial.println("[BT] Insecure connect failed, retrying with authenticated PIN 0000...");
        btConnected = SerialBT.connect(drhdAddress);
    }
    
    if (btConnected) {
        Serial.println("=================================================");
        Serial.println("✅ Successfully connected to Dr.HD 500 Sat Finder!");
        Serial.println("=================================================");
        digitalWrite(LED_PIN, HIGH);
    } else {
        Serial.println("❌ Connection failed. Will retry in 3 seconds.");
    }
}

void setup() {
    pinMode(LED_PIN, OUTPUT);
    digitalWrite(LED_PIN, LOW);
    
    Serial.begin(115200);
    delay(500);
    
    Serial.println("\n\n==========================================");
    Serial.println("  Dr.HD 500 Combo -> ESP32 Wireless Bridge");
    Serial.println("==========================================");

    // 1. Initialize Wi-Fi Access Point
    Serial.print("[WiFi] Creating Access Point: ");
    Serial.println(AP_SSID);
    WiFi.mode(WIFI_AP);
    WiFi.softAP(AP_SSID, AP_PASS);
    
    IPAddress IP = WiFi.softAPIP();
    Serial.print("[WiFi] AP IP Address: ");
    Serial.println(IP);
    
    // 2. Start TCP Server
    tcpServer.begin();
    tcpServer.setNoDelay(true);
    Serial.printf("[TCP] Server listening on %s:%d\n", IP.toString().c_str(), TCP_PORT);

    // 3. Initialize Bluetooth Classic SPP in Master Mode
    Serial.println("[BT] Starting Bluetooth Classic SPP Master...");
    SerialBT.setPin(DRHD_PIN);
    SerialBT.begin("DrHD-ESP32-Bridge", true); // true = Master mode
    
    // Initial connection attempt
    connectToDrHD();
    lastConnectAttempt = millis();
}

void loop() {
    unsigned long now = millis();

    // Reconnection logic if disconnected from Dr.HD sat finder
    if (!btConnected && (now - lastConnectAttempt > RECONNECT_INTERVAL)) {
        lastConnectAttempt = now;
        connectToDrHD();
    }

    // Check for incoming TCP connection from the iOS App
    if (!tcpClient || !tcpClient.connected()) {
        WiFiClient newClient = tcpServer.available();
        if (newClient) {
            tcpClient = newClient;
            tcpClient.setNoDelay(true);
            Serial.print("[TCP] iOS App connected from: ");
            Serial.println(tcpClient.remoteIP());
        }
    }

    // Bidirectional Relay:
    // A. Read from iOS App -> Forward to Dr.HD 500 via Bluetooth Classic
    if (tcpClient && tcpClient.connected() && tcpClient.available()) {
        int avail = tcpClient.available();
        uint8_t buf[256];
        int bytesRead = tcpClient.read(buf, min(avail, (int)sizeof(buf)));
        
        if (btConnected && bytesRead > 0) {
            // Write in 2-byte increments to prevent Dr.HD UART buffer overrun
            for (int i = 0; i < bytesRead; i += 2) {
                int chunkSize = min(2, bytesRead - i);
                SerialBT.write(&buf[i], chunkSize);
                delayMicroseconds(250);
            }
        }
    }

    // B. Read from Dr.HD 500 -> Forward to iOS App via TCP
    if (btConnected && SerialBT.available()) {
        int avail = SerialBT.available();
        uint8_t buf[256];
        int bytesRead = SerialBT.readBytes(buf, min(avail, (int)sizeof(buf)));
        
        if (tcpClient && tcpClient.connected() && bytesRead > 0) {
            tcpClient.write(buf, bytesRead);
        }
    }

    // Detect Bluetooth disconnect
    if (btConnected && !SerialBT.connected()) {
        Serial.println("[BT] Disconnected from Dr.HD 500.");
        btConnected = false;
        digitalWrite(LED_PIN, LOW);
    }
}
