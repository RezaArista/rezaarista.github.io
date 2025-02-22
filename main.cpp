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

//RELAY
bool relayStatus = false;      // Status relay (ON/OFF)
bool remoteStatus = false;   //dari esp ke 2
bool sentStatus = false; //yang dikirim ke mqtt
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
//INTERVAL UPDATE KE SERVER
unsigned long lastSendTime = 0; // Variabel untuk menyimpan waktu terakhir pengiriman
const unsigned long sendInterval = 10; // Interval pengiriman dalam milidetik (20 detik)

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

// Pin sensor water level
#define WATER_LEVEL_PIN A0

// Inisialisasi OLED
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
// Inisialisasi NTP Client
WiFiUDP udp;
NTPClient timeClient(udp, "pool.ntp.org", 7 * 3600, 60000); // GMT +7, update setiap 60 detik

unsigned long lastMillis = 0;  // Waktu milidetik terakhir untuk menghitung waktu maju
unsigned long localTime = 0; //waktu untuk lokal
bool timeReceived = false; // Flag untuk menentukan apakah waktu dari NTP sudah didapat

// Daftar SSID dan password Wi-Fi untuk koneksi otomatis
String ssid = "TRY";
String password = "pkid_2017";

// Pengaturan MQTT tanpa TLS
const char* mqtt_server = "1b1abc472a014a47a1cebe302164235c.s1.eu.hivemq.cloud"; //sesuaikan dengan mqtt anda
const int mqtt_port = 8883;
const char* mqtt_user = "rezaap1";
const char* mqtt_password = "Reza123456";
const char* mqtt_sts_topic = "Sts";  // Topik Event Ctrl
const char* mqtt_sts_level = "Lvl";  // Topik Event Ctrl


// Variabel untuk level air
const int maxSensorValue = 1023; // Nilai maksimal ADC (3.3V)
const int minSensorValue = 0;    // Nilai minimal ADC (0V)
const int lowWaterThreshold = 20; // Batas level rendah (%)
const int highWaterThreshold = 80; // Batas level tinggi (%)
float levelAir = 0;

void stopWifiReconnect() {
  wifiReconnectEnabled = false; // Matikan proses reconnect Wi-Fi
  Serial.println("Koneksi ulang Wi-Fi dihentikan oleh tombol.");
}

void kirimRelayStatus() {
  String relayMessage = relayStatus ? "1" : "0";  // Mengirim "1" jika relay ON, "0" jika relay OFF
  if (client.connected()) {
    client.publish(mqtt_sts_topic, relayMessage.c_str(), true);  // Publikasi status relay ke topik MQTT
    Serial.print("Status Relay Dikirim: ");
    Serial.println(relayMessage);
  } else {
    Serial.println("Gagal mengirim status relay. Tidak terhubung ke MQTT broker.");
  }
}

