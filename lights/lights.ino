/****************************************************************************************
    Project:        NodeMCU_Lights
    Author:         Daniel Pardo
    Date:           06/09/2023
    Description:    This project uses the Blynk IoT platform and a NodeMCU to control a
                    string of WS2812b individually-addressable RGB LEDs.
****************************************************************************************/
#define FASTLED_ALLOW_INTERRUPTS 0
#define FASTLED_INTERRUPT_RETRY_COUNT 0
#define FASTLED_ESP8266_NODEMCU_PIN_ORDER

#include <FastLED.h>
#include <ESP8266WiFi.h>
#include <BlynkSimpleEsp8266.h>

#define BLYNK_TEMPLATE_ID   "TMPL29n1f2-uo"
#define BLYNK_TEMPLATE_NAME "Quickstart Template"
#define AUTH_TOKEN          "RGFdCQMxln2JxUaVE14IllFcOcrCCzK9"
#define BLYNK_PRINT         Serial

// WiFi credentials.
#define NETWORK_NAME        "Verizon_7HSJ9F"
#define NETWORK_PASSWORD    "gap-toddle6-ret"   // Set to "" for open networks.

//FastLED Initialization
#define DATA_PIN            D2            //D2 in code = D4 on board
#define LED_TYPE            WS2811
#define COLOR_ORDER         RGB
#define NUM_LEDS            200           // Change this to reflect the number of LEDs you have

//CRGB leds[NUM_LEDS];
CRGBArray<NUM_LEDS> leds;

// Overall twinkle speed.
// 0 (VERY slow) to 8 (VERY fast).  
#define TWINKLE_SPEED 4

// Overall twinkle density.
// 0 (NONE lit) to 8 (ALL lit at once).  
#define TWINKLE_DENSITY 5

CRGB gBackgroundColor = CRGB::Black; 

// If AUTO_SELECT_BACKGROUND_COLOR is set to 1,
// then for any palette where the first two entries 
// are the same, a dimmed version of that color will
// automatically be used as the background color.
#define AUTO_SELECT_BACKGROUND_COLOR 1

// If COOL_LIKE_INCANDESCENT is set to 1, colors will 
// fade out slighted 'reddened', similar to how
// incandescent bulbs change color as they get dim down.
#define COOL_LIKE_INCANDESCENT 1

//....................................................................................................................................
//Global variables
uint8_t lightsOn;
uint8_t brightness;
uint8_t redVal;
uint8_t greenVal;
uint8_t blueVal;
uint8_t paletteActive;
uint8_t twinkleSpeed;
uint8_t animation;
uint8_t temperature;
bool    whiteLight;

extern const TProgmemRGBGradientPalettePtr gGradientPalettes[];
extern const uint8_t gGradientPaletteCount;
uint8_t gCurrentPaletteNumber;
CRGBPalette16 gCurrentPalette( CRGB::Black);
CRGBPalette16 gTargetPalette( gGradientPalettes[0] );

/***********************************************************************
  Blynk Functions
***********************************************************************/
// Sync all variables with the cloud.
BLYNK_CONNECTED()
{
  Blynk.syncAll();
  Serial.print("Synced");
}

// Turn lights on/off.
BLYNK_WRITE(V0)
{
  lightsOn = param.asInt();
}

// Set brightness.
BLYNK_WRITE(V1)
{
  brightness = param.asInt();
}

// Set single color.
BLYNK_WRITE(V2)
{
  redVal = param[0].asInt();
  greenVal = param[1].asInt();
  blueVal = param[2].asInt();
  if ((redVal == 255) && (greenVal == 255) && (blueVal == 255))
  {
    whiteLight = true;
  }
  else
  {
    whiteLight = false;
  }
}

// Set palette.
BLYNK_WRITE(V3)
{
  gCurrentPaletteNumber = param.asInt();
}

// Set animation.
BLYNK_WRITE(V4)
{
  animation = param.asInt();
}

