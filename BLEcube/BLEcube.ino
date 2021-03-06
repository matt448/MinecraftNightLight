///////////////////////////////////////////////
// Minecraft BLE Night Light
//
//
// Written by Matthew McMillan
//            @matthewmcmillan
//            https://matthewcmcmillan.blogspot.com
//            https://github.com/matt448/MinecraftNightLight
//
// Most of the BLE code comes from Adafruit's nRF8001 examples


/*********************************************************************
This is an example for our nRF8001 Bluetooth Low Energy Breakout

  Pick one up today in the adafruit shop!
  ------> http://www.adafruit.com/products/1697

Adafruit invests time and resources providing this open source code, 
please support Adafruit and open-source hardware by purchasing 
products from Adafruit!

Written by Kevin Townsend/KTOWN  for Adafruit Industries.
MIT license, check LICENSE for more information
All text above, and the splash screen below must be included in any redistribution
*********************************************************************/

// This version uses the internal data queing so you can treat it like Serial (kinda)!

#include <SPI.h>
#include "Adafruit_BLE_UART.h"
#include <Adafruit_NeoPixel.h>
#include <EEPROM.h>

//NeoPixel locations
int top[5] = {3, 4, 5, 12, 13};
int back[3] = {0, 1, 2};
int front[3] = {6, 7, 8};
int right[3] = {9, 10, 11};
int left[3] = {14, 15, 16};
//fullString allows rainbow animations to cycle in order based on cube sides
int fullString[17] = {0, 1, 2, 3, 4, 5, 12, 13, 6, 7, 8, 9, 10, 11, 14, 15, 16};

int numNeoPixels = 17;

//Color arrays for sides
int topRGB[3];
int backRGB[3];
int frontRGB[3];
int rightRGB[3];
int leftRGB[3];

// EEPROM Memory locations
int EEPROMstatusLoc = 0; // 1 byte
int frontColorLoc = 1; //3 bytes
int rightColorLoc = 4; //3 bytes
int leftColorLoc = 7;  //3 bytes
int topColorLoc = 10;  //3 bytes
int backColorLoc = 13; //3 bytes
int animationLoc = 21; //1 byte
int bleNameLoc = 22; //7 bytes
char validChars[37] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ1234567890";
char bleName[8];
int animation = 0; //0=none,1=rainbow,2=pulse,3=cycle sides
int pulseWaitms = 50; //Number of milliseconds to wait between brightness steps
int cycleWaitms = 200; //Number of milliseconds to wait between move colors to next side

// Connect CLK/MISO/MOSI to hardware SPI for BLE
// e.g. On UNO & compatible: CLK = 13, MISO = 12, MOSI = 11
#define ADAFRUITBLE_REQ 10
#define ADAFRUITBLE_RDY 3     // This should be an interrupt pin, on Uno thats #2 or #3
#define ADAFRUITBLE_RST 9

Adafruit_BLE_UART BTLEserial = Adafruit_BLE_UART(ADAFRUITBLE_REQ, ADAFRUITBLE_RDY, ADAFRUITBLE_RST);

//Setup the Neopixels
#define NEOPIN 6
// Parameter 1 = number of pixels in strip
// Parameter 2 = Arduino pin number (most are valid)
// Parameter 3 = pixel type flags, add together as needed:
//   NEO_KHZ800  800 KHz bitstream (most NeoPixel products w/WS2812 LEDs)
//   NEO_KHZ400  400 KHz (classic 'v1' (not v2) FLORA pixels, WS2811 drivers)
//   NEO_GRB     Pixels are wired for GRB bitstream (most NeoPixel products)
//   NEO_RGB     Pixels are wired for RGB bitstream (v1 FLORA pixels, not v2)
Adafruit_NeoPixel strip = Adafruit_NeoPixel(numNeoPixels, NEOPIN, NEO_GRB + NEO_KHZ800);



////////////////////////////////
// SETUP
////////////////////////////////
void setup(void)
{ 
  Serial.begin(9600);
  while(!Serial); // Leonardo/Micro should wait for serial init
  Serial.println(F("Minecraft Night Light with BLE Control"));
 
  //Check EEPROM initialization status and initialize it if necessary 
  int statusVal = checkEEPROMstatus();
  if(statusVal){
    Serial.println("EEPROM correctly initialized");
    readBLEnameFromEEPROM();
    Serial.print("BLE Name: ");
    Serial.print(bleName);
  }else{
    Serial.println("EEPROM needs to be initialized");
    Serial.println("Initializing EEPROM...");
    initEEPROM();
    Serial.println("EEPROM initialized");
  }
  
  //BTLEserial.setDeviceName("CUBE2"); /* 7 characters max! */
  BTLEserial.setDeviceName(bleName); /* 7 characters max! */
  BTLEserial.begin();
  
  strip.begin(); //Begin Neopixels
  readColorsFromEEPROM();
  setColorsFromMemory();
}

