/*********************************************** 
    ESP8266 APRS - (SmartBeaconing) by: 9W2KEY
               9w2key.blogspot.com
************************************************/

#include <ESP8266WiFi.h>
#include <TinyGPS++.h>
#include <SoftwareSerial.h>
#include <WiFiClient.h>

// --- Konfigurasi WiFi ---
const char* ssid = "xxxxxxxxxxxx";            // Masukkan SSID Wifi anda
const char* password = "xxxxxxxx";            // Masukkan password Wifi

// --- Konfigurasi APRS ---
const char* callsign = "XXXXXXXXX";           // Ganti dengan callsign Anda
const char* passcode = "xxxxx";               // Ganti dengan passcode APRS-IS Anda
const char* aprs_server = "asia.aprs2.net";   // Server APRS-IS 
const uint16_t aprs_port = 14580;             // Port untuk APRS

/********
Berikut adalah senarai Server APRS yang boleh diguna:-
1. rotate.aprs2.net (Global APRS Server)
2. euro.aprs2.net (Europe)
3. austria.aprs2.net
4. aprs.jasra.org.my (Johor, MY)
*********/

// --- Konfigurasi GPS ---
static const int RXPin = 14, TXPin = 12;      // Pin GPS RX --> D6 dan GPS TX --> D5 pada NodeMCU/ESP8266
static const uint32_t GPSBaud = 9600;
TinyGPSPlus gps;
SoftwareSerial ss(RXPin, TXPin);              // RX, TX

// --- Konfigurasi LED ---
const int scanWiFi =   15;      // LED Kuning - D8
const int wiFiON   =   13;      // LED Hijau - D7
const int TXBeacon =    2;      // LED Merah - D4

// ---- Konfigurasi Buzzer ----
int buzzer         =    0;      // Pin untuk buzzer D3

// --- Pengaturan Waktu Beacon SmartBeaconing ---
const unsigned long BEACON_FAST_INTERVAL = 8000;      // 8 saat (ketika bergerak)
const unsigned long BEACON_SLOW_INTERVAL = 1800000;   // 30 minit (ketika tidak bergerak)
const float LOW_SPEED_THRESHOLD_KNOTS = 1.0;          // Di bawah ini dianggap "tidak bergerak"
const float TURN_ANGLE_THRESHOLD_DEG = 25.0;          // Sudut konar leper minimum untuk triger beacon (25 darjah)

unsigned long lastBeaconTime = 0;
float lastCourse = 0.0;                               // Untuk pelago dengan arah terakhir konar lepar tadi

// ---- APRS-IS client implementation ----
WiFiClient tcpClient;
bool aprsConnected = false; 

// =========================================================================
//                          FUNGSI UTAMA APRS
// =========================================================================

bool connectAPRS(const char* server, uint16_t port) {
  if (tcpClient.connect(server, port)) {

    String loginStr = "user ";
    loginStr += callsign; 
    loginStr += " pass ";
    loginStr += passcode; 
    loginStr += " vers 9W2KEY-ESP8266_APRS 1.0\r\n";
    tcpClient.print(loginStr); 
    aprsConnected = true; 
    return true;
  }
  return false;
}

bool sendAPRSMessage(const char* message) {
  if (aprsConnected && tcpClient.connected()) {
    tcpClient.print(callsign); 
    tcpClient.print(">APKEY3,TCPIP*:");       // Device ATMega328P APRS Tracker by 9W2KEY
    tcpClient.print(message); 
    tcpClient.print("\r\n"); 
    return true;
  }
  return false;
}

void sendAPRSBeacon() {
  // Format paket APRS: 9W2KEY-12>APKEY3,TCPIP*:@ddmmyyhhmmzLat Long SSS/CRS Comment
  
  char buffer[100]; 

  // Format koordinat lintang (latitude) APRS
  char lat_buf[12];
  float lat = gps.location.lat();
  char lat_dir = (lat >= 0) ? 'N' : 'S'; 
  // DDMM.mmN/S
  sprintf(lat_buf, "%02d%05.2f%c", (int)abs(lat), fmod(abs(lat) * 60, 60), lat_dir);

  // Format koordinat bujur (longitude) APRS
  char lon_buf[12]; 
  float lon = gps.location.lng(); 
  char lon_dir = (lon >= 0) ? 'E' : 'W'; 
  // DDDMM.mmE/W
  sprintf(lon_buf, "%03d%05.2f%c", (int)abs(lon), fmod(abs(lon) * 60, 60), lon_dir); 

  // Format derasan (speed) dan arah (course)
  // Kederasan dalam knot (SSS), Arah dalam darjah (CRS)
  int speed_knots = (int)(gps.speed.knots()); // Kederasan dalam knot
  int course_deg = (int)(gps.course.deg());   // Arah dalam darjah
  
  // Membuat string paket APRS. '!' digunakan untuk posisi tanpa time-stamping.
  // Format: !DDMM.mmN\DDDMM.mmOCCC/SSSKomen.
  // tukar askara di tengah %sO%0 untuk tukar simbol.
  // Askara O dlm tu adalah untuk simbol Roket. 
  sprintf(buffer, "!%s\\%sO%03d/%03d Experiment ESP8266 APRS by 9W2KEY | 9w2key.blogspot.com |", lat_buf, lon_buf, course_deg, speed_knots); 


  Serial.print("Menghantar beacon APRS: "); 
  Serial.println(buffer); 

  // Mengirim paket ke APRS-IS
   if (sendAPRSMessage(buffer)) {
    Serial.println("Beacon berjaya dihantar.");
      digitalWrite(TXBeacon, HIGH);
      delay(200);
      digitalWrite(TXBeacon, LOW);
      tone(buzzer, 1100); 
      delay(50);
      digitalWrite(TXBeacon, HIGH);
      noTone(buzzer);
      delay(200);
      digitalWrite(TXBeacon, LOW);
  } else {
    Serial.println("Gagal menghantar beacon."); 
  }
}

