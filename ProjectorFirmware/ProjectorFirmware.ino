#include <Arduino.h>
#include <SPI.h>
#include <U8g2lib.h>

#define LCDWidth           u8g2.getDisplayWidth()
#define ALIGN_CENTER(t)    ((LCDWidth - (u8g2.getUTF8Width(t))) / 2)
#define ALIGN_RIGHT(t)     (LCDWidth -  u8g2.getUTF8Width(t))
#define ALIGN_LEFT         0

#define PIN_LCD_RESET 10
#define PIN_STATUS_LED 17
#define PIN_PWM_LED 9

/*
  Projector Firmware von Constantin Zborowska
  Version 30.12.2020
  
  Programmiert für die Verwendung mit MieleSmartG470

  Serial Data Befehl:

  BEFEHL: ETE0000PR000T00M0U0E0A0H0X
  EINHEIT:    s    %  °C

  Wenn PR > 100 wird die Projector LED Deaktiviert
  
*/

U8G2_ST7567_HEM6432_F_HW_I2C u8g2(U8G2_R0, /* reset=*/ PIN_LCD_RESET);

char serialReceiveArray[32];
char restTemp[] = "0000";
bool serialReceiveComplete = false;
bool serialReceiveTotal = false;
int serialReceiveByteCount = 0;

//Display Informations
int ete_timer = 0; //in seconds
int temperatur = 0;
int progress = 0;
bool statusIntake = true;
bool statusOuttake = true;
bool statusCirculator = true;
bool statusHeater = true;

bool beamerLEDOn = true;

int ani_counter = 0;
bool ani_statusHeater = false;
bool ani_statusIntake = false;
bool ani_statusOuttake = false;
bool ani_statusCirculator = false;
bool ani_cursor = false;
char *timeToChars = (char*)malloc(2);

long timer_last_exec = 0;

void setup(void) {
  Serial.begin(9600);
  u8g2.setBusClock(100000);
  u8g2.begin();

  pinMode(PIN_STATUS_LED, OUTPUT);
  pinMode(PIN_PWM_LED, OUTPUT);
  analogWrite(PIN_PWM_LED, 255);

  u8g2.setDisplayRotation(U8G2_R2);

  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_t0_22b_mr);
  u8g2.drawUTF8(ALIGN_CENTER("MIELE"), 28, "MIELE");
  u8g2.sendBuffer();
  
  delay(1000);

  //Serial.println("Start Programm");
}

void loop(void) {
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_crox4hb_tf);
  u8g2.setContrast(255);

  //Serial Received Loop
  while (Serial.available() > 0) {
    serialReceiveArray[serialReceiveByteCount] = Serial.read();
    if (serialReceiveArray[serialReceiveByteCount] == 'X') {
      if (serialReceiveTotal) {
        serialReceiveComplete = true; 
      } else {
        serialReceiveComplete = false;
        serialReceiveByteCount = 0;
      }
      serialReceiveTotal = true;
      break;
    }
    if (serialReceiveByteCount > 30) {
      serialReceiveByteCount = 0;
      serialReceiveTotal = false;
      break;
    } else {
      serialReceiveByteCount+=1;
    }
  }
  //Update Device Status on Serial Receive
  if (serialReceiveComplete) {
    //read ETE
    restTemp[0] = serialReceiveArray[3];
    restTemp[1] = serialReceiveArray[4];
    restTemp[2] = serialReceiveArray[5];
    restTemp[3] = serialReceiveArray[6];
    ete_timer = atoi(restTemp);
    restTemp[0] = '0';
    restTemp[1] = serialReceiveArray[9];
    restTemp[2] = serialReceiveArray[10];
    restTemp[3] = serialReceiveArray[11];
    progress = atoi(restTemp);
    if (progress > 100) {
      beamerLEDOn = false;
    } else {
      beamerLEDOn = true;
    }
    restTemp[0] = '0';
    restTemp[1] = '0';
    restTemp[2] = serialReceiveArray[13];
    restTemp[3] = serialReceiveArray[14];
    temperatur = atoi(restTemp);
    statusCirculator = (serialReceiveArray[18] == '1') ? true : false;
    statusIntake = (serialReceiveArray[20] == '1') ? true : false;
    statusOuttake = (serialReceiveArray[22] == '1') ? true : false;
    statusHeater = (serialReceiveArray[24] == '1') ? true : false;
    serialReceiveComplete = false;
    serialReceiveByteCount = 0;
    digitalWrite(PIN_STATUS_LED, true);
  }


  //DISPLAY DETALS
  u8g2.setFont(u8g2_font_u8glib_4_tf);
  if (ani_statusHeater && statusHeater) { u8g2.drawGlyph(2, 9, 0x007e); }
  u8g2.setCursor(8, 7);
  u8g2.print(temperatur);
  u8g2.drawUTF8(16, 7, "°C");
  //intake indicator
  if (ani_statusIntake && statusIntake) { u8g2.drawGlyph(46, 7, 0x00bb); }
  u8g2.drawGlyph(50, 7, 0x007f); //maschine symbol
  if (ani_statusCirculator && statusCirculator) { u8g2.drawBox(51, 5, 1, 1); }
  if (!ani_statusCirculator && statusCirculator){ u8g2.drawBox(52, 5, 1, 1); }
  if (ani_statusOuttake && statusOuttake) {u8g2.drawGlyph(55, 7, 0x00bb); }

  //DISPLAY ETE
  u8g2.setFont(u8g2_font_crox4hb_tf);
  u8g2.setCursor(ALIGN_CENTER("00:00"), 24);
  int laufende_minute = 0;
  int laufende_sekunde = 0;
  laufende_minute = ete_timer/60;
  laufende_sekunde = ete_timer-(laufende_minute*60);
  sprintf(timeToChars, "%02d", laufende_minute);
  u8g2.print(timeToChars);
  u8g2.print(":");
  sprintf(timeToChars, "%02d", laufende_sekunde);
  u8g2.print(timeToChars);
  
  //DISPLAY PROGRESSBAR ----------------------
  //float percent = progress / 100;
  int PixelProgress = (int) (62 * ((float) progress / 100));
  u8g2.drawRFrame(0, 27, 64, 5, 1);
  u8g2.drawBox(1, 28, 1+PixelProgress, 3);
  if (ani_cursor && progress < 100) {
    u8g2.drawBox(2+PixelProgress, 28, 1, 3); //cursor
  }
  
  u8g2.sendBuffer();

  if (millis() > timer_last_exec + 1000) {
    timer_last_exec = millis();
    if (ete_timer > 0) { ete_timer--; }
    ani_counter=0;
  }

  //ANIMATION STEPPS
  if (ani_counter % 20 == 0) { //langsam
    ani_statusHeater = !ani_statusHeater;
    ani_cursor = !ani_cursor;
  }
  if (ani_counter % 10 == 0) {
    ani_statusCirculator = !ani_statusCirculator;
  }
  if (ani_counter % 4 == 0) { //schnell
    ani_statusIntake = !ani_statusIntake;
    ani_statusOuttake = !ani_statusOuttake;
  }

  delay(100);
  ani_counter++;
  digitalWrite(PIN_STATUS_LED, false);
  analogWrite(PIN_PWM_LED, beamerLEDOn ? 255 : 0);
}
