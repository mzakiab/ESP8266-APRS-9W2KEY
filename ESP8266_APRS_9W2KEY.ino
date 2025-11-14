/************************************************ 
    ESP8266 APRS - (SmartBeaconing) by: 9W2KEY
               9w2key.blogspot.com
*************************************************/

#include <ESP8266WiFi.h>
#include <ESP8266WiFiMulti.h>           // Kutukanah untuk multiple WiFi
#include <TinyGPS++.h>
#include <SoftwareSerial.h>
#include <WiFiClient.h>

// --- Konfigurasi WiFi ---
ESP8266WiFiMulti wifiMulti;             // untuk handle multiple wifi network

// --- Konfigurasi APRS ---
const char* callsign = "9W2KEY-12";           // Ganti dengan callsign Anda
const char* passcode = "15783";               // Ganti dengan passcode APRS-IS Anda
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
//const int scanGPS  =  1;      // LED 
//const int adaGPS   =  2;      // LED 
const int scanWiFi =   15;      // LED Kuning - D8
const int wiFiON   =   13;      // LED Hijau - D7
const int TXBeacon =    2;      // LED Merah - D4

// ---- Konfigurasi Buzzer ----
int buzzer          =    0;     // Pin untuk buzzer D3
int toneTX          =   4000;   // Tone TX
int toneKoner       =   3000;   // Tone Koner Leper
int toneISerr       =   50;     // Tone server terputus

// ---- Konfigurasi Butang TX Beacon Manual ----
#define BUTTON_PIN      4       // Pin D2
volatile boolean txRequest = false; // Menandakan permintaan TX manual beacon

// --- Pengaturan Waktu Beacon SmartBeaconing ---
const unsigned long BEACON_FAST_INTERVAL = 8000;      // 8 saat (ketika bergerak)
const unsigned long BEACON_SLOW_INTERVAL = 1800000;   // 30 minit (ketika tidak bergerak)
// const unsigned long BEACON_SLOW_INTERVAL = 15000;     // 15 saat, untuk testing system 
const float LOW_SPEED_THRESHOLD_KNOTS = 3.0;          // Di bawah nilai ini dianggap "tidak bergerak"
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
    loginStr += " vers 9W2KEY-ESP8266_APRS v2.2\r\n";
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