/**************************************************************************/
/*!
    Constantly checks for new events on the nRF8001
*/
/**************************************************************************/
aci_evt_opcode_t laststatus = ACI_EVT_DISCONNECTED;



//////////////////////////////
// MAIN LOOP
//////////////////////////////

void loop()
{
  // Tell the nRF8001 to do whatever it should be working on.
  BTLEserial.pollACI();

  // Ask what is our current status
  aci_evt_opcode_t status = BTLEserial.getState();
  // If the status changed....
  if (status != laststatus) {
    // print it out!
    if (status == ACI_EVT_DEVICE_STARTED) {
        Serial.println(F("* BLE Advertising started"));
    }
    if (status == ACI_EVT_CONNECTED) {
        Serial.println(F("* BLE Connected!"));
    }
    if (status == ACI_EVT_DISCONNECTED) {
        Serial.println(F("* BLE Disconnected or advertising timed out"));
    }
    // OK set the last status change to this one
    laststatus = status;
  }

  if (status == ACI_EVT_CONNECTED) {
    // Lets see if there's any data for us!
    if (BTLEserial.available()) {
      Serial.print("* "); Serial.print(BTLEserial.available()); Serial.println(F(" bytes available from BTLE"));
    }
    // OK while we still have something to read, get a character and print it out
    while (BTLEserial.available()) {
      char c = BTLEserial.read();
      Serial.print(c);
      if(c == 'C'){ //Set all sides to same color
        int red = BTLEserial.read();
        Serial.print(red);
        Serial.print(",");
        int green = BTLEserial.read();
        Serial.print(green);
        Serial.print(",");
        int blue = BTLEserial.read();
        Serial.print(blue);
        Serial.print(" ");
        updateSolidColor(red, green, blue);
      }else if(c == 'A'){ //Enable animation
         char anistyle = BTLEserial.read();
	 if(anistyle == 'R'){
           Serial.println(anistyle);
           Serial.println("Setting animation to rainbow");
           animation = 1;
	 }else if(anistyle == 'P'){ //Pulse animation
           Serial.println(anistyle);
           Serial.println("Setting animation to pulse");
	   animation = 2;
         }else if(anistyle == 'C'){ //Cycle colors animation
           Serial.println(anistyle);
           Serial.println("Setting animation to cycle");
           animation = 3;
	 }else{
           Serial.println(anistyle);
           Serial.println("Setting animation to none");
	   animation = 0;
           flashColor(2, 0, 255, 0); //flash green to indicate cmd rx
           setColorsFromMemory();
	 }
      }else if(c == 'B'){ //Recieve new BLE Name
        rxBLEname();
      }else if(c == 'S'){ //Set one side to new color
        char cubeSide = BTLEserial.read();
        Serial.print(cubeSide);
        Serial.print(",");
        int red = BTLEserial.read();
        Serial.print(red);
        Serial.print(",");
        int green = BTLEserial.read();
        Serial.print(green);
        Serial.print(",");
        int blue = BTLEserial.read();
        Serial.print(blue);
        Serial.print(" ");
        updateSideColor(cubeSide, red, green, blue);
      }else if(c == 'W'){ //Commands to save EEPROM data
        char saveEEPROMcmd = BTLEserial.read();
        if(saveEEPROMcmd == 'n'){
          Serial.println(saveEEPROMcmd);
          Serial.println("Saving BLE name to EEPROM");
          writeBLEnameToEEPROM();
        }else if(saveEEPROMcmd == 'c'){
          Serial.println(saveEEPROMcmd);
          Serial.println("Saving side colors to EEPROM");
          writeColorsToEEPROM();
        }else{
          Serial.println(saveEEPROMcmd);
          Serial.println("Unknown EEPROM save command.");
        }
      }
    }
    

    // Next up, see if we have any data to get from the Serial console
    if (Serial.available()) {
      // Read a line from Serial
      Serial.setTimeout(100); // 100 millisecond timeout
      String s = Serial.readString();

      // We need to convert the line to bytes, no more than 20 at this time
      uint8_t sendbuffer[20];
      s.getBytes(sendbuffer, 20);
      char sendbuffersize = min(20, s.length());

      Serial.print(F("\n* Sending -> \"")); Serial.print((char *)sendbuffer); Serial.println("\"");

      // write the data
      BTLEserial.write(sendbuffer, sendbuffersize);
    }
  }
  if(animation == 1){
    rainbowCycle(10);
  }else if(animation == 2){
    pulseColors(pulseWaitms);
  }else if(animation == 3){
    cycleColors(cycleWaitms);
  }
}