// Decide between solid color or palette.
BLYNK_WRITE(V5)
{
  paletteActive = param.asInt();  
}

// Set twinkle speed.
BLYNK_WRITE(V6)
{
  twinkleSpeed = param.asInt();
}

// Set temperature for white light.
BLYNK_WRITE(V7)
{
  temperature = param.asInt();
}

//.......................................................................................................................................
//Twinkle animation functions
//  This function loops over each pixel, calculates the 
//  adjusted 'clock' that this pixel should use, and calls 
//  "CalculateOneTwinkle" on each pixel.  It then displays
//  either the twinkle color of the background color, 
//  whichever is brighter.
void drawTwinkles( CRGBSet& L)
{
  // "PRNG16" is the pseudorandom number generator
  // It MUST be reset to the same starting value each time
  // this function is called, so that the sequence of 'random'
  // numbers that it generates is (paradoxically) stable.
  uint16_t PRNG16 = 11337;
  
  uint32_t clock32 = millis();

  // Set up the background color, "bg".
  CRGB bg;
  if( (AUTO_SELECT_BACKGROUND_COLOR == 1) &&
      (gCurrentPalette[0] == gCurrentPalette[1] )) {
    bg = gCurrentPalette[0];
    uint8_t bglight = bg.getAverageLight();
    if( bglight > 64) {
      bg.nscale8_video( 16); // very bright, so scale to 1/16th
    } else if( bglight > 16) {
      bg.nscale8_video( 64); // not that bright, so scale to 1/4th
    } else {
      bg.nscale8_video( 86); // dim, scale to 1/3rd.
    }
  } else {
    bg = gBackgroundColor; // just use the explicitly defined background color
  }

  uint8_t backgroundbrightness = bg.getAverageLight();
  
  for( CRGB& pixel: L) {
    PRNG16 = (uint16_t)(PRNG16 * 2053) + 1384; // next 'random' number
    uint16_t myclockoffset16= PRNG16; // use that number as clock offset
    PRNG16 = (uint16_t)(PRNG16 * 2053) + 1384; // next 'random' number
    // use that number as clock speed adjustment factor (in 8ths, from 8/8ths to 23/8ths)
    uint8_t myspeedmultiplierQ5_3 =  ((((PRNG16 & 0xFF)>>4) + (PRNG16 & 0x0F)) & 0x0F) + 0x08;
    uint32_t myclock30 = (uint32_t)((clock32 * myspeedmultiplierQ5_3) >> 3) + myclockoffset16;
    uint8_t  myunique8 = PRNG16 >> 8; // get 'salt' value for this pixel

    // We now have the adjusted 'clock' for this pixel, now we call
    // the function that computes what color the pixel should be based
    // on the "brightness = f( time )" idea.
    CRGB c = computeOneTwinkle( myclock30, myunique8);

    uint8_t cbright = c.getAverageLight();
    int16_t deltabright = cbright - backgroundbrightness;
    if( deltabright >= 32 || (!bg)) {
      // If the new pixel is significantly brighter than the background color, 
      // use the new color.
      pixel = c;
    } else if( deltabright > 0 ) {
      // If the new pixel is just slightly brighter than the background color,
      // mix a blend of the new color and the background color
      pixel = blend( bg, c, deltabright * 8);
    } else { 
      // if the new pixel is not at all brighter than the background color,
      // just use the background color.
      pixel = bg;
    }
  }
}

