#include <Wire.h>
#include <Adafruit_SSD1306.h>
#include <PubSubClient.h>
#include <WiFiClient.h>
#include <Adafruit_GFX.h>
#include <SPI.h>
#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <ESP8266WebServer.h>
#include <ESP8266HTTPUpdateServer.h>  // Library untuk HTTP update
#include <WiFiClientSecure.h>

//GANTIWIFI
ESP8266WebServer server(80);
ESP8266HTTPUpdateServer httpUpdater;  // Objek HTTP Update
WiFiClientSecure espClient;  
PubSubClient client(espClient);

//webconfig
bool webconfigmode = false;
bool isButtonPressed = false;
unsigned long flashButtonPressTime = 0;  // Waktu saat tombol flash ditekan
const unsigned long flashButtonHoldTime = 1000;  // Waktu dalam milidetik untuk deteksi tombol 1 detik
int buttonState = 0;  // Menyimpan status tombol
bool wifiReconnectEnabled = true; // Status apakah Wi-Fi reconnect diaktifkan
unsigned long buttonPressStart = 0;
unsigned long longPressDuration = 1000; // Durasi tekan lama (ms)
bool longPressDetected = false;
float levelAir = 0;

// Ukuran layar OLED
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64

// Pin I2C OLED
#define SDA_PIN D1
#define SCL_PIN D2

// Alamat I2C OLED
#define OLED_RESET    -1
#define OLED_I2C_ADDR 0x3C

// Pin tombol Flash (GPIO0)
#define FLASH_BUTTON 0

// Pin ON OFF
#define RELAY_PIN D4       // Pin relay
#define BUTTON_PIN 0       // Tombol GPIO0 (D3)

// Inisialisasi OLED
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
// Inisialisasi NTP Client
WiFiUDP udp;
NTPClient timeClient(udp, "pool.ntp.org", 7 * 3600, 60000); // GMT +7, update setiap 60 detik

unsigned long lastMillis = 0;  // Waktu milidetik terakhir untuk menghitung waktu maju
unsigned long localTime = 0; //waktu untuk lokal
bool timeReceived = false; // Flag untuk menentukan apakah waktu dari NTP sudah didapat
//INTERVAL UPDATE KE SERVER
unsigned long lastSendTime = 0; // Variabel untuk menyimpan waktu terakhir pengiriman
const unsigned long sendInterval =10; // Interval pengiriman dalam milidetik (20 detik)

// Daftar SSID dan password Wi-Fi untuk koneksi otomatis
String ssid = "TRY";
String password = "pkid_2017";

// Pengaturan MQTT tanpa TLS
const char* mqtt_server = "1b1abc472a014a47a1cebe302164235c.s1.eu.hivemq.cloud"; //sesuaikan dengan mqtt anda
const int mqtt_port = 8883;
const char* mqtt_user = "rezaap";
const char* mqtt_password = "Reza123456";
const char* level_air = "Sts";  // Topik Event Ctrl
const char *level_level = "Lvl";


//RELAY
bool relayStatus = false;      // Status relay (ON/OFF)
bool remoteStatus = false;   //dari esp ke 2
bool sentStatus = false; //yang dikirim ke mqtt

void stopWifiReconnect() {
  wifiReconnectEnabled = false; // Matikan proses reconnect Wi-Fi
  Serial.println("Koneksi ulang Wi-Fi dihentikan oleh tombol.");
}