//////////////////////////////
//  FUNCTIONS
//////////////////////////////

// Check EEPROM status byte. If it contains a 'v' EEPROM
// has been initialized.
int checkEEPROMstatus(){
  int returnVal;
  char statusVal;
  statusVal = EEPROM.read(EEPROMstatusLoc);
  Serial.print("EEPROMstatus: ");
  Serial.println(statusVal);
  if(statusVal == 'v'){
    returnVal = 1;
  }else{
    returnVal = 0;
  }
  Serial.print("returnVal: ");
  Serial.println(returnVal);
  return(returnVal);
}


// Place initial values in all the necessary EEPROM
// memory locations
void initEEPROM(){
  //init memory locations for all colors
  for (int i = frontColorLoc; i < frontColorLoc+15; i++){
    EEPROM.write(i, 200);
  }
  //init animation memory location
  EEPROM.write(animationLoc, 'N');
  //init BLE advertised name
  EEPROM.write(bleNameLoc+0, 'C');
  EEPROM.write(bleNameLoc+1, 'U');
  EEPROM.write(bleNameLoc+2, 'B');
  EEPROM.write(bleNameLoc+3, 'E');
  EEPROM.write(bleNameLoc+4, ' ');
  EEPROM.write(bleNameLoc+5, ' ');
  EEPROM.write(bleNameLoc+6, ' ');
  //init EEPROMstatus
  EEPROM.write(EEPROMstatusLoc, 'v');
}


//Read BLE name from EEPROM and put into char array
void readBLEnameFromEEPROM(){
  //Loop through BLE Name EEPROM locations and read characters
  //until an invalid character is reached. Once an invalid character
  //is reached the function exits.
  Serial.println("Reading BLE Name from EEPROM");
  for(int i=0; i<8; i++){
    char currChar = EEPROM.read(bleNameLoc+i);
    if(checkValidChar(currChar)){
      Serial.print(currChar); Serial.println(" :Valid");
      bleName[i] = currChar;
    }else{
      Serial.print(currChar); Serial.println(" :Invalid");
      break;
    }
  }
}


//Write BLE name stored in variable to EEPROM
void writeBLEnameToEEPROM(){
    //Loop through name stored in bleName array and verify
    //it contains valid data before writting to EEPROM.
    for(int i=0; i<8; i++){
    char currChar = bleName[i];
    if(checkValidChar(currChar)){
      Serial.print(currChar); Serial.println(" :Valid");
      EEPROM.write(bleNameLoc+i, currChar);
    }else{
      Serial.print(currChar); Serial.println(" :Invalid");
      break; //Break out of loop if we hit an invalid char.
    }
  }
  flashColor(4, 0, 255, 0); //flash green to indicate successful save
}


//Receive new BLE Name over BLE UART
void rxBLEname(){
  char currChar = BTLEserial.read(); //Read first character of the new name
  int i=0;
  Serial.println();
  Serial.println("Receiving new BLE Name...");
  while(checkValidChar(currChar) && i<7){ //BLE name can only be 7 characters. Already recvd one.
    Serial.print("RECVD: ");
    Serial.println(currChar);
    bleName[i] = currChar; //push character onto bleName array
    currChar = BTLEserial.read(); //Read next character of the new name
    i++;
  }
  Serial.print("i finished at: ");
  Serial.println(i);
  for(i; i<8; i++){ //Blank out the rest of bleName array
    bleName[i] = 0;
  }
  Serial.print("Finished receiving BLE Name: ");
  Serial.println(bleName);
  flashColor(2, 0, 0, 255); //flash blue to indicate successful rx
}