void tampilan() {
  // Membaca nilai sensor water level
  int sensorValue = analogRead(WATER_LEVEL_PIN);

  // Menghitung persentase level air
  float waterPercentage = map(sensorValue, minSensorValue, maxSensorValue, 0, 100);
  if (waterPercentage < 0) waterPercentage = 0;
  if (waterPercentage > 900) waterPercentage = 100;

  // Menampilkan hasil ke Serial Monitor
  // Serial.println("---------------------------------");
  // Serial.print("ADC Value: ");
  // Serial.println(sensorValue);

  // Serial.print("Water Level: ");
  // Serial.print(waterPercentage);
  // Serial.println("%");

  // Memberikan notifikasi berdasarkan batas tertentu
 if (waterPercentage > highWaterThreshold) {
    Serial.println("STATUS: Water level is HIGH!");
    relayStatus = false;
  } 

  // Menampilkan persentase di OLED
  display.clearDisplay();

  display.clearDisplay();  // Bersihkan layar
  unsigned long seconds, minutes, hours;
  if (WiFi.status() == WL_CONNECTED && timeReceived) {
    // Periksa apakah waktu sudah diterima dari NTP
    seconds = timeClient.getEpochTime();
    minutes = seconds / 60;
    hours = minutes / 60;
    seconds %= 60;
    minutes %= 60;
    hours %= 24;
  } else {
    unsigned long deltaTime = millis() - lastMillis;
    localTime += deltaTime; // Update waktu lokal berdasarkan millis()
    seconds = localTime / 1000;
    minutes = seconds / 60;
    hours = minutes / 60;
    seconds %= 60;
    minutes %= 60;
    hours %= 24;
  }
      // Ambil waktu dari NTPClient
  unsigned long epochTime = timeClient.getEpochTime();
    // Konversi epochTime menjadi time_t
  time_t timeInSeconds = static_cast<time_t>(epochTime);

    int signalStrength = -100;  // Default nilai jika Wi-Fi tidak terhubung
  if (WiFi.status() == WL_CONNECTED) {
    signalStrength = WiFi.RSSI();  // Ambil kekuatan sinyal jika terhubung
  } else {
    Serial.println("Tidak terhubung ke Wi-Fi");
  }
  // Serial.print("RSSI: ");
  // Serial.println(signalStrength);
  // Hitung berapa bar yang perlu ditampilkan berdasarkan kekuatan sinyal
  int bars = 0;
  // Serial.print("bar: ");
  // Serial.println(bars);
  if (signalStrength > -70) {
    bars = 4;  // Sinyal sangat kuat
  } else if (signalStrength > -80) {
    bars = 3;  // Sinyal cukup kuat
  } else if (signalStrength > -90) {
    bars = 2;  // Sinyal lemah
  } else if (signalStrength > -100) {
    bars = 1;  // Sinyal sangat lemah
  } else {
    bars = 0;  // Tidak ada sinyal
  }
  // Tampilkan simbol Wi-Fi di pojok kiri
  display.setTextSize(2);  // Ukuran teks lebih besar untuk simbol
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);

  for (int i = 0; i < 4; i++) {
    // Gambar bar berdasarkan jumlah bars
    int xPos = 0 + (i * 8);  // Posisi horizontal untuk setiap bar (lebih lebar antar bar)
    
    if (i < bars) {
      display.fillRect(xPos, 0, 5, 10, SSD1306_WHITE);  // Bar penuh (lebar 8, tinggi 20)
    } else {
      display.drawRect(xPos, 0, 5, 10, SSD1306_WHITE);  // Bar kosong (lebar 8, tinggi 20)
    }
  }
    // Tampilkan waktu di sisi kanan
  display.setCursor(60, 0);
  display.setTextSize(1);
  display.print("---");


  // Tampilkan waktu di sisi kanan
  display.setCursor(93, 0);
  display.setTextSize(1);
  if (hours < 10) {
    display.print("0"); // Menampilkan 0 sebelum menit
  }
  display.print(hours);
  display.print(":");
  // Menambahkan format dua digit untuk menit
  if (minutes < 10) {
    display.print("0"); // Menampilkan 0 sebelum menit
  }
  display.print(minutes);


  display.setTextSize(2); // Ukuran teks besar
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 20);
  display.print("LEVEL");
  display.print(relayStatus ? " ON" : " OFF");  // Display "ON" or "OFF" depending on the relay status

  display.setTextSize(3);
  display.setCursor(20, 40);
  display.print(waterPercentage);
  display.print("%");

  display.display();
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
  espClient.setInsecure();  // Menggunakan setInsecure() jika tanpa sertifikat CA

  
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
          if (!longPressDetected) {
            Serial.println("Tombol Flash ditekan singkat!");
        
            // Membaca nilai sensor water level
            int sensorValue = analogRead(WATER_LEVEL_PIN);
        
            // Menghitung persentase level air
            float waterPercentage = map(sensorValue, minSensorValue, maxSensorValue, 0, 100);
            if (waterPercentage < 0) waterPercentage = 0;
            if (waterPercentage > 900) waterPercentage = 100;
        
            // Serial.print("Water Level: ");
            // Serial.print(waterPercentage);
            // Serial.println("%");
        
            // Cek apakah waterPercentage melebihi batas tinggi (misalnya 80%)
            if (waterPercentage < highWaterThreshold) {
                Serial.println("Level air belum penuh.");
                relayStatus = !relayStatus;  // Toggle relay
                // Serial.print("Relay Status: ");
                // Serial.println(relayStatus ? "ON" : "OFF");
            } else {
                Serial.println("Level air sudah penuh");
                relayStatus = false;
            }
        
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

// Fungsi untuk membaca, menampilkan, dan log level air dalam satu fungsi
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

  // // Cek apakah topik adalah "Lvl" (level air)
  // if (strcmp(topic, "Lvl") == 0) {
  //   float levelValue = payloadStr.toFloat();  // Ubah payload ke float
  //   levelAir = levelValue;

  //   if (levelValue >= 100.0) {  // Jika level 100%, abaikan payload, matikan relay, dan publish ke STS
  //     Serial.println("Water level 100%. Ignoring any command.");
  //     remoteStatus = false;
  //     relayStatus = false; // Matikan relay secara paksa

  //     // Kirim status ke topik STS jika MQTT terhubung
  //     if (client.connected()) {
  //       client.publish("sts", "0", true); // Publikasi status 1 ke topik STS
  //       Serial.println("Water level 100% - Published STS 1");
  //     }

  //     return;  // Langsung keluar dari fungsi untuk mencegah eksekusi lebih lanjut
  //   }
  // }

  // Cek jika topik adalah mqtt_sts_topic
  if (strcmp(topic, "Sts") == 0) {
    if (levelAir >= 100.0) {
      Serial.println("Ignoring MQTT command because water level is 100%.");
      return; // Abaikan perintah jika water level 100%
    }

    if (payloadStr == "1") {
      remoteStatus = true;  // Relay ON
      Serial.println("Relay turned ON");
    } else if (payloadStr == "0") {
      remoteStatus = false;  // Relay OFF
      Serial.println("Relay turned OFF");
    }
  }

  // Sinkronisasi relayStatus dengan remoteStatus
  if (remoteStatus != relayStatus) {
    relayStatus = remoteStatus;
  }
}