void tampilan() {

  display.clearDisplay();  // Clear the display
  unsigned long seconds, minutes, hours;
  if (WiFi.status() == WL_CONNECTED && timeReceived) {
    // Check if time is received from NTP
    seconds = timeClient.getEpochTime();
    minutes = seconds / 60;
    hours = minutes / 60;
    seconds %= 60;
    minutes %= 60;
    hours %= 24;
  } else {
    unsigned long deltaTime = millis() - lastMillis;
    localTime += deltaTime; // Update local time based on millis()
    seconds = localTime / 1000;
    minutes = seconds / 60;
    hours = minutes / 60;
    seconds %= 60;
    minutes %= 60;
    hours %= 24;
  }

  // Get Wi-Fi signal strength
  int signalStrength = -100;  // Default value if Wi-Fi is not connected
  if (WiFi.status() == WL_CONNECTED) {
    signalStrength = WiFi.RSSI();  // Get the signal strength if connected
  } else {
    Serial.println("Not connected to Wi-Fi");
  }

  // Calculate how many bars to display based on signal strength
  int bars = 0;
  if (signalStrength > -70) {
    bars = 4;  // Very strong signal
  } else if (signalStrength > -80) {
    bars = 3;  // Good signal
  } else if (signalStrength > -90) {
    bars = 2;  // Weak signal
  } else if (signalStrength > -100) {
    bars = 1;  // Very weak signal
  } else {
    bars = 0;  // No signal
  }

  // Display Wi-Fi bars in the top left corner
  display.setTextSize(2);  // Larger text size for the symbol
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);

  for (int i = 0; i < 4; i++) {
    // Draw the bar based on the number of bars
    int xPos = 0 + (i * 8);  // Horizontal position for each bar (wider spacing)
    
    if (i < bars) {
      display.fillRect(xPos, 0, 5, 10, SSD1306_WHITE);  // Solid bar
    } else {
      display.drawRect(xPos, 0, 5, 10, SSD1306_WHITE);  // Empty bar
    }
  }

  // Display time in the top right corner
  display.setCursor(93, 0);
  display.setTextSize(1);
  if (hours < 10) {
    display.print("0"); // Display 0 before hour if it's less than 10
  }
  display.print(hours);
  display.print(":");
  if (minutes < 10) {
    display.print("0"); // Display 0 before minutes if it's less than 10
  }
  display.print(minutes);

  display.setTextSize(4); // Larger text for relay status
  display.setCursor(20, 30);
  display.print(relayStatus ? "ON" : "OFF");  // Display "ON" or "OFF" depending on the relay status
  digitalWrite(RELAY_PIN, relayStatus ? LOW : HIGH); // Aktifkan/Mematikan relay

  display.display();
}

void kirimRelayStatus() {
  String relayMessage = relayStatus ? "1" : "0";  // Mengirim "1" jika relay ON, "0" jika relay OFF
  if (client.connected()) {
    client.publish(level_air, relayMessage.c_str(), true);  // Publikasi status relay dengan retain = true
    Serial.print("Status Relay Dikirim: ");
    Serial.println(relayMessage);
  } else {
    Serial.println("Gagal mengirim status relay. Tidak terhubung ke MQTT broker.");
  }
}

void webConfig() {
  // Reset dan setup OLED display
  display.clearDisplay();
  display.setTextSize(2);
  display.setCursor(47, 10);
  display.setTextColor(SSD1306_WHITE);
  display.print("WEB");
  display.setCursor(33, 35);
  display.print("CONFIG");
  display.display();

  WiFi.disconnect(true); // Hentikan koneksi Wi-Fi sebelumnya
  delay(100);

  // Generate SSID berdasarkan MAC address
  String macAddress = WiFi.macAddress();
  macAddress.replace(":", "");
  String ssidName = "Config_" + macAddress.substring(6);
  if (ssidName.length() > 32) {
    ssidName = ssidName.substring(0, 32);
  }

  String apPassword = "123456789";

  // Konfigurasi Access Point
  WiFi.softAP(ssidName.c_str(), apPassword.c_str());
  IPAddress IP = WiFi.softAPIP();

  Serial.println("Access Point started.");
  Serial.print("SSID: ");
  Serial.println(ssidName);
  Serial.print("AP IP address: ");
  Serial.println(IP);
  Serial.print("Password: ");
  Serial.println(apPassword);

  // Menangani permintaan root "/"
  server.on("/", HTTP_GET, [&]() {
    String page = R"rawliteral(
      <!DOCTYPE html>
      <html>
      <body>
        <h2>Wi-Fi Configuration</h2>
        <form action="/save" method="POST">
          <label for="ssid">SSID:</label><br>
          <input type="text" id="ssid" name="ssid" value=")rawliteral" + ssid + R"rawliteral(" required><br>
          <label for="password">Password:</label><br>
          <input type="password" id="password" name="password" value=")rawliteral" + password + R"rawliteral(" required><br><br>
          <input type="submit" value="Save">
        </form>
      </body>
      </html>
    )rawliteral";

    server.send(200, "text/html", page);
  });

  // Menangani permintaan "/save"
  server.on("/save", HTTP_POST, []() {
    if (server.hasArg("ssid") && server.hasArg("password")) {
      String ssid = server.arg("ssid");
      String password = server.arg("password");

      Serial.println("Received Wi-Fi credentials:");
      Serial.print("SSID: ");
      Serial.println(ssid);
      Serial.print("Password: ");
      Serial.println(password);

      server.send(200, "text/html", "<h2>Configuration Saved</h2><p>Please restart your device to connect.</p>");
      
      // Simpan konfigurasi Wi-Fi di EEPROM (opsional)
      WiFi.begin(ssid.c_str(), password.c_str());
      delay(1000);
      ESP.restart(); // Restart ESP untuk menerapkan pengaturan
    } else {
      server.send(400, "text/html", "<h2>Error</h2><p>Missing SSID or password!</p>");
    }
  });
  webconfigmode = true;

  // Setup HTTP update server
  httpUpdater.setup(&server, "/update");

  // Mulai server
  server.begin();
  Serial.println("HTTP server started");
}