//Check if passed character is valid for BLE name
int checkValidChar(char checkChar){
    int returnVal = 0;
    for(int c=0; c<36; c++){
      if(validChars[c] == checkChar){
        returnVal = 1;
        break;
      }
    }
    return returnVal;
}


//Read saved colors from EEPROM and store them in color arrays
void readColorsFromEEPROM(){
  //Read top colors from EEPROM
  for(int i=0; i<3; i++){
    topRGB[i] = EEPROM.read(topColorLoc+i);
  }
  //Read front colors from EEPROM
  for(int i=0; i<3; i++){
    frontRGB[i] = EEPROM.read(frontColorLoc+i);
  }
  //Read back colors from EEPROM
  for(int i=0; i<3; i++){
    backRGB[i] = EEPROM.read(backColorLoc+i);
  }
  //Read right colors from EEPROM
  for(int i=0; i<3; i++){
    rightRGB[i] = EEPROM.read(rightColorLoc+i);
  }
  //Read left colors from EEPROM
  for(int i=0; i<3; i++){
    leftRGB[i] = EEPROM.read(leftColorLoc+i);
  }
  animation = EEPROM.read(animationLoc);
}


//Save colors and animation to EEPROM
void writeColorsToEEPROM(){
  //Write top colors to EEPROM
  for(int i=0; i<3; i++){
     EEPROM.write(topColorLoc+i, topRGB[i]);
  }
  //Write front colors to EEPROM
  for(int i=0; i<3; i++){
     EEPROM.write(frontColorLoc+i, frontRGB[i]);
  }
  //Write back colors to EEPROM
  for(int i=0; i<3; i++){
     EEPROM.write(backColorLoc+i, backRGB[i]);
  }
  //Write right colors to EEPROM
  for(int i=0; i<3; i++){
     EEPROM.write(rightColorLoc+i, rightRGB[i]);
  }
  //Write left colors to EEPROM
  for(int i=0; i<3; i++){
     EEPROM.write(leftColorLoc+i, leftRGB[i]);
  }
  //Write animation style to EEPROM
  EEPROM.write(animationLoc, animation);
  flashColor(2, 0, 255, 0); //flash green to indicate save complete
}


//Set NeoPixels to colors stored in color arrays
void setColorsFromMemory(){
  //Set top pixels to color from EEPROM
  for(int i=0; i<5; i++){
    strip.setPixelColor(top[i], topRGB[0], topRGB[1], topRGB[2]);
  }
  //Set front pixels to color from EEPROM
  for(int i=0; i<3; i++){
    strip.setPixelColor(front[i], frontRGB[0], frontRGB[1], frontRGB[2]);
  }
  //Set back pixels to color from EEPROM
  for(int i=0; i<3; i++){
    strip.setPixelColor(back[i], backRGB[0], backRGB[1], backRGB[2]);
  }
  //Set right pixels to color from EEPROM
  for(int i=0; i<3; i++){
    strip.setPixelColor(right[i], rightRGB[0], rightRGB[1], rightRGB[2]);
  }
  //Set left pixels to color from EEPROM
  for(int i=0; i<3; i++){
    strip.setPixelColor(left[i], leftRGB[0], leftRGB[1], leftRGB[2]);
  }
  strip.show();  //set the colors on the pixels
}


//Set all the NeoPixels to a solid color
void updateSolidColor(int red, int green, int blue){
  //Set all the pixels to the same color
  for(int i=0; i < numNeoPixels; i++){
    strip.setPixelColor(i, red, green, blue);
  }
  //Hmmm need to do something different here. Some animations are ok
  //for solid colors. (like pulse !AP)
  animation = 0;
  strip.show();
  //Update color arrays with new values in case we save to eeprom
  topRGB[0] = red;
  topRGB[1] = green;
  topRGB[2] = blue;
  backRGB[0] = red;
  backRGB[1] = green;
  backRGB[2] = blue;
  frontRGB[0] = red;
  frontRGB[1] = green;
  frontRGB[2] = blue;
  rightRGB[0] = red;
  rightRGB[1] = green;
  rightRGB[2] = blue;
  leftRGB[0] = red;
  leftRGB[1] = green;
  leftRGB[2] = blue;
}


