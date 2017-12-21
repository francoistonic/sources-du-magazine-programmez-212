/// 
/// Wiring
/// ------
/// 
/// ESP8266 port | SI4703 signal | RDA5807M signal
/// :----------: | :-----------: | :-------------:
/// GND          | GND           | GND   
/// 3.3V         | VCC           | VCC
/// D1           | SCLK          | SCLK
/// D2           | SDIO          | SDIO
/// D5           | RST           | -
///
/// D3           | + button (Pull-up)
/// D4           | Menu button (Pull-up + LED)
/// D8           | - button (Pull-down)
///

#include <Wire.h>

// OLED Display Lib
// To support 64x48 be sure to use alternative https://github.com/mcauser/Adafruit_SSD1306.git
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
Adafruit_SSD1306 display(0);

// Radio Lib
#include <radio.h>
// #include <RDA5807M.h>
#include <SI4703.h>
//#include <SI4705.h>
//#include <TEA5767.h>

#include <RDSParser.h>

/// uncomment the right radio object definition.
// RADIO radio;       ///< Create an instance of a non functional radio.
// RDA5807M radio;    ///< Create an instance of a RDA5807 chip radio
SI4703   radio;    ///< Create an instance of a SI4703 chip radio.
// SI4705   radio;    ///< Create an instance of a SI4705 chip radio.
// TEA5767  radio;    ///< Create an instance of a TEA5767 chip radio.

/// RDS parser
RDSParser rds;
char rdsServiceName[9];
char rdsTxt[66];
uint8_t rdsHour = 0; 
uint8_t rdsMinute = 0;

void RDS_process(uint16_t block1, uint16_t block2, uint16_t block3, uint16_t block4) {
  rds.processData(block1, block2, block3, block4);
}

/// Update the ServiceName text on the LCD display.
void DisplayServiceName(char *name)
{
  strcpy(rdsServiceName, name);
}

void DisplayText(char *txt)
{
  strcpy(rdsTxt, txt);
} 

void DisplayTime(uint8_t hour, uint8_t minute)
{
  rdsHour = hour;
  rdsMinute = minute;
} 

void clearRds() { // Clear RDS Service Name and Text
  memset(rdsServiceName, 0, sizeof(rdsServiceName));
  memset(rdsTxt, 0, sizeof(rdsTxt));
}

// menuMode
int menuMode = 0; // 0 = Seek, 1 = Volume

// - - - - - - - - - - - - - - - - - - - - - - - - - -

void refreshOled() {
  RADIO_INFO radioInfo;
  RADIO_FREQ freq = radio.getFrequency();
  char freqStr[12]; // Frequency string
  
  unsigned long nowTime = millis();
  static unsigned long lastTime;
    
  static int index = 0;
  static int pauseText = 3; // pause scrooling when starting message

  char printBuffer[50];
  int lenghtString;

  if (nowTime < 100) lastTime = nowTime; // we wrapped around, lets just try again
  if ((lastTime + 150) > nowTime) return ; // refresh rate and scrolling speed
  lastTime = nowTime;


  radio.formatFrequency(freqStr, sizeof(freqStr));
  radio.getRadioInfo(&radioInfo);

  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(WHITE);
  display.setCursor(0,0);    
  display.println(freqStr);   
  display.println("\n " + String(rdsServiceName));
  
  // Scroll RDS Text on 1 line
  if (rdsTxt[index] == 0) {
    index = 0;
    pauseText = 3;
  }
  memset(printBuffer, 0, 50);
  strncpy(printBuffer, rdsTxt+index, 10);    
  display.println(printBuffer);
  if (pauseText == 0) index++;
  else pauseText--;


  switch(menuMode) {
    case 0: // seek Mode - Display clock
      sprintf(printBuffer, "\n   %02d:%02d", rdsHour, rdsMinute);
      display.println(printBuffer);
      break;
    case 1: // Volume
      display.println("\nVolume: " + String(radio.getVolume()));
      break;
  }  
  display.display();      
}

// Buttons management
#define buttonMode D4 // pull-up (+ LED on D1 Mini)
#define buttonUp   D3 // pull-up
#define buttonDown D8 // pull-Down

#define commandNone 0
#define commandUp   1
#define commandDown 2
#define commandMode 3

void checkCommand() {
  static int lastCommand = -1;
  int command = 0;
  
  if (!digitalRead(buttonUp)) command = commandUp; // pull-up
  if (digitalRead(buttonDown)) command = commandDown; // pull-down
  if (!digitalRead(buttonMode)) command = commandMode; // pull-up
  
  if (command == lastCommand) return; // Button not realeased
  lastCommand = command;
  
  switch(menuMode) {
    case 0: // seek
      if (command == commandUp) { 
        radio.seekUp();
        clearRds();
      }
      if (command == commandDown) {
        radio.seekDown();
        clearRds();
      }
      break;
    case 1: // Volume
      {
        int v = radio.getVolume();
        if (command == commandUp && v < 15) radio.setVolume(++v);
        if (command == commandDown && v > 0) radio.setVolume(--v);
        break;
      }      
  }
  if (command == commandMode) menuMode++;
  if (menuMode > 1) menuMode = 0;
} 

// - - - - - - - - - - - - - - - - - - - - - - - - - -

void setup() {
  // Open the Serial port is case of debug
  Serial.begin(115200);

  // Initialize buttons
  pinMode(buttonMode, INPUT);
  pinMode(buttonUp, INPUT);
  pinMode(buttonDown, INPUT);

  // Initialize OLED SSD1306
  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);  // initialize with the I2C addr 0x3C (for the 64x48)
  display.display();  

  // Initialize Radio 
  radio.init();
  // Enable information to the Serial port
  // radio.debugEnable();

  radio.setBandFrequency(RADIO_BAND_FM, 9690); // 96.9Mhz Voltage
  radio.setMono(false);
  radio.setMute(false);
  radio.setVolume(12);

  radio.attachReceiveRDS(RDS_process);
  rds.attachServicenNameCallback(DisplayServiceName);
  rds.attachTextCallback(DisplayText);
  rds.attachTimeCallback(DisplayTime);
} 

void loop() {
  radio.checkRDS();
  refreshOled();
  checkCommand();
} 