//  This function takes a time in pseudo-milliseconds,
//  figures out brightness = f( time ), and also hue = f( time )
//  The 'low digits' of the millisecond time are used as 
//  input to the brightness wave function.  
//  The 'high digits' are used to select a color, so that the color
//  does not change over the course of the fade-in, fade-out
//  of one cycle of the brightness wave function.
//  The 'high digits' are also used to determine whether this pixel
//  should light at all during this cycle, based on the TWINKLE_DENSITY.
CRGB computeOneTwinkle( uint32_t ms, uint8_t salt)
{
  uint16_t ticks = ms >> (8-twinkleSpeed);
  uint8_t fastcycle8 = ticks;
  uint16_t slowcycle16 = (ticks >> 8) + salt;
  slowcycle16 += sin8( slowcycle16);
  slowcycle16 =  (slowcycle16 * 2053) + 1384;
  uint8_t slowcycle8 = (slowcycle16 & 0xFF) + (slowcycle16 >> 8);
  
  uint8_t bright = 0;
  if( ((slowcycle8 & 0x0E)/2) < TWINKLE_DENSITY) {
    bright = attackDecayWave8( fastcycle8);
  }

  uint8_t hue = slowcycle8 - salt;
  CRGB c;
  if( bright > 0) {
    c = ColorFromPalette( gCurrentPalette, hue, bright, NOBLEND);
    if( COOL_LIKE_INCANDESCENT == 1 ) {
      coolLikeIncandescent( c, fastcycle8);
    }
  } else {
    c = CRGB::Black;
  }
  return c;
}

uint8_t attackDecayWave8( uint8_t i)
{
  if( i < 86) {
    return i * 3;
  } else {
    i -= 86;
    return 255 - (i + (i/2));
  }
}

void coolLikeIncandescent( CRGB& c, uint8_t phase)
{
  if( phase < 128) return;

  uint8_t cooling = (phase - 128) >> 4;
  c.g = qsub8( c.g, cooling);
  c.b = qsub8( c.b, cooling * 2);
}

//...................................................................................................................................................
//Scroll animation function
void scroll( CRGB* ledarray, uint16_t numleds, const CRGBPalette16& gCurrentPalette)
{
  static uint8_t startindex = 0;
  startindex--;
  fill_palette( ledarray, numleds, startindex, (256 / NUM_LEDS) + 1, gCurrentPalette, brightness, LINEARBLEND);
  //FastLED.delay(20);
  FastLED.delay(41-5*twinkleSpeed);
}

//..................................................................................................................................................
// Gradient Color Palette definitions

// Gradient palette "retro2_16_gp", originally from
// http://soliton.vm.bytemark.co.uk/pub/cpt-city/ma/retro2/tn/retro2_16.png.index.html
DEFINE_GRADIENT_PALETTE( retro2_16_gp ) {
    0, 188,135,  1,
  255,  46,  7,  1};

// Gradient palette "es_pinksplash_07_gp", originally from
// http://soliton.vm.bytemark.co.uk/pub/cpt-city/es/pink_splash/tn/es_pinksplash_07.png.index.html
DEFINE_GRADIENT_PALETTE( es_pinksplash_07_gp ) {
    0, 229,  1,  1,
   61, 242,  4, 63,
  101, 255, 12,255,
  127, 249, 81,252,
  153, 255, 11,235,
  193, 244,  5, 68,
  255, 232,  1,  5};

// Gradient palette "es_ocean_breeze_068_gp", originally from
// http://soliton.vm.bytemark.co.uk/pub/cpt-city/es/ocean_breeze/tn/es_ocean_breeze_068.png.index.html
DEFINE_GRADIENT_PALETTE( es_ocean_breeze_068_gp ) {
    0, 100,156,153,
   51,   1, 99,137,
  101,   1, 68, 84,
  104,  35,142,168,
  178,   0, 63,117,
  255,   1, 10, 10};

// Gradient palette "departure_gp", originally from
// http://soliton.vm.bytemark.co.uk/pub/cpt-city/mjf/tn/departure.png.index.html
DEFINE_GRADIENT_PALETTE( departure_gp ) {
    0,   8,  3,  0,
   42,  23,  7,  0,
   63,  75, 38,  6,
   84, 169, 99, 38,
  106, 213,169,119,
  116, 255,255,255,
  138, 135,255,138,
  148,  22,255, 24,
  170,   0,255,  0,
  191,   0,136,  0,
  212,   0, 55,  0,
  255,   0, 55,  0};