//Set one side to a new color
void updateSideColor(char cubeSide, int red, int green, int blue){
  if(cubeSide == 't'){
    //Update color array with new values in case we save to eeprom
    topRGB[0] = red;
    topRGB[1] = green;
    topRGB[2] = blue;
    //Set the top to new color
    for(int i=0; i < 5; i++){
      strip.setPixelColor(top[i], red, green, blue);
    }
  }else if(cubeSide == 'f'){
    //Update color array with new values in case we save to eeprom
    frontRGB[0] = red;
    frontRGB[1] = green;
    frontRGB[2] = blue;
    //Set the front to new color
    for(int i=0; i < 3; i++){
      strip.setPixelColor(front[i], red, green, blue);
    }
  }else if(cubeSide == 'r'){
    //Update color array with new values in case we save to eeprom
    rightRGB[0] = red;
    rightRGB[1] = green;
    rightRGB[2] = blue;
    //Set the right to new color
    for(int i=0; i < 3; i++){
      strip.setPixelColor(right[i], red, green, blue);
    }
  }else if(cubeSide == 'l'){
    //Update color array with new values in case we save to eeprom
    leftRGB[0] = red;
    leftRGB[1] = green;
    leftRGB[2] = blue;
    //Set the left to new color
    for(int i=0; i < 3; i++){
      strip.setPixelColor(left[i], red, green, blue);
    }
  }else if(cubeSide == 'b'){
    //Update color array with new values in case we save to eeprom
    backRGB[0] = red;
    backRGB[1] = green;
    backRGB[2] = blue;
    //Set the back to new color
    for(int i=0; i < 3; i++){
      strip.setPixelColor(back[i], red, green, blue);
    }
  }
  strip.show(); //Display the color change
}


//Flash all NeoPixels a color and then set back to original colors.
void flashColor(int numFlashes, int red, int green, int blue){
  int origAnimation = animation;
  animation = 0; //temporarily disable animations
  for(int f=0; f<numFlashes; f++){
    for(int i=0; i < numNeoPixels; i++){
      strip.setPixelColor(i, red, green, blue);
    }
    strip.show();
    delay(250);
    for(int i=0; i < numNeoPixels; i++){
      strip.setPixelColor(i, 0, 0, 0); //Set all to off
    }
    strip.show();
    delay(250);
  }
  //Set everything back to original settings
  setColorsFromMemory();
  strip.show();
  animation = origAnimation;
}


//This sets all the pixels to a certain brightness based on percent.
//The in memory RGB value is divided by the percent passed in and set on the pixel.
void dimColors(int dimPercent) {
  //100% is fully on and 0% is fully off
  float dimVal = float(dimPercent)/100;
  //Set top pixels to dimVal
  for(int i=0; i<5; i++){
    strip.setPixelColor(top[i], topRGB[0]*dimVal, topRGB[1]*dimVal, topRGB[2]*dimVal);
  }
  //Set front pixels to dimVal
  for(int i=0; i<3; i++){
    strip.setPixelColor(front[i], frontRGB[0]*dimVal, frontRGB[1]*dimVal, frontRGB[2]*dimVal);
  }
  //Set back pixels to dimVal
  for(int i=0; i<3; i++){
    strip.setPixelColor(back[i], backRGB[0]*dimVal, backRGB[1]*dimVal, backRGB[2]*dimVal);
  }
  //Set right pixels to dimVal
  for(int i=0; i<3; i++){
    strip.setPixelColor(right[i], rightRGB[0]*dimVal, rightRGB[1]*dimVal, rightRGB[2]*dimVal);
  }
  //Set left pixels to dimVal
  for(int i=0; i<3; i++){
    strip.setPixelColor(left[i], leftRGB[0]*dimVal, leftRGB[1]*dimVal, leftRGB[2]*dimVal);
  }
  strip.show();  //set the colors on the pixels
}


//Make all pixels dim 1% per step until they are off and then
//brighten them again until they are back to the starting values.
void pulseColors(uint8_t wait) {
  //100% is fully on and 0% is fully off
  for(int d=99; d > 1; d--){
    dimColors(d);
    //Visually the cube was staying bright longer than it was dimming.
    //This speeds up the wait between steps at brighter levels
    if(d > 75){
      delay(wait/3);
    }else if(d > 50 && d <= 75){
      delay(wait/2);
    }else{
      delay(wait);
    }
  }
  delay(wait); //wait one more time before lighting it back up
  for(int d=0; d < 101; d++){
    dimColors(d);
    if(d > 75){
      delay(wait/3);
    }else if(d > 50 && d <= 75){
      delay(wait/2);
    }else{
      delay(wait);
    }
  }
}