void setup() {
  // Inisialisasi Serial Monitor
  Serial.begin(115200);

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
  client.setServer(mqtt_server, mqtt_port);
  setup_wifi();
  // Atur sertifikat root CA untuk koneksi TLS
  espClient.setInsecure();  // Menggunakan setInsecure() jika tanpa sertifikat CA
  client.setServer(mqtt_server, mqtt_port);
  client.setCallback(mqttCallback);
  // Inisialisasi pin sensor
  pinMode(WATER_LEVEL_PIN, INPUT);

  Serial.println("Water Level Monitoring Started...");
  Serial.println("---------------------------------");
}
void reconnect_mqtt() {
  String macAddress = WiFi.macAddress(); // MAC Address Wi-Fi
  String clientId = "ES" + macAddress;
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
        if (!longPressDetected) {
          Serial.println("Tombol Flash ditekan singkat!");
      
          // Membaca nilai sensor water level
          int sensorValue = analogRead(WATER_LEVEL_PIN);
      
          // Menghitung persentase level air
          float waterPercentage = map(sensorValue, minSensorValue, maxSensorValue, 0, 100);
          if (waterPercentage < 0) waterPercentage = 0;
          if (waterPercentage > 900) waterPercentage = 100;
      
          // Serial.print("Water Level: ");
          // Serial.print(waterPercentage);
          // Serial.println("%");
      
          // Cek apakah waterPercentage melebihi batas tinggi (misalnya 80%)
          if (waterPercentage < highWaterThreshold) {
              Serial.println("Level air belum penuh.");
              relayStatus = !relayStatus;  // Toggle relay
              // Serial.print("Relay Status: ");
              // Serial.println(relayStatus ? "ON" : "OFF");
          } else {
              Serial.println("Level air sudah penuh");
              relayStatus = false;
          
          }
      
          delay(10);
      }
        // Reset status tombol
        isButtonPressed = false;
        longPressDetected = false;
      }
    }
    Serial.print("Menghubungkan ke MQTT broker...");
    if (client.connect("ESP8266Cl", mqtt_user, mqtt_password)) { //gaboleh sama namanya
      Serial.println("Terhubung ke MQTT broker"); 
      client.subscribe(mqtt_sts_topic);
    } else {
      Serial.print("Gagal, rc=");
      Serial.print(client.state());
      Serial.println(" Coba lagi dalam 5 detik.");
      Serial.println(WiFi.status());
      setup_wifi();
      delay(10);
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
      if (!longPressDetected) {
        Serial.println("Tombol Flash ditekan singkat!");
    
        // Membaca nilai sensor water level
        int sensorValue = analogRead(WATER_LEVEL_PIN);
    
        // Menghitung persentase level air
        float waterPercentage = map(sensorValue, minSensorValue, maxSensorValue, 0, 100);
        if (waterPercentage < 0) waterPercentage = 0;
        if (waterPercentage > 900) waterPercentage = 100;
    
        // Serial.print("Water Level: ");
        // Serial.print(waterPercentage);
        // Serial.println("%");
    
        // Cek apakah waterPercentage melebihi batas tinggi (misalnya 80%)
        if (waterPercentage < highWaterThreshold) {
            Serial.println("Level air belum penuh.");
            relayStatus = !relayStatus;  // Toggle relay
            // Serial.print("Relay Status: ");
            // Serial.println(relayStatus ? "ON" : "OFF");
        } else {
            Serial.println("Level air sudah penuh");
            relayStatus = false;
        }
    
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
  if (!client.connected()) {
    reconnect_mqtt();
  }
  client.loop();
  // Update waktu NTP
  timeClient.update();
  // Panggil fungsi tampilan untuk menampilkan data di OLED
  tampilan();
  // Cek apakah sudah waktunya untuk mengirim data
  unsigned long currentTime = millis();
  if (currentTime - lastSendTime >= sendInterval) {
    lastSendTime = currentTime;
    
    // Membaca nilai sensor water level
    int sensorValue = analogRead(WATER_LEVEL_PIN);
    
    // Menghitung persentase level air dengan float agar lebih akurat
    float waterPercentage = ((float)(sensorValue - minSensorValue) / (maxSensorValue - minSensorValue)) * 100.0;

    // Pastikan nilai tetap dalam rentang 0 - 100%
    waterPercentage = constrain(waterPercentage, 0.0, 100.0);

    // Kirim data ke MQTT jika koneksi tersedia
    if (client.connected()) {
        String waterLevelStr = String(waterPercentage, 2);  // Konversi float ke String
        client.publish(mqtt_sts_level, waterLevelStr.c_str(), true);  // Kirim ke MQTT
        
        Serial.print("Level dikirim: ");
        Serial.println(waterLevelStr);
        if (waterPercentage > highWaterThreshold) {
          Serial.println("STATUS: Water level is HIGH!");
        } 
    } else {
        Serial.println("Gagal mengirim level air. Tidak terhubung ke MQTT broker.");
    }
  }

  if (relayStatus!=sentStatus) {
    delay(5000);
    sentStatus=relayStatus; // perbarui status pengiriman
    kirimRelayStatus();
  }


  delay(10);  // Tunggu 2 detik sebelum pembacaan berikutnya
}
