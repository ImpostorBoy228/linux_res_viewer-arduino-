#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <ArduinoJson.h>
#include <IRremote.h>

LiquidCrystal_I2C lcd(0x27,16,2);
const int RECV_PIN = 11;

int currentPage = 0;
const int TOTAL_PAGES = 4;
unsigned long lastUpdate = 0;
bool needRefresh = true;
unsigned long lastIR = 0;
const unsigned long IR_DELAY = 300;

String inputBuffer = "";

float cpu=0, ram=0, cpu_speed=0, wl_recv=0, load_avg=0, cpu_temp=0, gpu_temp=0;

// последние значения для сравнения (для частичного обновления)
float last_cpu=-1, last_ram=-1, last_cpu_speed=-1, last_load=-1;
float last_wl=-1, last_cpu_temp=-1, last_gpu_temp=-1;

void setup() {
  Serial.begin(9600);
  IrReceiver.begin(RECV_PIN, ENABLE_LED_FEEDBACK);
  lcd.init();
  lcd.backlight();
  lcd.setCursor(0,0);
  lcd.print(" PC MONITOR ");
  lcd.setCursor(0,1);
  lcd.print("   READY!   ");
}

void loop() {
  // ---- IR: переключение страницы с debounce ----
  if (IrReceiver.decode()) {
    if (millis() - lastIR > IR_DELAY) {
      currentPage = (currentPage + 1) % TOTAL_PAGES;
      needRefresh = true;
      lastIR = millis();
    }
    IrReceiver.resume();
  }

  // ---- Serial: чтение JSON ----
  while (Serial.available()) {
    char c = (char)Serial.read();
    inputBuffer += c;
    if (c == '>' && inputBuffer.endsWith("</JSON>")) {
      parseJSON(inputBuffer);
      inputBuffer = "";
      needRefresh = true;
    }
    if (inputBuffer.length() > 128) inputBuffer = "";
  }

  // ---- Обновление LCD ----
  if (needRefresh && millis() - lastUpdate > 400) {
    showPage(currentPage);
    lastUpdate = millis();
    needRefresh = false;
  }
}

// ---- JSON парсинг ----
void parseJSON(String buf) {
  int s = buf.indexOf('{');
  int e = buf.lastIndexOf('}');
  if (s==-1 || e==-1) return;

  StaticJsonDocument<192> doc;
  DeserializationError err = deserializeJson(doc, buf.substring(s,e+1));
  if (err) return;

  cpu       = doc["cpu"];
  ram       = doc["ram"];
  cpu_speed = doc["cpu_speed"];
  wl_recv   = doc["wl_recv"];
  load_avg  = doc["load_avg_5min"];
  cpu_temp  = doc["cpu_temp"];
  gpu_temp  = doc["gpu_temp"];
}

// ---- Показ страниц с частичным обновлением ----
void showPage(int p) {
  switch(p){
    case 0: // CPU + RAM на двух строках
      lcd.setCursor(0,0);
      if(cpu != last_cpu){
        lcd.print("CPU: "); lcd.print(cpu,0); lcd.print("%   ");
        last_cpu = cpu;
      }
      lcd.setCursor(0,1);
      if(ram != last_ram){
        lcd.print("RAM: "); lcd.print(ram,0); lcd.print("%   ");
        last_ram = ram;
      }
      break;
    case 1: // CPU SPD + LOAD5m
      lcd.setCursor(0,0);
      if(cpu_speed != last_cpu_speed){
        lcd.print("SPD:"); lcd.print(cpu_speed,2); lcd.print("GHz ");
        last_cpu_speed = cpu_speed;
      }
      lcd.setCursor(0,1);
      if(load_avg != last_load){
        lcd.print("LA(5m):"); lcd.print(load_avg,2); lcd.print("     ");
        last_load = load_avg;
      }
      break;
    case 2: // WiFi RX
      lcd.setCursor(0,0);
      if(wl_recv != last_wl){
        lcd.print("WiFi RX:       "); // очистка остатка
        lcd.setCursor(0,1);
        lcd.print(wl_recv,2); lcd.print(" KB/s       ");
        last_wl = wl_recv;
      }
      break;
    case 3: // CPU + GPU Temp на двух строках
      lcd.setCursor(0,0);
      if(cpu_temp != last_cpu_temp){
        lcd.print("CPU: "); lcd.print(cpu_temp,0); lcd.print("C   ");
        last_cpu_temp = cpu_temp;
      }
      lcd.setCursor(0,1);
      if(gpu_temp != last_gpu_temp){
        lcd.print("GPU: "); lcd.print(gpu_temp,0); lcd.print("C   ");
        last_gpu_temp = gpu_temp;
      }
      break;
  }
}