// Gradient palette "es_landscape_64_gp", originally from
// http://soliton.vm.bytemark.co.uk/pub/cpt-city/es/landscape/tn/es_landscape_64.png.index.html
DEFINE_GRADIENT_PALETTE( es_landscape_64_gp ) {
    0,   0,  0,  0,
   37,   2, 25,  1,
   76,  15,115,  5,
  127,  79,213,  1,
  128, 126,211, 47,
  130, 188,209,247,
  153, 144,182,205,
  204,  59,117,250,
  255,   1, 37,192};

// Gradient palette "es_landscape_33_gp", originally from
// http://soliton.vm.bytemark.co.uk/pub/cpt-city/es/landscape/tn/es_landscape_33.png.index.html
DEFINE_GRADIENT_PALETTE( es_landscape_33_gp ) {
    0,   1,  5,  0,
   19,  32, 23,  1,
   38, 161, 55,  1,
   63, 229,144,  1,
   66,  39,142, 74,
  255,   1,  4,  1};

// Gradient palette "gr65_hult_gp", originally from
// http://soliton.vm.bytemark.co.uk/pub/cpt-city/hult/tn/gr65_hult.png.index.html
DEFINE_GRADIENT_PALETTE( gr65_hult_gp ) {
    0, 247,176,247,
   48, 255,136,255,
   89, 220, 29,226,
  160,   7, 82,178,
  216,   1,124,109,
  255,   1,124,109};

// Gradient palette "GMT_drywet_gp", originally from
// http://soliton.vm.bytemark.co.uk/pub/cpt-city/gmt/tn/GMT_drywet.png.index.html
DEFINE_GRADIENT_PALETTE( GMT_drywet_gp ) {
    0,  47, 30,  2,
   42, 213,147, 24,
   84, 103,219, 52,
  127,   3,219,207,
  170,   1, 48,214,
  212,   1,  1,111,
  255,   1,  7, 33};

// Gradient palette "ib15_gp", originally from
// http://soliton.vm.bytemark.co.uk/pub/cpt-city/ing/general/tn/ib15.png.index.html
DEFINE_GRADIENT_PALETTE( ib15_gp ) {
    0, 113, 91,147,
   72, 157, 88, 78,
   89, 208, 85, 33,
  107, 255, 29, 11,
  141, 137, 31, 39,
  255,  59, 33, 89};

// Gradient palette "es_emerald_dragon_08_gp", originally from
// http://soliton.vm.bytemark.co.uk/pub/cpt-city/es/emerald_dragon/tn/es_emerald_dragon_08.png.index.html
DEFINE_GRADIENT_PALETTE( es_emerald_dragon_08_gp ) {
    0,  97,255,  1,
  101,  47,133,  1,
  178,  13, 43,  1,
  255,   2, 10,  1};

// Gradient palette "fire_gp", originally from
// http://soliton.vm.bytemark.co.uk/pub/cpt-city/neota/elem/tn/fire.png.index.html
DEFINE_GRADIENT_PALETTE( fire_gp ) {
    0,   1,  1,  0,
   76,  32,  5,  0,
  146, 192, 24,  0,
  197, 220,105,  5,
  240, 252,255, 31,
  250, 252,255,111,
  255, 255,255,255};

// Gradient palette "Colorfull_gp", originally from
// http://soliton.vm.bytemark.co.uk/pub/cpt-city/nd/atmospheric/tn/Colorfull.png.index.html
DEFINE_GRADIENT_PALETTE( Colorfull_gp ) {
    0,  10, 85,  5,
   25,  29,109, 18,
   60,  59,138, 42,
   93,  83, 99, 52,
  106, 110, 66, 64,
  109, 123, 49, 65,
  113, 139, 35, 66,
  116, 192,117, 98,
  124, 255,255,137,
  168, 100,180,155,
  255,  22,121,174};

