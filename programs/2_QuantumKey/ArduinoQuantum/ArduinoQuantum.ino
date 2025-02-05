/*
  Arduino program for debugging purpose and to transmit/receive
  polarisation keys between two parties (Mission 2). 
  Implemented features: see the HELP section
 Author: Qcumber (2018) 
*/

#include <EEPROM.h>
#include "EEPROMAnything.h"
//#include <Stepper.h>
#include <PolarizerMotor.h>
#include <Entropy.h>

// Important parameters
const int seqLength = 16;  // Polarisation sequence length (16 bit)
const int pinLsr = 12;      // Laser pin
const int pinDeb = 13;     // Debugging pin (LED on the board)
const int sensorLoc = 0;   // A0
int catchTh = 200;   // Threshold in CATCH command 200 = ~1V.

// Parameters
const int stepDelay = 2; // 2 ms
const int stepSpeed = 1000; // 1000 rpm: any number high enough would do
const int stepsPerRevolution = 2048; 

const int serialTimeout = 100; // 100 ms

const int EEloc_angleCurrent = 0; // Takes 2 bytes
const int EEloc_angleTarget = 2;  // Takes 2 bytes

int polOffset = 0; // Polarisation offset, retrieve and store in EEProm
const int EEloc_polOffset = 4;  // Takes 2 bytes

int polSeq[seqLength] = {0}; // int datatype, in multiples of 45 degrees.

const int seqStepTime = 300;  // Time between each steps. Def: 200 ms
const int seqInitTarget = 1;   // Set initialization polarisation for seq always (D)
const int seqPinStart = 0;  // Time in sequence to start / ON the pin
const int seqPinStop = 300;   // Time in sequence to stop / OFF the pin
const int seqReadTime = 150;  // 100 ms (hopefully in the middle of the laser pulse)
const int seqSyncBlink = 300;  // 200 ms (to initialise the signal)
//int moveType;
int moveStepper(int moveType = 1, int steps = 30); //2:approachAngle, anything else: gotoAngle 
    
// To deal with floating point rounding off error
// in the conversion between angle and steps,
// we define a variable which keeps track of that.
float stepsErrAcc = 0; 
    
// initialize the stepper library on pins 8 through 11:
// need to swap pins 10 and 9 (due to wiring reason)
PolarizerMotor myMotor(8,9);

void setup() {
  // set the speed at 60 rpm: 
  myMotor.initialize();
  // myMotor.setSpeed(stepSpeed);
  // initialize the serial port:
  Serial.begin(38400);
  Serial.setTimeout(serialTimeout);
  // Obtain the polarisation offset from EEProm
  EEPROM_readAnything(EEloc_polOffset, polOffset);
  // Set the laser pin to be output
  pinMode(pinLsr, OUTPUT);
  pinMode(pinDeb, OUTPUT); // For debugging
  // Set up the entropy library (to generate random numbers)
  Entropy.initialize();
}