void setup_wifi() {
  delay(10);
  Serial.println();
  Serial.print("Menghubungkan ke Wi-Fi: ");
  
  // Looping untuk mencoba setiap SSID dalam daftar
  while (WiFi.status() != WL_CONNECTED) {
    WiFi.begin(ssid.c_str(), password.c_str());  // Coba koneksi Wi-Fi dengan SSID dan password yang sesuai
    
    // Tunggu hingga koneksi berhasil atau timeout
    unsigned long startAttemptTime = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - startAttemptTime < 10000) {  // Timeout 10 detik
      delay(10);
      Serial.print(".");
      // Perbarui waktu lokal
      localTime += (millis() - lastMillis);
      lastMillis = millis();
      tampilan();
       // Cek apakah tombol Flash ditekan
      if (digitalRead(FLASH_BUTTON) == LOW) {
        if (!isButtonPressed) {
          isButtonPressed = true;
          buttonPressStart = millis(); // Simpan waktu awal tombol ditekan
        } else if (millis() - buttonPressStart > longPressDuration && !longPressDetected) {
          // Tombol ditekan lama
          longPressDetected = true;
          webconfigmode = true; //config mode dioffkan
          stopWifiReconnect(); // Hentikan koneksi ulang Wi-Fi
          Serial.println(webconfigmode);
          Serial.println("Tombol Flash ditekan lama!");
          // webConfig();
          return webConfig();  // Keluar dari fungsi reconnect_wifi
        }
      } else {
        flashButtonPressTime = 0;  // Reset waktu jika tombol tidak ditekan
        if (isButtonPressed) {
          if (!longPressDetected && !levelAir >= 100) {
            // Tombol dilepas sebelum tekan lama, aksi tekan biasa
            Serial.println("Tombol Flash ditekan singkat!");
             // Toggle relay status if pressed for 0.2 seconds
            relayStatus = !relayStatus;  // Toggle the relay status
            digitalWrite(RELAY_PIN, relayStatus ? LOW : HIGH); // Aktifkan/Mematikan relay
            Serial.print("Relay Status: ");
            Serial.println(relayStatus ? "ON" : "OFF");
            delay(10);
          }
          // Reset status tombol
          isButtonPressed = false;
          longPressDetected = false;
        }
      }
    }
    
    // Jika koneksi berhasil
    if (WiFi.status() == WL_CONNECTED) {
      // shiftRegisterWrite(0b00100100);  // LED Hijau untuk status terhubung
      Serial.println("\nTerhubung ke Wi-Fi!");
      Serial.print("Alamat IP: ");
      Serial.println(WiFi.localIP());
      timeReceived = true;
      return;  // Keluar dari fungsi setup_wifi setelah koneksi berhasil
    } else {
      Serial.println("\nGagal terhubung");
    }
  }
}

// // Fungsi untuk membaca, menampilkan, dan log level air dalam satu fungsi
// void mqttCallback(char* topic, byte* payload, unsigned int length) {
//   Serial.print("Message received on topic: ");
//   Serial.println(topic);
//   Serial.print("Payload: ");
//   String payloadStr = "";
//   for (int i = 0; i < length; i++) {
//     payloadStr += (char)payload[i];
//   }
//   Serial.println(payloadStr);