// =========================================================================
//                               SETUP
// =========================================================================

void setup() {

{
  //pinMode(scanGPS, OUTPUT);
  //pinMode(adaGPS, OUTPUT);
  pinMode(scanWiFi, OUTPUT);
  pinMode(wiFiON, OUTPUT);
  pinMode(TXBeacon, OUTPUT);
  pinMode(buzzer, OUTPUT);
}

  Serial.begin(115200);
  ss.begin(GPSBaud);                  // Memula komunikasi serial dengan GPS
  Serial.println("\nScan WiFi..."); 
  WiFi.begin(ssid, password); 
  while (WiFi.status() != WL_CONNECTED) { 
    delay(500); 
    Serial.print(".");
    digitalWrite(scanWiFi, HIGH);
    digitalWrite(wiFiON, LOW);
    delay(30);
    digitalWrite(scanWiFi, LOW);
    digitalWrite(wiFiON, LOW); 
  }

  Serial.println("\nWiFi bersambung."); 
  Serial.print("Alamat IP: ");
  Serial.println(WiFi.localIP()); 

  digitalWrite(scanWiFi, LOW);
  digitalWrite(wiFiON, HIGH);

  Serial.println("Menghubungkan tracker ke APRS-IS..."); 
  if (connectAPRS(aprs_server, aprs_port)) { 
    Serial.println("Tracker telah bersambung ke APRS-IS."); 
  } else {
    Serial.println("Gagal terhubung ke APRS-IS."); 
  }

}

// =========================================================================
//                    LOOP (SmartBeaconing Logic)
// =========================================================================

void loop() {
  // Memproses data dari GPS secara terus menerus
  while (ss.available() > 0) {
    if (gps.encode(ss.read())) {
      // Data GPS lengkap telah diterima dan di-encode
    }
  }

  // Untuk make sure konet seng ke APRS-IS tidak putus.
  // Kalu putus, dia akan try cuba sambung lagi dan lagi sampai mung tutup
  if (!tcpClient.connected()) {
    aprsConnected = false; 
    Serial.println("Sambungan ke APRS-IS terputus, mencuba untuk sambungkan lagi...");
    digitalWrite(wiFiON, HIGH);
    delay(30);
    digitalWrite(wiFiON, LOW); 
    if (connectAPRS(aprs_server, aprs_port)) {
      Serial.println("Okay... sudah berjaya sambung dengan APRS-IS. Standby nak hantar beacon!");
      digitalWrite(wiFiON, HIGH);
    }
  }

  // --- SmartBeaconing ---
  if (gps.location.isValid()) {
    float currentSpeedKnots = gps.speed.knots(); 
    float currentCourseDeg = gps.course.deg();

    unsigned long currentInterval = BEACON_SLOW_INTERVAL;
    bool forceBeacon = false;

    // 1. Tentukan Interval Berasaskan Kecepatan
    if (currentSpeedKnots > LOW_SPEED_THRESHOLD_KNOTS) {
      // Bergerak: Gunakan interval cepat (8 saat)
      currentInterval = BEACON_FAST_INTERVAL;
    } else {
      // Tidak bergerak: Gunakan interval lambat (30 minit)
      currentInterval = BEACON_SLOW_INTERVAL;
    }

    // 2. Periksa Perubahan Arah (Hanya ketika bergerak)
    if (currentSpeedKnots > LOW_SPEED_THRESHOLD_KNOTS) {
      if (lastCourse != 0.0) { 
        float courseDelta = abs(currentCourseDeg - lastCourse);
        
        // Putaran dari 360 ke 0 (atau sebaliknya)
        if (courseDelta > 180.0) {
          courseDelta = 360.0 - courseDelta;
        }

        // Jika perubahan arah melebihi konar leper, TX new beacon
        if (courseDelta >= TURN_ANGLE_THRESHOLD_DEG) {
          forceBeacon = true;
          Serial.print("SmartBeacon: Perubahan arah (");
          Serial.print(courseDelta);
          Serial.println(" deg) menghantar beacon.");
          
          digitalWrite(TXBeacon, HIGH);
          delay(200);
          digitalWrite(TXBeacon, LOW);
          tone(buzzer, 1100); 
          delay(50);
          digitalWrite(TXBeacon, HIGH);
          noTone(buzzer);
          delay(200);
          digitalWrite(TXBeacon, LOW);
        }
      }
      lastCourse = currentCourseDeg; // Refresh lastCourse ketika bergerak
    } else {
      lastCourse = 0.0; // Setel ulang lastCourse ketika tidak bergerak
    }
    
    // 3. TX Beacon jika terpaksa atau interval sudah cukup masa
    if (forceBeacon || (millis() - lastBeaconTime >= currentInterval)) {
      sendAPRSBeacon();
      lastBeaconTime = millis(); 
    }
  } else {
    Serial.println("Menunggu isyarat dari GPS ...");
        }
}