void loop() {
  while (!Serial.available()); // Listen to serial input
  char serbuf[16] = ""; // Reinitialise buffer (16 bytes, char array size 16)
  Serial.readBytesUntil(' ', serbuf, 15); // Until whitespace
  // Serial.print("serbuf is:"); // Debug
  // Serial.println(serbuf); // Debug
  // Obtain which input command (enumerated)
  int enumc = -1; // default choice
  int maxChoice = 21;
  char sercmd[maxChoice][8] = {"HELP",             // 0
    "SETANG", "ANG?", "SETPOL", "POL?", "SETHOF",  // 5
    "HOF?", "POLSEQ", "RNDSEQ", "RNDBAS", "SEQ?",  // 10
    "LASON", "LASOFF", "VOLT?", "RUNSEQ", "TXSEQ", // 15
    "RXSEQ", "CATCH", "TH?", "SETTH","*IDN?"};     // 20
  for (int c=0; c<maxChoice; c++){
    if (strcasecmp(sercmd[c],serbuf) == 0){ 
      enumc = c;// Obtain match
    }
  }
  
  // Declaring some other parameters
  char valbuf[16] = "";      // Buffer to receive chartype value from serial
  int angleTarget;
  int setPolTo; 
  float polFloat;
  char polseqbuf[seqLength] = ""; // Buffer to receive chartype pol sequences from serial 
  int polSeqMod[seqLength] = {0}; // Polarisation sequence within the range (0,3).       
  int sensorValue;
  
  
  // Switching between different cases
  switch(enumc){
    
    case 0: //HELP
      Serial.print(F("Quantum Key Construction\n"));
      Serial.print(F("HELP        \t    Print help statement\n"));
      Serial.print(F("SETANG X    \t    Set angle to X (in degrees)\n"));
      Serial.print(F("ANG?        \t    Ask for current angle\n"));
      Serial.print(F("SETPOL X    \t    Set polarisation to X -> 0(H), 1(D), 2(V), 3(A)\n"));
      Serial.print(F("POL?        \t    Ask for current polarisation\n"));
      Serial.print(F("SETHOF X    \t    Set angle offset for H polarisation\n"));
      Serial.print(F("HOF?        \t    Ask for the angle offset for H polarisation\n"));
      Serial.print(F("POLSEQ X..  \t    Set polarisation sequence as X..\n"));
      Serial.print(F("RNDSEQ      \t    Set random polarisation sequence\n"));
      Serial.print(F("RNDBAS      \t    Set random polarisation basis\n"));
      Serial.print(F("SEQ?        \t    Ask for sequence\n"));
      Serial.print(F("LASON       \t    Turn on laser\n"));
      Serial.print(F("LASOFF      \t    Turn off laser\n"));
      Serial.print(F("VOLT?       \t    Ask for sensor voltage\n"));
      Serial.print(F("RUNSEQ      \t    Run the sequence (generic)\n"));
      Serial.print(F("TXSEQ       \t    Run the sequence (as a sender)\n"));
      Serial.print(F("RXSEQ       \t    Run the sequence (as a receiver)\n"));
      Serial.print(F("CATCH       \t    Wait for laser light and display time\n"));
      Serial.print(F("TH?         \t    Gets detector threshold for CATCH - 200=1V\n"));
      Serial.print(F("SETTH X     \t    Set laser detection threshold (0-1023) - 200=1V\n"));
      Serial.print(F("*IDN?       \t    Gets device name (Quantum/Classical)\n"));
      break; 
      
    case 1: //SETANG X
      // listen again (for angle)
      while (!Serial.available());
      Serial.readBytesUntil(' ', valbuf, 15); // Until whitespace
      angleTarget = atoi(valbuf);
      // Serial.print("angleTarget is:"); //Debug
      // Serial.println(angleTarget); // Debug
      EEPROM_writeAnything(EEloc_angleTarget, angleTarget);
      moveStepper();
      Serial.println("OK");
      break;
      
    case 2: //ANG?
      //EEPROM_readAnything(EEloc_angleTarget, angleTarget);
      Serial.println(myMotor.readAngle());
      break;
      
    case 3: //SETPOL X
      // listen again (for polarisation)
      while (!Serial.available());
      Serial.readBytesUntil(' ', valbuf, 15); // Until whitespace
      setPolTo = (int)valbuf[0] - 48; // Only convert the first char: the rest are bullshit
      if (setPolTo < 0 || setPolTo > 3){
        Serial.println("Input error detected.");
        setPolTo = 0; // If input error, assume zero polarisation (H)
      }
      angleTarget = setPolTo * 45 + polOffset;
      EEPROM_writeAnything(EEloc_angleTarget, angleTarget);
      moveStepper();
      Serial.println("OK");
      break;
      
    case 4: //POL?
      EEPROM_readAnything(EEloc_angleTarget, angleTarget);
      // Convert to polarisation
      polFloat = (angleTarget - polOffset) / 45.; 
      Serial.println(polFloat);
      break;
      
    case 5: //SETHOF X
      // listen again (for ofset value)
      while (!Serial.available());
      Serial.readBytesUntil(' ', valbuf, 15); // Until whitespace
      polOffset = atoi(valbuf);
      EEPROM_writeAnything(EEloc_polOffset, polOffset);
      Serial.println("OK");
      break;
      
    case 6: //HOF?
      EEPROM_readAnything(EEloc_polOffset, polOffset);
      Serial.println(polOffset);
      break;
      
    case 7: //POLSEQ X
      // Listen for the sequence string
      while (!Serial.available());
      Serial.readBytesUntil(' ', polseqbuf, seqLength); // Until seqLength
      //Serial.print("polseqbuf is:"); // Debug
      //Serial.println(polseqbuf); // Debug
      transCharSeq(polseqbuf, polSeqMod, seqLength);    // Translate from char[] to int[]
      polToSeq(polSeqMod, seqLength, polSeq);
      // // Debug (print the morphed sequence)
      // for (int i=0; i<seqLength; i++){
      //   Serial.print(polSeq[i]);
      // } Serial.print('\n');
      // // Debug endline
      Serial.println("OK");
      break;
      
    case 8: //RNDSEQ
      specialRandom(polSeqMod, seqLength, 4); // Create random array
      polToSeq(polSeqMod, seqLength, polSeq); // Transfer to polSeq
      Serial.println("OK");
      break;
      
    case 9: //RNDBAS
      specialRandom(polSeqMod, seqLength, 2); // Create random array
      polToSeq(polSeqMod, seqLength, polSeq); // Transfer to polSeq
      Serial.println("OK");
      break;
      
    case 10: //SEQ?
      printSeq();
      break;
      
    case 11: //LASON
      digitalWrite(pinLsr, HIGH);      
      Serial.println("LASER ON!");
      break;
      
    case 12: //LASOFF
      digitalWrite(pinLsr, LOW);      
      Serial.println("LASER OFF!");
      break;
      
    case 13: //VOLT?
      sensorValue = analogRead(sensorLoc);
      // Serial.print("Val: ");
      Serial.println(sensorValue);
      break;
      
    case 14: //RUNSEQ
      runSequence(0);  // mode 0 (for debugging)
      Serial.println("OK");
      break;
      
    case 15: //TXSEQ
      runSequence(1);  // mode 1 (for TX)
      Serial.println("OK");
      break;
      
    case 16: //RXSEQ
      runSequence(2);  // mode 2 (for RX)
      // In mode 2, it will print the sensor values
      break;

    case 17: //CATCH
      Serial.print(lasCatch()); 
      Serial.println(" ms is when the light triggers.");
      break;

    case 18: //TH?
      Serial.println(catchTh);
        break;

    case 19: //SETTH
      while (!Serial.available());
      Serial.readBytesUntil(' ', valbuf, 15);
      catchTh = atoi(valbuf);
      Serial.println("OK");
      break;

    case 20: //*IDN?
      Serial.println("Quantum");
      break;
     
    default:
      Serial.println("Unknown command");
      break;
  }
}

