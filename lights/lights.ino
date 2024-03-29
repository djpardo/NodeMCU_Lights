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
#include "ColorPalettes.h"
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

// Overall twinkle density.
// 0 (NONE lit) to 8 (ALL lit at once).  
#define TWINKLE_DENSITY 5

// If AUTO_SELECT_BACKGROUND_COLOR is set to 1,
// then for any palette where the first two entries 
// are the same, a dimmed version of that color will
// automatically be used as the background color.
#define AUTO_SELECT_BACKGROUND_COLOR 1

// If COOL_LIKE_INCANDESCENT is set to 1, colors will 
// fade out slighted 'reddened', similar to how
// incandescent bulbs change color as they get dim down.
#define COOL_LIKE_INCANDESCENT 1

/*******************************************************************
 * Global Variables
 ******************************************************************/
uint8_t lightsOn;
uint8_t brightness;
uint8_t redVal;
uint8_t greenVal;
uint8_t blueVal;
uint8_t paletteActive;
uint8_t animationSpeed;
uint8_t animation;
uint8_t temperature;
bool    whiteLight;

CRGBArray<NUM_LEDS> leds;

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

uint8_t gCurrentPaletteNumber;
CRGBPalette16 gCurrentPalette( CRGB::Black);
CRGBPalette16 gTargetPalette( gGradientPalettes[0] );
CRGB gBackgroundColor = CRGB::Black; 
const uint8_t gGradientPaletteCount = sizeof( gGradientPalettes) / sizeof( TProgmemRGBGradientPalettePtr );

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
  animationSpeed = param.asInt();
}

// Set temperature for white light.
BLYNK_WRITE(V7)
{
  temperature = param.asInt();
}

/******************************************************************
 * Twinkle Animation Functions
 *****************************************************************/
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
  uint16_t ticks = ms >> (8-animationSpeed);
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

/*****************************************************************************
 * Scroll Animation
 ****************************************************************************/
void scroll( CRGB* ledarray, uint16_t numleds, const CRGBPalette16& gCurrentPalette)
{
  static uint8_t startindex = 0;
  startindex--;
  fill_palette( ledarray, numleds, startindex, (256 / NUM_LEDS) + 1, gCurrentPalette, brightness, LINEARBLEND);
  //FastLED.delay(20);
  FastLED.delay(41-5*animationSpeed);
}

/*****************************************************************************  
 *   Main LED Logic
 ****************************************************************************/
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
