#define TINY_GSM_MODEM_SIM800
#define TINY_GSM_RX_BUFFER 256
#include <TinyGPS++.h>
#include <TinyGsmClient.h>
#include <ArduinoHttpClient.h>

const char FIREBASE_HOST[]  = "SUA-HOST-FIREBASE";
const String FIREBASE_AUTH  = "SUA-KEY-FIREBASE";
const String FIREBASE_PATH  = "/";
const int SSL_PORT          = 443;

char apn[]  = "java.claro.com.br";
char user[] = "Claro";
char pass[] = "Claro";


#define rxPin 4
#define txPin 2
HardwareSerial sim800(1);
TinyGsm modem(sim800);


#define RXD2 16
#define TXD2 17
HardwareSerial neogps(2);
TinyGPSPlus gps;

TinyGsmClientSecure gsm_client_secure_modem(modem, 0);
HttpClient http_client = HttpClient(gsm_client_secure_modem, FIREBASE_HOST, SSL_PORT);


unsigned long previousMillis = 0;
long interval = 5000;


void setup() {
  Serial.begin(115200);
  Serial.println("Iniciando ESP32");
  sim800.begin(9600, SERIAL_8N1, rxPin, txPin);
  Serial.println("Iniciando SIM800L");
  neogps.begin(9600, SERIAL_8N1, RXD2, TXD2);
  Serial.println("Iniciando NeoGPS");
  delay(3000);
  Serial.println("Iniciando modem...");
  modem.restart();
  String modemInfo = modem.getModemInfo();
  Serial.print("Modem: ");
  Serial.println(modemInfo);
  
  http_client.setHttpResponseTimeout(50 * 1000);
  delay(30000);
}

void loop() {
  Serial.print(F("Conectando a "));
  Serial.print(apn);
  if (!modem.gprsConnect(apn, user, pass)) {
    Serial.println(" Falha ao conectar");
    delay(1000);
    return;
  }
  Serial.println(" OK");

  
  http_client.connect(FIREBASE_HOST, SSL_PORT);
  

  while (true) {
    if (!http_client.connected()) {
      Serial.println();
      http_client.stop();
      Serial.println("Não conectou ao FIREBASE");
      break;
    }
    else{
      gps_loop();
    }
  }
}

void PostToFirebase(const char* method, const String & path , const String & data, HttpClient* http) {
  String response;
  int statusCode = 0;
  http->connectionKeepAlive();  

  String url;
  if (path[0] != '/') {
    url = "/";
  }
  url += path + ".json";
  url += "?auth=" + FIREBASE_AUTH;
  Serial.print("POST:");
  Serial.println(url);
  Serial.print("Data:");
  Serial.println(data);
  
  http->post(url, "application/json", data);
  
  statusCode = http->responseStatusCode();
  Serial.print("Status code: ");
  Serial.println(statusCode);
  if (statusCode == -3) {
    Serial.println("Reiniciando o processo de conexão GPRS...");
    http->stop();
    modem.gprsDisconnect();

    Serial.print(F("Conectando a "));
    Serial.println(apn);
    if (!modem.gprsConnect(apn, user, pass)) {
      Serial.println("Falha ao reconectar");
      delay(1000);
      return;
    }
    
    Serial.println("Reconectado à rede GPRS com sucesso.");
    http->connect(FIREBASE_HOST, SSL_PORT); 
  } else{
      response = http->responseBody();
      Serial.print("Response: ");
      Serial.println(response);
  }

  if (!http->connected()) {
    Serial.println();
    http->stop();// Shutdown
    Serial.println("Desconectado do FIREBASE");
  }


}


void gps_loop() {
  boolean newData = false;
  for (unsigned long start = millis(); millis() - start < 2000;) {
    while (neogps.available()) {
      if (gps.encode(neogps.read())) {
        newData = true;
        break;
      }
    }
  }

  if (newData) {
    String latitude, longitude;
    int horas, minutos;
    unsigned long dia, mes, ano;

    if (gps.location.lat() == 0.0) {
      latitude = "-4.447959";
    } else {
      latitude = String(gps.location.lat(), 6);
    }

    if (gps.location.lng() == 0.0) {
      longitude = "-41.457627";
    } else {
      longitude = String(gps.location.lng(), 6);
    }

    dia = gps.date.day();
    mes = gps.date.month();
    ano = gps.date.year();

    horas = gps.time.hour() - 3;
    if (horas ==-3){
      horas = 21;
    }
    
    minutos = gps.time.minute();
    String minuto;
    if (minutos < 10) {
      minuto = "0" + String(minutos);
    } else {
      minuto = String(minutos);
    }

    // velocidade = gps.speed.kmph();

    String gpsData = "{";
    gpsData += "\"latitude\":" + latitude + ",";
    gpsData += "\"longitude\":" + longitude + ",";
    gpsData += "\"data\":\"" + String(dia) + "-" + String(mes) + "-" + String(ano) + "\",";
    gpsData += "\"horario\":\"" + String(horas) + ":" + String(minuto) + "\"";
    gpsData += "}";

    PostToFirebase("PATCH", FIREBASE_PATH, gpsData, &http_client);
  }
}