int checkSign(double num){
  if (num > 0) return 1;
  if (num < 0) return -1;
  return 0;
}

int specialMod(int num, int mod){
  // Special modulus operation to limit to positive modulus number
  int result = num % mod;
  if (result < 0){
    result += mod;
  }
  return result;
}

int printSeq(){
  // Print the stored sequence (values only from 0 to 3)
  for (int i=0; i<seqLength; i++){
    Serial.print(specialMod(polSeq[i], 4) );  // mod 4
  } 
  Serial.print('\n'); // endline
  
  return 1; // Exits 
}

unsigned long lasCatch(){
  int sensorValue = 0;
  while (sensorValue < catchTh){
    sensorValue = analogRead(sensorLoc);
  }
  //Serial.print("sensorVal: ");
  //Serial.println(sensorValue);
  //Serial.print("catchTh: ");
  //Serial.println(catchTh);
  return millis();
}

int specialRandom(int array[], int arrayLength, int maxVal){
  // Fill in the array with specialised random numbers in [0, maxVal)   
  for(int i=0; i<arrayLength; i++){
    array[i] = Entropy.random(maxVal);     
  }
}

int moveStepper(int moveType, int steps) {
  // Serial.print("moveStepper called with moveType:");
  // Serial.println(moveType);
  int current;
  int target;
  // read current and target angles from Arduino's EEPROM memory
  // EEPROM_readAnything(EEloc_angleCurrent, current);
  EEPROM_readAnything(EEloc_angleTarget, target);
  if (moveType==2) {
//     Serial.println("Calling approachAngle");
    myMotor.approachAngle(target, steps);
  } else {
    //Serial.println("Calling gotoAngle");
    //Serial.print("target is:");
    //Serial.println(target);
    //Serial.print(myMotor.readAngle());
    myMotor.gotoAngle(target);
  }
  // Current angle is now the target angle
  // EEPROM_writeAnything(EEloc_angleCurrent, target);
  // Serial.print("outside: ");
  // Serial.println(myMotor.readAngle());
  return 1;
}

int transCharSeq(char polseqbuf[], int polSeqMod[], int seqLength){
  // Translate char array to int array
  int polnow;
  
  // Iterating over the array
  for(int i=0; i<seqLength; i++){
    // Convert ASCII char to int
    polnow = (int)polseqbuf[i] - 48; 
    // Convert all nulls to zero ints (if sequences less than 64 digits)
    if (polnow == -48) polnow = 0;
    // Convert all nonsense values to 0
    if (polnow < 0 || polnow > 3){
      Serial.println("Input error detected."); 
      polnow = 0; // Skipping that particular input
    }
    // Update the int array  
    polSeqMod[i] = polnow;
  }
  
  return 1; // Exit the function
}