// Gradient palette "Sunset_Real_gp", originally from
// http://soliton.vm.bytemark.co.uk/pub/cpt-city/nd/atmospheric/tn/Sunset_Real.png.index.html
DEFINE_GRADIENT_PALETTE( Sunset_Real_gp ) {
    0, 120,  0,  0,
   22, 179, 22,  0,
   51, 255,104,  0,
   85, 167, 22, 18,
  135, 100,  0,103,
  198,  16,  0,130,
  255,   0,  0,160};

//Gradient palette "BlacK_Red_Magenta_Yellow_gp"
DEFINE_GRADIENT_PALETTE( BlacK_Red_Magenta_Yellow_gp ) {
    0,   0,  0,  0,
   42,  42,  0,  0,
   84, 255,  0,  0,
  127, 255,  0, 45,
  170, 255,  0,255,
  212, 255, 55, 45,
  255, 255,255,  0};

// Gradient palette "Blue_Cyan_Yellow_gp", originally from
// http://soliton.vm.bytemark.co.uk/pub/cpt-city/nd/basic/tn/Blue_Cyan_Yellow.png.index.html
DEFINE_GRADIENT_PALETTE( Blue_Cyan_Yellow_gp ) {
    0,   0,  0,255,
   63,   0, 55,255,
  127,   0,255,255,
  191,  42,255, 45,
  255, 255,255,  0};

// Gradient palette "moon_gp", originally from
// http://soliton.vm.bytemark.co.uk/pub/cpt-city/pn/tn/moon.png.index.html
DEFINE_GRADIENT_PALETTE( moon_gp ) {
    0,  53, 99,145,
    3,  54,100,147,
    9,  57,103,149,
   14,  59,105,151,
   20,  64,108,151,
   26,  66,111,153,
   31,  68,114,155,
   37,  72,115,156,
   43,  74,118,156,
   48,  78,121,158,
   54,  82,124,158,
   59,  84,127,160,
   65,  88,130,160,
   71,  91,133,162,
   76,  94,136,162,
   82,  97,138,164,
   88, 100,141,164,
   93, 104,144,166,
   99, 107,147,166,
  104, 112,149,168,
  110, 115,152,168,
  116, 120,156,170,
  121, 123,159,172,
  127, 128,162,174,
  133, 133,168,174,
  138, 139,171,176,
  144, 142,175,178,
  149, 146,178,180,
  155, 150,182,182,
  161, 153,186,184,
  166, 159,187,184,
  172, 163,193,186,
  177, 169,195,186,
  183, 173,199,188,
  189, 179,203,188,
  194, 186,207,190,
  200, 190,211,190,
  206, 197,217,192,
  211, 203,221,192,
  217, 210,225,194,
  222, 215,229,197,
  228, 222,233,197,
  234, 227,237,199,
  239, 232,239,201,
  245, 237,241,203,
  251, 242,246,205,
  255, 244,248,205};

// Gradient palette "Sandy_Scars_Aus_gp", originally from
// http://soliton.vm.bytemark.co.uk/pub/cpt-city/colo/evad/tn/Sandy_Scars_Aus.png.index.html
DEFINE_GRADIENT_PALETTE( Sandy_Scars_Aus_gp ) {
    0, 242,156, 46,
   51, 242,156, 46,
   51, 215, 61, 32,
  102, 215, 61, 32,
  102, 159, 40, 31,
  153, 159, 40, 31,
  153,  64,  3,  8,
  204,  64,  3,  8,
  204,  26,  5, 23,
  255,  26,  5, 23};
  
// Gradient palette "autumnrose_gp", originally from
// http://soliton.vm.bytemark.co.uk/pub/cpt-city/rc/tn/autumnrose.png.index.html
// converted for FastLED with gammas (2.6, 2.2, 2.5)
// Size: 32 bytes of program space.

