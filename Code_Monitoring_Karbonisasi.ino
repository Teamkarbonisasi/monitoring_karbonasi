// library yang digunakan
#include <Arduino.h>
#include <WiFi.h>
#include <FirebaseESP32.h>
#include <LiquidCrystal_I2C.h>
#include <max6675.h>
#include <NTPClient.h>
#include <WiFiUdp.h>

// wifi credentials
#define WIFI_SSID "leppi"
#define WIFI_PASSWORD "qosmantap"

// firebase credentials
#define DATABASE_SECRET "izBiv7KFFHYJPHlDjFiNR1bSi7HVOcNTGkeeBdUA"
#define DATABASE_URL "https://team-karbonisasi-default-rtdb.asia-southeast1.firebasedatabase.app/"

// nomor pin sensor
int smokePin = 35, sck = 12, cs = 14, so = 27;


// variabel firebase json untuk mengirim data ke realtime database
FirebaseData fbdo;
FirebaseJson json;
FirebaseJson monitor;

// data user
const String USER_UID = "hRyIHp6qNsZW4juyywiBKBwX23S2";
const String sensorPath = "sensors/" + USER_UID + "/";

String proses = "idle";
String bucketPath = "";
bool record = false;
int counter = 1;

MAX6675 suhu(sck, cs, so);
LiquidCrystal_I2C lcd(0x27, 16, 2);

// setup ntp client
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org");
String months[12]={"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};

// function declaration
void setStore();
bool getStateRecord();
void connectingWifi();
void connectingFirebase();
void sendData();
void monitoringData();
float calculateSmoke();
void displayLCD();
String getId();
String getTime();
String getDate();

void setup(){
  Serial.begin(9600);

  lcd.init();
  lcd.backlight();
  connectingWifi();
  connectingFirebase();
  setStore();

  timeClient.begin();
  timeClient.setTimeOffset(25200);

  pinMode(smokePin, INPUT); 

  lcd.clear();
}

void loop(){
  // menjalankan setiap fungsi setiap 1 detik sekali
  if (Firebase.ready()){
    // update time
    timeClient.update();

    sendData();
    monitoringData();
    displayLCD();
  }
  delay(1000);
}

void connectingWifi(){
  // konek ke wifi
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Connecting to Wi-Fi");
  while (WiFi.status() != WL_CONNECTED){
      lcd.setCursor(0, 0);
    lcd.print("Menghubungkan...");
    Serial.print(".");
    delay(500);
  }
  Serial.println();

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Terhubung");
}

void connectingFirebase(){
  // proses koneksi ke firebase realtime database
  Serial.print("Connected with IP: ");
  Serial.println(WiFi.localIP());
  Serial.println();

  Firebase.begin(DATABASE_URL, DATABASE_SECRET);
  Firebase.reconnectWiFi(true);
}

String getId(){
  time_t epochTime = timeClient.getEpochTime();
  struct tm *ptm = gmtime ((time_t *)&epochTime);

  int hour = timeClient.getHours();
  int minute = timeClient.getMinutes();
  int second = timeClient.getSeconds();
  int day = ptm->tm_mday;
  int month = ptm->tm_mon+1;
  int year = ptm->tm_year+1900;

  return String(day) + String(month) + String(year) + String(hour) + String(minute) + String(second);
}

String getTime(){
  int currentHour = timeClient.getHours();
  int currentMinute = timeClient.getMinutes();
  return String(currentHour) + ":" + String(currentMinute);
}

String getDate(){
  time_t epochTime = timeClient.getEpochTime();
  struct tm *ptm = gmtime ((time_t *)&epochTime); 

  int monthDay = ptm->tm_mday;
  int currentMonth = ptm->tm_mon+1;
  String currentMonthName = months[currentMonth-1];
  int currentYear = ptm->tm_year+1900;

  return String(monthDay) + "-" + currentMonthName + "-" + String(currentYear);
}

float calculateSmoke(){
  // proses perhitungan nilai asap ke satuan ppm
  float data = analogRead(smokePin);
  float maxValue = 4095;

  return data / maxValue * 100;
}

void setStore(){
  // membuat inisiasi database jika database untuk uid tersebut belum ada
  if(!Firebase.RTDB.getBool(&fbdo, sensorPath)){
    Firebase.RTDB.setInt(&fbdo, sensorPath + "monitor", 0);
    Firebase.RTDB.setBool(&fbdo, sensorPath + "recording", false);
  }
}

bool getStateRecord() {
  // mengecek apakah status di realtime database sedang berlangsung atau tidak
  String path = sensorPath + "recording";
  bool result;

  Firebase.RTDB.getBool(&fbdo, path, &result);

  return result;
}

void displayLCD(){
  // menampilkan proses ke lcd
  if(calculateSmoke() > 20.00 && suhu.readCelsius() > 35){
    if(proses != "berlangsung"){
      lcd.clear();
    }
    lcd.setCursor(5,0);
    lcd.print("Proses");
    lcd.setCursor(3,1);
    lcd.print("Berlangsung");
    proses = "berlangsung";
  }else{
    if(proses != "idle"){
      lcd.clear();
    }
    lcd.setCursor(3,0);
    lcd.print("Alat Siap");
    lcd.setCursor(3,1);
    lcd.print("Digunakan");
    proses = "idle";
  }
}

void monitoringData(){
  // mengirim data suhu dan asap ke realtime database
  const String path = sensorPath + "monitor";

  monitor.add("suhu", suhu.readCelsius());
  monitor.add("smoke", calculateSmoke());

  Firebase.RTDB.setJSON(&fbdo, path, &monitor);
  monitor.clear();
}

void sendData(){
  // mengirim data history ke realtime database
  if(getStateRecord()){
    // memulai recording data untuk nanti disimpan ke history
    if(bucketPath == ""){
      Serial.print(getId());
      bucketPath = sensorPath + "histories/" + getId(); 
      json.add("startTime", getTime());
      json.add("endTime", "");
      json.add("date", getDate());
      Firebase.RTDB.setJSON(&fbdo, bucketPath, &json);
      record = true;
      json.clear();
    }

    json.add("suhu", suhu.readCelsius());
    json.add("smoke", calculateSmoke());

    Firebase.RTDB.setJSON(&fbdo, bucketPath + "/data/" + String(counter), &json);
    json.clear();
    counter++;
  }else{
    // mengirim data hasil record ke realtime database
    if(record){
      Firebase.RTDB.setString(&fbdo, bucketPath + "/endTime", getTime()); 
  
      record = false;
      bucketPath = "";
      counter = 1;
    }
  }
}