int polToSeq(int polSeqMod[], int seqLength, int polSeq[]){
  // This mani-fold thingy is to implement an efficient rotation, such that
  // the maximum rotation per sequence step is not more than 2 (90 degrees)
  int mani = 4; // Group size of 4 (H,D,V,A)
  int fold = 0; // Starting from 0 to 3;

  for(int i=0; i<seqLength; i++){
    // Constructing the polSeq array
    polSeq[i] = polSeqMod[i] + mani * fold;
    // Increase manifold if 3 -> 0, and decrease manifold if 0 -> 3.
    if (i<seqLength-1){  // Skipping last value comparison
      if (polSeqMod[i]==3 && polSeqMod[i+1]==0){
        fold += 1;    // Increase manifold
      }
      if (polSeqMod[i]==0 && polSeqMod[i+1]==3){
        fold -= 1;    // Decrease manifold
      }
    }
  }
  return 1;
}

int runSequence(int mode){
  // Mode 0 (or anything else): Plain sequence + blink on pin 13 (debugging)
  // Mode 1: Mode 0 + laser + sync signal (TX)
  // Mode 2: Mode 1 + sense + triggered   (RX)
  int angleTarget;
  unsigned long timeNow = 0;
  unsigned long timeStep = 0;
  // Only for mode 2
  int sensorArrVal[seqLength] = {0};
  
  // Get ready : polarisation initialiastion (def: D)
  EEPROM_readAnything(EEloc_polOffset, polOffset);   // Update the polarisation offset
  angleTarget = seqInitTarget * 45 + polOffset;
  EEPROM_writeAnything(EEloc_angleTarget, angleTarget);
  // Move motor using the more precise approachAngle function 
  //(default is gotoAngle with up to 5 deg variation)
  moveStepper(2, 30);
  
  if (mode == 1){
    timeStep = millis() + seqSyncBlink; // Get the next time step
    // ON part of the sync pulse 
    digitalWrite(pinLsr, HIGH);         
    // Block the signal until the next time step
    while(timeNow<timeStep){
      timeNow = millis();     // Keep checking the time
    }
    // OFF part of the sync pulse 
    digitalWrite(pinLsr, LOW);         
    timeStep += seqSyncBlink; // Set the main sequence trigger
  } else if (mode == 2) {
    //Serial.println("Listen for sync pulse."); // debug
    timeStep = lasCatch();    // Caught the sync signal
    timeStep+= 2 * seqSyncBlink;
  } else{
    // Main sequence starts now
    timeStep = millis();
  }
  
  // Main sequence 
  for (int i=0; i<seqLength; i++){
    // Block until the next timeStep
    while(timeNow<timeStep){
      timeNow = millis();    // Keep checking the time
    } 
    // Move to the prescribed angle
    angleTarget = polSeq[i] * 45 + polOffset;
    EEPROM_writeAnything(EEloc_angleTarget, angleTarget);
    // Serial.println("Moving to prescribed angle");
    moveStepper(2, 20);

    timeStep = millis();
    // Perform operation on the laser / sensor
    if (mode == 1){
      pinBlink(timeStep, pinLsr);  // Blinks on laser
    } else if (mode == 2) {
      // Sense at the predetermined time and add result to array
      sensorArrVal[i] = senseAtTime(timeStep, sensorLoc); 
    } else{
      pinBlink(timeStep, pinDeb);  // Blinks on pin 13
    }
    // Update the timeStep
    timeStep += seqStepTime;
  }
  
  // Go back to initialisation (def: D)
  angleTarget = seqInitTarget * 45 + polOffset;
  EEPROM_writeAnything(EEloc_angleTarget, angleTarget);
  moveStepper(2,30);
  
  // Sends the measurement values to serial (for mode == 2)
  if (mode == 2){
    for (int i=0; i<seqLength; i++){
      Serial.print(sensorArrVal[i]);
      Serial.print(" "); // Space between values
    }
    Serial.print("\n");  // New line, to signify end 
  }
}

int senseAtTime(unsigned long timeStep, int sensorLoc){
  int sensorValue;
  unsigned long timeNow = 0;
  
  // Block until the seqReadTime
  while(timeNow < timeStep + seqReadTime){
    timeNow = millis();
  } 
  sensorValue = analogRead(sensorLoc);
  
  return sensorValue;
}

int pinBlink(unsigned long timeStep, int pin){
  unsigned long timeNow = 0;
  
  // Block until the start trigger
  while(timeNow < timeStep + seqPinStart){
    timeNow = millis();
  }
  // Serial.println(timeNow-timeStep); // debugging
  digitalWrite(pin, HIGH);             // ON the pin
  
  // Block until the stop trigger
  while(timeNow < timeStep + seqPinStop){
    timeNow = millis();
  }
  digitalWrite(pin, LOW);              // ON the pin
  
  return 1;    // Exit process
}