//   // Cek topik yang diterima
//   if (strcmp(topic, level_air) == 0) {
//     // Jika pesan diterima dari Sts topic
//     if (payloadStr == "1") {
//       remoteStatus = true; // Relay ON
//       Serial.println("Relay turned ON");
//     } else if (payloadStr == "0") {
//       remoteStatus = false; // Relay OFF
//       Serial.println("Relay turned OFF");
//     }
//   }
//   if (remoteStatus!=relayStatus){
//     relayStatus = remoteStatus;
//     digitalWrite(RELAY_PIN, relayStatus ? LOW : HIGH); // Aktifkan/Mematikan relay
//   }
// }

void mqttCallback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Message received on topic: ");
  Serial.println(topic);
  Serial.print("Payload: ");

  // Konversi payload ke String
  String payloadStr = "";
  for (int i = 0; i < length; i++) {
    payloadStr += (char)payload[i];
  }
  Serial.println(payloadStr);

  // Cek apakah topik adalah "Lvl" (level air)
  if (strcmp(topic, "Lvl") == 0) {
    float levelValue = payloadStr.toFloat();  // Ubah payload ke float
    levelAir = levelValue;

    if (levelValue >= 100.0) {  // Jika level 100%, abaikan payload, matikan relay, dan publish ke STS
      Serial.println("Water level 100%. Ignoring any command.");
      remoteStatus = false;
      relayStatus = false; // Matikan relay secara paksa

      // Kirim status ke topik STS jika MQTT terhubung
      if (client.connected()) {
        client.publish("Sts", "0", true); // Publikasi status 1 ke topik STS
        Serial.println("Water level 100% - Published STS 1");
      }

      return;  // Langsung keluar dari fungsi untuk mencegah eksekusi lebih lanjut
    }
  }

  // Cek jika topik adalah mqtt_sts_topic
  if (strcmp(topic, level_air) == 0) {
    if (levelAir >= 100.0) {
      Serial.println("Ignoring MQTT command because water level is 100%.");
      return; // Abaikan perintah jika water level 100%
    }
    else{
      if (payloadStr == "1") {
        remoteStatus = true;  // Relay ON
        Serial.println("Relay turned ON");
      } else if (payloadStr == "0") {
        remoteStatus = false;  // Relay OFF
        Serial.println("Relay turned OFF");
      }
            // Sinkronisasi relayStatus dengan remoteStatus
      if (remoteStatus != relayStatus) {
        relayStatus = remoteStatus;
      }
    }
    
  }


}



void setup() {
  // Inisialisasi Serial Monitor
  Serial.begin(115200);
  pinMode(RELAY_PIN, OUTPUT);

  // Inisialisasi OLED
  Wire.begin(SDA_PIN, SCL_PIN);
  if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_I2C_ADDR)) {
    Serial.println(F("Failed to initialize OLED"));
    for (;;);
  }

  // Membersihkan layar OLED
  // Mulai NTPClient
  timeClient.begin();
  timeClient.update(); // Sinkronkan waktu awal
  display.clearDisplay();
  tampilan();
  display.display();
  // Atur sertifikat root CA untuk koneksi TLS
  espClient.setInsecure();  // Menggunakan setInsecure() jika tanpa sertifikat CA
  client.setServer(mqtt_server, mqtt_port);
  client.setCallback(mqttCallback);
  setup_wifi();

  Serial.println("Water Level Monitoring Started...");
  Serial.println("---------------------------------");
}
void reconnect_mqtt() {
  String macAddress = WiFi.macAddress(); // MAC Address Wi-Fi
  String clientId = "ESP32Client" + macAddress;
  while (!client.connected()) {
    if (digitalRead(FLASH_BUTTON) == LOW) {
      if (!isButtonPressed) {
        isButtonPressed = true;
        buttonPressStart = millis(); // Simpan waktu awal tombol ditekan
      } else if (millis() - buttonPressStart > longPressDuration && !longPressDetected) {
        // Tombol ditekan lama
        longPressDetected = true;
        webconfigmode = true; //config mode dioffkan
        stopWifiReconnect(); // Hentikan koneksi ulang Wi-Fi
        Serial.println(webconfigmode);
        Serial.println("Tombol Flash ditekan lama!");
        // webConfig();
        return webConfig();  // Keluar dari fungsi reconnect_wifi
      }
    } else {
      flashButtonPressTime = 0;  // Reset waktu jika tombol tidak ditekan
      if (isButtonPressed) {
        if (!longPressDetected && !levelAir >= 100) {
          // Tombol dilepas sebelum tekan lama, aksi tekan biasa
          Serial.println("Tombol Flash ditekan singkat!");
           // Toggle relay status if pressed for 0.2 seconds
          relayStatus = !relayStatus;  // Toggle the relay status
          digitalWrite(RELAY_PIN, relayStatus ? LOW : HIGH); // Aktifkan/Mematikan relay
          isButtonPressed = false;
          Serial.print("Relay Status: ");
          Serial.println(relayStatus ? "ON" : "OFF");
          delay(10);
        }
        // Reset status tombol
        isButtonPressed = false;
        longPressDetected = false;
      }
    }
    Serial.print("Menghubungkan ke MQTT broker...");
    if (client.connect("ESP8266Client", mqtt_user, mqtt_password)) {
      Serial.println("Terhubung ke MQTT broker");
      client.subscribe(level_air);
      client.subscribe(level_level);
    } else {
      Serial.print("Gagal, rc=");
      Serial.print(client.state());
      Serial.println(" Coba lagi dalam 5 detik.");
      Serial.println(WiFi.status());
      setup_wifi();
      delay(100);
    }
  }
}