void cycleColors(uint8_t wait) {
                     // front,    right,    back,     left
  int fourSides[12] = {6, 7, 8, 9, 10, 11, 0, 1, 2, 14, 15, 16};

  int seq1[12] = {leftRGB[0], leftRGB[1], leftRGB[2], frontRGB[0], frontRGB[1], frontRGB[2], rightRGB[0], rightRGB[1], rightRGB[2], backRGB[0], backRGB[1], backRGB[2]};
  int seq2[12] = {backRGB[0], backRGB[1], backRGB[2], leftRGB[0], leftRGB[1], leftRGB[2], frontRGB[0], frontRGB[1], frontRGB[2], rightRGB[0], rightRGB[1], rightRGB[2]};
  int seq3[12] = {rightRGB[0], rightRGB[1], rightRGB[2], backRGB[0], backRGB[1], backRGB[2], leftRGB[0], leftRGB[1], leftRGB[2], frontRGB[0], frontRGB[1], frontRGB[2]};
  int seq4[12] = {frontRGB[0], frontRGB[1], frontRGB[2], rightRGB[0], rightRGB[1], rightRGB[2], backRGB[0], backRGB[1], backRGB[2], leftRGB[0], leftRGB[1], leftRGB[2]};


  for(int i=0; i<12; i+=3){
    strip.setPixelColor(fourSides[0+i], seq1[0+i], seq1[1+i], seq1[2+i]);
    strip.setPixelColor(fourSides[1+i], seq1[0+i], seq1[1+i], seq1[2+i]);
    strip.setPixelColor(fourSides[2+i], seq1[0+i], seq1[1+i], seq1[2+i]);
  }
  strip.show();
  delay(wait);

  for(int i=0; i<12; i+=3){
    strip.setPixelColor(fourSides[0+i], seq2[0+i], seq2[1+i], seq2[2+i]);
    strip.setPixelColor(fourSides[1+i], seq2[0+i], seq2[1+i], seq2[2+i]);
    strip.setPixelColor(fourSides[2+i], seq2[0+i], seq2[1+i], seq2[2+i]);
  }
  strip.show();
  delay(wait);

  for(int i=0; i<12; i+=3){
    strip.setPixelColor(fourSides[0+i], seq3[0+i], seq3[1+i], seq3[2+i]);
    strip.setPixelColor(fourSides[1+i], seq3[0+i], seq3[1+i], seq3[2+i]);
    strip.setPixelColor(fourSides[2+i], seq3[0+i], seq3[1+i], seq3[2+i]);
  }
  strip.show();
  delay(wait);

  for(int i=0; i<12; i+=3){
    strip.setPixelColor(fourSides[0+i], seq4[0+i], seq4[1+i], seq4[2+i]);
    strip.setPixelColor(fourSides[1+i], seq4[0+i], seq4[1+i], seq4[2+i]);
    strip.setPixelColor(fourSides[2+i], seq4[0+i], seq4[1+i], seq4[2+i]);
  }
  strip.show();
  delay(wait);
}


//Rainbow animation
void rainbow(uint8_t wait) {
  uint16_t i, j;
  for(j=0; j<256; j++) {
    for(i=0; i<strip.numPixels(); i++) {
      strip.setPixelColor(fullString[i], Wheel((i+j) & 255));
    }
    strip.show();
    delay(wait);
  }
}


// Slightly different, this makes the rainbow equally distributed throughout
void rainbowCycle(uint8_t wait) {
  uint16_t i, j;
  for(j=0; j<256; j++) { // 1 cycle of all colors on wheel
    for(i=0; i< strip.numPixels(); i++) {
      strip.setPixelColor(fullString[i], Wheel(((i * 256 / strip.numPixels()) + j) & 255));
    }
    strip.show();
    delay(wait);
  }
}


// Input a value 0 to 255 to get a color value.
// The colours are a transition r - g - b - back to r.
uint32_t Wheel(byte WheelPos) {
  WheelPos = 255 - WheelPos;
  if(WheelPos < 85) {
   return strip.Color(255 - WheelPos * 3, 0, WheelPos * 3);
  } else if(WheelPos < 170) {
    WheelPos -= 85;
   return strip.Color(0, WheelPos * 3, 255 - WheelPos * 3);
  } else {
   WheelPos -= 170;
   return strip.Color(WheelPos * 3, 255 - WheelPos * 3, 0);
  }
}
