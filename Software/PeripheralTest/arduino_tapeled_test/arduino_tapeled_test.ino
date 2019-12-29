#include <Adafruit_NeoPixel.h>
#define MAX_VAL 255  // 0 to 255 for brightness
#define DELAY_TIME 10 
#define DELAY_TIME2 5

// Parameter 1 = number of pixels in strip
// Parameter 2 = pin number (most are valid)
// Parameter 3 = pixel type flags, add together as needed:
//   NEO_RGB     Pixels are wired for RGB bitstream
//   NEO_GRB     Pixels are wired for GRB bitstream
//   NEO_KHZ400  400 KHz bitstream (e.g. FLORA pixels)
//   NEO_KHZ800  800 KHz bitstream (e.g. High Density LED strip)
Adafruit_NeoPixel strip = Adafruit_NeoPixel(10, 6, NEO_GRB + NEO_KHZ800);

void setup() {
  pinMode(2, INPUT);
  strip.begin();
  strip.show(); // Initialize all pixels to 'off'

}

int colorSel = 0;

void loop() {
  if(digitalRead(2)==LOW){
    switch(colorSel){
      case 0:
            for(int i=0;i<4;i++){
      strip.setPixelColor(i, strip.Color(255,0,0));
        strip.show();
      }
      break;
      case 1:
            for(int i=0;i<4;i++){
      strip.setPixelColor(i, strip.Color(0,255,0));
        strip.show();
      }
      break;
      case 2:
            for(int i=0;i<4;i++){
      strip.setPixelColor(i, strip.Color(0,0,255));
        strip.show();
      }
      break;
    }
  } else {
          for(int i=0;i<4;i++){
      strip.setPixelColor(i, strip.Color(0,0,0));
        strip.show();
        if(colorSel == 2){
          colorSel=0;
        } else {
          colorSel++;
        }
      }
  }
  // Some example procedures showing how to display to the pixels:
//    colorWipe(strip.Color(0, 0, 0), DELAY_TIME); // Black
//  colorWipe(strip.Color(MAX_VAL, 0, 0), DELAY_TIME); // Red
//  colorWipe(strip.Color(0, MAX_VAL, 0), DELAY_TIME); // Green
//  colorWipe(strip.Color(MAX_VAL, MAX_VAL, 0), DELAY_TIME); // Yellow
//  colorWipe(strip.Color(0, 0, MAX_VAL), DELAY_TIME); // Blue
//  colorWipe(strip.Color(MAX_VAL, 0, MAX_VAL), DELAY_TIME); // Purple
//  colorWipe(strip.Color(0, MAX_VAL, MAX_VAL), DELAY_TIME); // Cyan
//  colorWipe(strip.Color(MAX_VAL, MAX_VAL, MAX_VAL), DELAY_TIME); // White
//  rainbow(DELAY_TIME2);
//  rainbowCycle(DELAY_TIME2);
}

// Fill the dots one after the other with a color
void colorWipe(uint32_t c, uint8_t wait) {
  for(uint16_t i=0; i<strip.numPixels(); i++) {
      strip.setPixelColor(i, c);
      strip.show();
      delay(wait);
  }
}

void rainbow(uint8_t wait) {
  uint16_t i, j;

  for(j=0; j<256; j++) {
    for(i=0; i<strip.numPixels(); i++) {
      strip.setPixelColor(i, Wheel((i+j) & 255));
    }
    strip.show();
    delay(wait);
  }
}

// Slightly different, this makes the rainbow equally distributed throughout
void rainbowCycle(uint8_t wait) {
  uint16_t i, j;

  for(j=0; j<256*5; j++) { // 5 cycles of all colors on wheel
    for(i=0; i< strip.numPixels(); i++) {
      strip.setPixelColor(i, Wheel(((i * 256 / strip.numPixels()) + j) & 255));
    }
    strip.show();
    delay(wait);
  }
}

// Input a value 0 to 255 to get a color value.
// The colours are a transition r - g - b - back to r.
uint32_t Wheel(byte WheelPos) {
  if(WheelPos < 85) {
   return strip.Color((WheelPos * 3)*MAX_VAL/255, (255 - WheelPos * 3)*MAX_VAL/255, 0);
  } else if(WheelPos < 170) {
   WheelPos -= 85;
   return strip.Color((255 - WheelPos * 3)*MAX_VAL/255, 0, (WheelPos * 3)*MAX_VAL/255);
  } else {
   WheelPos -= 170;
   return strip.Color(0, (WheelPos * 3)*MAX_VAL/255, (255 - WheelPos * 3)*MAX_VAL/255);
  }
}