DEFINE_GRADIENT_PALETTE( autumnrose_gp ) {
    0,  71,  3,  1,
   45, 128,  5,  2,
   84, 186, 11,  3,
  127, 215, 27,  8,
  153, 224, 69, 13,
  188, 229, 84,  6,
  226, 242,135, 17,
  255, 247,161, 79};

// Gradient palette "bhw1_05_gp", originally from
// http://soliton.vm.bytemark.co.uk/pub/cpt-city/bhw/bhw1/tn/bhw1_05.png.index.html
// converted for FastLED with gammas (2.6, 2.2, 2.5)
// Size: 8 bytes of program space.

DEFINE_GRADIENT_PALETTE( bhw1_05_gp ) {
    0,   1,221, 53,
  255,  73,  3,178};
  
//..............................................................................................................................................  
// This list of color palettes acts as a "playlist"; you can add or delete, or re-arrange as you wish.
const TProgmemRGBGradientPalettePtr gGradientPalettes[] = {
  Rainbow_gp,                       //you get it                GOOD
  moon_gp,                          //white                     OK
  es_ocean_breeze_068_gp,           //white-teal                OK
  gr65_hult_gp,                     //purple-white-teal-blue    OK
  bhw1_05_gp,                       //teal-purple
  Colorfull_gp,                     //blue-white-pink-green     OK
  es_landscape_64_gp,               //blue-white-green          GOOD
  Blue_Cyan_Yellow_gp,               //yellow-green-teal-blue    GOOD
  GMT_drywet_gp,                    //blue-teal-green-orange    GOOD
  es_landscape_33_gp,               //teal-orange-black         OK
  es_emerald_dragon_08_gp,          //green-black               OK
  departure_gp,                     //green-white-brown-black   OK
  BlacK_Red_Magenta_Yellow_gp,      //yellow-purple-pink-black  GOOD
  retro2_16_gp,                     //orange-red                OK
  autumnrose_gp,                    //orange-red
  es_pinksplash_07_gp,              //red-pink-lightpink        OK
  Sandy_Scars_Aus_gp,               //peach                     OK
  ib15_gp,                          //pink-orange-lightpurple   OK
  Sunset_Real_gp,                   //orange-red-purple-blue    GOOD
  fire_gp                          //white-yellow-red-black    GOOD
  };

// Count of how many cpt-city gradients are defined:
const uint8_t gGradientPaletteCount = sizeof( gGradientPalettes) / sizeof( TProgmemRGBGradientPalettePtr );


void showLEDs() 
{
  if (lightsOn == 0)                    //Lights are off
  {
    FastLED.setBrightness(0);
  }
  else                                  //Lights are on
  {
    FastLED.setBrightness(brightness);
    
    if (paletteActive)
    {
      nblendPaletteTowardPalette( gCurrentPalette, gTargetPalette, 200);
      gTargetPalette = gGradientPalettes[gCurrentPaletteNumber];
      if (animation)
      {
        drawTwinkles(leds);
      }
      else
      {
        scroll(leds, NUM_LEDS, gCurrentPalette);
      }  
    }
    
    else
    {
      if (whiteLight)
      {
        redVal = 255;
        greenVal = 255 - temperature/3;
        blueVal = 255 - temperature;
      }
      fill_solid(leds, NUM_LEDS, CRGB(redVal, greenVal, blueVal));
    }
  }
  
  FastLED.show();
}

/*****************************************************************************************************
 * Main Program
 ****************************************************************************************************/
void setup()
{
  delay(3000);            //Sanity for FastLED
  Serial.begin(115200);   //Debug console
  Serial.println("Starting...");
  
  Blynk.begin(AUTH_TOKEN, NETWORK_NAME, NETWORK_PASSWORD);
  
  FastLED.addLeds<LED_TYPE,DATA_PIN, COLOR_ORDER>(leds, NUM_LEDS).setCorrection(TypicalLEDStrip);
  FastLED.setBrightness(brightness);
}

void loop()
{
  Blynk.run();      //DON'T USE DELAY()
  showLEDs();
}