void sendAPRSBeacon(const char* comment) {
  // Format paket APRS: 9W2KEY-12>APKEY3,TCPIP*:@ddmmyyhhmmzLat Long SSS/CRS Comment
  
  // char buffer[100];
  char buffer[120];

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

  // Format altitude dalam meter
  int altitude_meters = (int)(gps.altitude.meters());
  
  // Membuat string paket APRS. '!' digunakan untuk posisi tanpa time-stamping.
  // Format: !DDMM.mmN\DDDMM.mmOCCC/SSSKomen.
  // tukar askara di tengah %sO%0 untuk tukar simbol.
  // Askara O dlm tu adalah untuk simbol Roket. 
  // sprintf(buffer, "!%s\\%sO%03d/%03d/A=%06d Experiment GPS + ESP8266 APRS | 9w2key.blogspot.com |", lat_buf, lon_buf, course_deg, speed_knots, altitude_meters);
  sprintf(buffer, "!%s\\%sO%03d/%03d/A=%06d %s", lat_buf, lon_buf, course_deg, speed_knots, altitude_meters, comment);

  Serial.print("Menghantar beacon APRS: "); 
  Serial.println(buffer); 

  // Mengirim paket ke APRS-IS
   if (sendAPRSMessage(buffer)) {
    Serial.println("Beacon berjaya dihantar.");
      digitalWrite(TXBeacon, HIGH);
      delay(200);
      digitalWrite(TXBeacon, LOW);
      tone(buzzer, toneTX); 
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
//                 Interrupt Service Routine (ISR)
// =========================================================================
// ISR untuk mengendalikan penekanan butang
void IRAM_ATTR handleButtonPress() {
  txRequest = true;     // Tetapkan flag permintaan TX
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

  // --- Konfigurasi WiFiMulti ---
  WiFi.mode(WIFI_STA);                                          // Tetapkan mod kepada Station
  wifiMulti.addAP("ABEKEY", "ingatfreeko");                     // Hok spok talipong 1
  wifiMulti.addAP("nokpasswordwifiko@unifi", "ingatfreeko");    // Wifi rumah 1
  wifiMulti.addAP("nokgapotuu_2.4GHz@unifi", "ingatfreeko");    // Wifi rumah 2
  wifiMulti.addAP("9W2KEY-VIVO", "ingatfreeko");                // Hok spok talipong 2
  // Tambah lagi rangkaian lain di bawah ni jika ada:
  // wifiMulti.addAP("SSID BARU", "PASSWORD BARU");             // Tambah Rangkaian 5

  // --- Logik Sambungan WiFi Baharu ---
  Serial.println("Menyambung ke mana-mana rangkaian WiFi yang ada...");
  while (wifiMulti.run() != WL_CONNECTED) { 
    delay(500); 
    Serial.print(".");
    digitalWrite(scanWiFi, HIGH);
    digitalWrite(wiFiON, LOW);
    delay(30);
    digitalWrite(scanWiFi, LOW);
    digitalWrite(wiFiON, LOW); 
  }

  Serial.println("\nWiFi bersambung."); 
  Serial.print("Bersambung ke SSID: ");
  Serial.println(WiFi.SSID());              // Tunjukkan SSID mana yang berjaya disambung
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

// --- Setup untuk butang TX Beacon ---
// Tambah kod ini untuk butang tekan
pinMode(BUTTON_PIN, INPUT_PULLUP); // Tetapkan pin butang sebagai input dengan pull-up dalaman
// Lampirkan interrupt: Apabila butang ditekan (FALLING edge), panggil fungsi handleButtonPress
attachInterrupt(digitalPinToInterrupt(BUTTON_PIN), handleButtonPress, FALLING);

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

const char* stationary_comment = "Experiment ESP8266 APRS: Parking mode | 9w2key.blogspot.com |";   // Tak gerak hantar koment ini
const char* moving_comment =     "Experiment ESP8266 APRS: Mobile mode | 9w2key.blogspot.com |";    // Bergerak hantar hok ni
const char* tekanButang =        "ESP8266 APRS: == ACTIVE == | 9w2key.blogspot.com |";              // Komen ni bila tekan butang TX

  // --- Pengurusan Sambungan WiFi dan APRS-IS (Penting) ---
  // Fungsi wifiMulti.run() perlu sentiasa dipanggil dalam loop()
  // untuk memastikan ia menguruskan sambungan semula WiFi secara automatik.
  if (wifiMulti.run() == WL_CONNECTED) {
    digitalWrite(wiFiON, HIGH);
    digitalWrite(scanWiFi, LOW);

  // Untuk make sure konet seng ke APRS-IS tidak putus.
  // Kalu putus, dia akan try cuba sambung lagi dan lagi sampai mung tutup
  if (!tcpClient.connected()) {
    aprsConnected = false; 
    Serial.println("Sambungan ke APRS-IS terputus, mencuba untuk sambungkan lagi...");
    digitalWrite(wiFiON, LOW);
    digitalWrite(scanWiFi, HIGH);
    tone(buzzer, toneISerr);
    delay(100);
    digitalWrite(scanWiFi, LOW);
    delay(50);
    noTone(buzzer);
    delay(100);
    tone(buzzer, toneISerr);
    delay(50);
    noTone(buzzer);
    delay(2000); 
    if (connectAPRS(aprs_server, aprs_port)) {
      Serial.println("Okay... sudah berjaya sambung semula dengan APRS-IS. Standby nak hantar beacon!");
      digitalWrite(wiFiON, HIGH);
      digitalWrite(scanWiFi, LOW);
      noTone(buzzer);

        Serial.println("\nWiFi bersambung semula."); 
        Serial.print("Bersambung ke SSID: ");
        Serial.println(WiFi.SSID());              // Tunjukkan SSID mana yang berjaya disambung
        Serial.print("Alamat IP: ");              // Tunjuk IP address
        Serial.println(WiFi.localIP()); 
    }
  }

  } else {
    // Jika WiFi terputus, kita cuba sambung semula guna wifiMulti.run()
    Serial.println("WiFi terputus. Mencuba sambung semula rangkaian...");
    digitalWrite(wiFiON, LOW);
    digitalWrite(scanWiFi, HIGH);       // Beri isyarat LED sedang scan
    delay(100);                         // Bagi dia berkelip sikit
    digitalWrite(scanWiFi, LOW);        // Bagi dia berkelip sikit
    delay(1000);                        // Tunggu sebentar sebelum cuba lagi
    return;                             // Langkau logik beacon buat sementara waktu
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
          tone(buzzer, toneKoner); 
          digitalWrite(TXBeacon, HIGH);
          delay(50);
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
      if (currentSpeedKnots <= LOW_SPEED_THRESHOLD_KNOTS) {
        sendAPRSBeacon(stationary_comment);
      } else {
      sendAPRSBeacon(moving_comment); 
      }
      lastBeaconTime = millis(); 
    }
  } else {
    Serial.println("Menunggu isyarat dari GPS ...");
        }

// Tambah kod ini untuk semak permintaan butang
if (txRequest) {
    Serial.println("Permintaan TX manual dikesan. Menghantar beacon APRS...");
    digitalWrite(TXBeacon, HIGH);
    tone(buzzer, toneISerr);
    sendAPRSBeacon(tekanButang);   // Panggil fungsi penghantaran beacon sedia ada
    delay(2000);
    noTone(buzzer);
    digitalWrite(TXBeacon, LOW);
    txRequest = false;  // Tetapkan semula bendera selepas penghantaran
   }

}