void loop() {

  // Cek apakah tombol Flash ditekan
  if (digitalRead(FLASH_BUTTON) == LOW) {
    if (!isButtonPressed) {
      isButtonPressed = true;
      buttonPressStart = millis(); // Simpan waktu awal tombol ditekan
    } else if (millis() - buttonPressStart > longPressDuration && !longPressDetected) {
      // Tombol ditekan lama
      longPressDetected = true;
      webconfigmode = true; //config mode dioffkan
      Serial.println(webconfigmode);
      Serial.println("Tombol Flash ditekan lama!");
      webConfig();
    }
  } else {
    if (isButtonPressed) {
      if (levelAir >= 100.0) {  
        Serial.println("Tombol dinonaktifkan karena level air sudah 100%!");
      }else{
        // Tombol dilepas sebelum tekan lama, aksi tekan biasa
        Serial.println("Tombol Flash ditekan singkat!");
        // Toggle relay status if pressed for 0.2 seconds
          relayStatus = !relayStatus;  // Toggle the relay status
          digitalWrite(RELAY_PIN, relayStatus ? LOW : HIGH); // Aktifkan/Mematikan relay
          Serial.print("Relay Status: ");
          Serial.println(relayStatus ? "ON" : "OFF");
          delay(10);
      }
      // Reset status tombol
      isButtonPressed = false;
      longPressDetected = false;
    }
  }
  if (webconfigmode) {
    server.handleClient();
    return; // Jika dalam mode menu, hentikan loop utama
  }

   // Update waktu lokal saat Wi-Fi terputus
  if (WiFi.status() != WL_CONNECTED) {
    localTime += (millis() - lastMillis);  // Update waktu lokal berdasarkan millis()
    lastMillis = millis();
    tampilan();  // Tampilkan waktu lokal yang dihitung maju
    setup_wifi();
  } else {
    // Jika Wi-Fi terhubung dan waktu belum diterima, update waktu NTP
    if (WiFi.status() == WL_CONNECTED && timeReceived) {
      timeClient.update();
      localTime = timeClient.getEpochTime() * 1000;  // Update waktu dari NTP
      lastMillis = millis();
      timeReceived = true;
      tampilan();
    }
    
  }
  // Serial.print("pesenan air: ");
  // Serial.println(levelAir);
  if (!client.connected()) {
    reconnect_mqtt();
  }
  digitalWrite(RELAY_PIN, relayStatus ? LOW : HIGH); // Aktifkan/Mematikan relay
  client.loop();
  // Update waktu NTP
  timeClient.update();
  // Panggil fungsi tampilan untuk menampilkan data di OLED
  tampilan();
  // Cek apakah sudah waktunya untuk mengirim data
  unsigned long currentTime = millis();
  if (relayStatus!=sentStatus) {
    delay(4000);
    sentStatus=relayStatus; // perbarui status pengiriman
    kirimRelayStatus();
  }

  delay(10);  // Tunggu 2 detik sebelum pembacaan berikutnya
}
