#include <Wire.h>
#include <TM1637Display.h>
#include <Adafruit_NeoPixel.h>

#define CLK 2                 // TM1637 CLK pin
#define DIO 3                 // TM1637 DIO pin
#define BRIGHTNESS_7SEG 7     // 7セグの輝度(0~7)

#define NUM_PIXELS 10         // 接続している NeoPixel の数
#define DATA_PIN 6            // 通信に使う PIN
#define BRIGHTNESS_NEO 50    // Neopixcel輝度

TM1637Display TIME_7SEG(CLK, DIO);

Adafruit_NeoPixel HP_GAUGE = Adafruit_NeoPixel(NUM_PIXELS, DATA_PIN, NEO_GRB + NEO_KHZ800);


void receiveEvent();

void Opening();
void I2CERROR();

void NOMAL_MODE(int HP, int timeH, int timeL);
void COUNTDOWN_MODE(int HP, int timeL);
void FINISH_MODE();
void RESET_MODE();
void ZERO_MODE();

void HP_DSP(int hp);

void HP_SET(char color[10]);
void GREEN(int num);
void YELLOW(int num);
void RED(int num);
void NONE(int num);




int TIME  = 0;
int HP    = 0;


void setup(){

  Opening();

//Serial.begin(9600);
  Wire.begin(0x1E);
  Wire.onReceive(receiveEvent);
}

//I2Cマスタからデータ送信時に実行
void receiveEvent(int Bytes){

  int event;
  int TIME, timeH, timeL;
  int HP;
/*
  while(Wire.available() != 4){
    I2CERROR();
  }
*/
  while(Wire.available()){

    event  = Wire.read();   //モード選択バイト
    HP     = Wire.read();   //HP
    timeH  = Wire.read();   //整数
    timeL  = Wire.read();   //小数点以下

    switch(event){
      case 0 : 
        NOMAL_MODE(HP, timeH, timeL);
        break;

      case 1 : 
        COUNTDOWN_MODE(HP, timeL);
        break;

      case 2 : 
        FINISH_MODE();
        break;

      case 3 : 
        RESET_MODE();
        break;

      default : 
        ZERO_MODE();
        break;
    }

  //  Serial.println(HP);
  }  

}



void loop(){

}


//電源投入時デモ
void Opening(){

  delay(500);

  HP_GAUGE.begin();
  HP_GAUGE.show();

  byte ESC[] = { 0,SEG_A | SEG_D | SEG_E | SEG_F | SEG_G , SEG_A | SEG_C | SEG_D | SEG_F | SEG_G , SEG_A | SEG_D | SEG_E | SEG_F };

  //7セグ輝度変更デモ
  for(int i=0; i<BRIGHTNESS_7SEG; i++){

    TIME_7SEG.setSegments(ESC);
    TIME_7SEG.setBrightness(i);
    if(i <= 1){
      HP_DSP(1);
    }
    else if(i>1 && i<=2){
      HP_DSP(2);
    }
    else if(i>2 && i<=3){
      HP_DSP(3);
    }
    else if(i>3 && i<=4){
      HP_DSP(4);
    }
    else if(i>4){
      HP_DSP(5);
    }
    delay(400);
  }

  //Serial.println("ready");
  delay(100);
}

//I2Cエラー(対応したデータと異なるバイト数のデータが送られてきたときエラー表示します。)
void I2CERROR(){
byte Err[] = { 0, SEG_A | SEG_D | SEG_E | SEG_F | SEG_G , SEG_E | SEG_G , SEG_E | SEG_G };
byte ZERO[] = {0, 0, 0, 0};

  TIME_7SEG.setSegments(ZERO);
  HP_SET("OYOYOYOYOY"); 
  delay(200);
  TIME_7SEG.setSegments(Err);
  TIME_7SEG.setBrightness(BRIGHTNESS_7SEG);
  HP_SET("RORORORORO");
  delay(200);
}

void NOMAL_MODE(int HP, int timeH, int timeL){

  int TIME;

  TIME   = (timeH*10)+timeL; 

  TIME_7SEG.showNumberDecEx(TIME,0x20,true);

  HP_DSP(HP);

}

void COUNTDOWN_MODE(int HP, int timeL){

  byte GO[] = { 0,0, SEG_A | SEG_C | SEG_D | SEG_E | SEG_F , SEG_A | SEG_B | SEG_C | SEG_D | SEG_E | SEG_F };
  uint8_t num[4] = {};

  HP_DSP(HP);

  int count;

  if(timeL == 0xFF){

    //HP_SET("OOOOOOOOOO");

    TIME_7SEG.setSegments(GO);
    TIME_7SEG.setBrightness(BRIGHTNESS_7SEG);

  } 
  else if(timeL>=0 && timeL<=9){

    num[2] = TIME_7SEG.encodeDigit(timeL);

    TIME_7SEG.setSegments(num);
    TIME_7SEG.setBrightness(BRIGHTNESS_7SEG);
      
  }

}

void FINISH_MODE(){

  byte END[] = {0,SEG_A | SEG_D | SEG_E | SEG_F | SEG_G , SEG_C | SEG_E | SEG_G , SEG_B | SEG_C | SEG_D | SEG_E | SEG_G};


  TIME_7SEG.setSegments(END);
  TIME_7SEG.setBrightness(BRIGHTNESS_7SEG);

}

void RESET_MODE(){
  byte ESC[] = { 0,SEG_A | SEG_D | SEG_E | SEG_F | SEG_G , SEG_A | SEG_C | SEG_D | SEG_F | SEG_G , SEG_A | SEG_D | SEG_E | SEG_F };

  TIME_7SEG.setSegments(ESC);
  TIME_7SEG.setBrightness(2);
  HP_SET("OOOOOOOOOO");

}

void ZERO_MODE(){

  byte zero[] = {0,0,0,0};

  TIME_7SEG.setSegments(zero);
  TIME_7SEG.setBrightness(0);
  HP_SET("OOOOOOOOOO");

}

//HPゲージ表示
void HP_DSP(int hp){

  switch (hp) {
      case 5 :
        HP_SET("GGGGGGGGGG");
        break;

      case 4 :
        HP_SET("OOGGGGGGGG");
        break;

      case 3 :
        HP_SET("OOOOGGGGGG");
        break;

      case 2 :
        HP_SET("OOOOOOYYYY");
        break;

      case 1 :
        HP_SET("OOOOOOOORR");
        break;

      default :
        HP_SET("OOOOOOOOOO");
        break;
  }
}

//10文字配列でLEDそれぞれの色を指定。 G-緑、Y-黄、R-赤、O-無
void HP_SET(char color[10]){
  for(int i=0; i<10; i++){
      if(color[i] == 'G'){
        GREEN(i);
      }
      else if(color[i] == 'Y'){
        YELLOW(i);
      }
      else if(color[i] == 'R'){
        RED(i);
      }
      else if(color[i] == 'O'){
        NONE(i);
      }
    }
    HP_GAUGE.setBrightness(BRIGHTNESS_NEO);
    HP_GAUGE.show();  
}

void GREEN(int num){
  HP_GAUGE.setPixelColor(num,   0, 255,  20);
}

void YELLOW(int num){
  HP_GAUGE.setPixelColor(num, 255, 255,   0);
}

void RED(int num){
  HP_GAUGE.setPixelColor(num, 255,   0,   0);
}

void NONE(int num){
  HP_GAUGE.setPixelColor(num,  0,  0,  0);
}
