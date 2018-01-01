// PINS: float switch = d2, pump relay = d3, pressure transducer = a1, solenoind = d4
// this should run on a nano or uno, but does nout output anything
//

#include <Arduino.h>
#include <Wire.h>

//Variables for pressure switch
int floatSwitchPin = 2; //float switch digital pin
int pswRelayPin = 3; //relay digital pin
int psiTransPin = A1; //
int lowLimit = 65; //psi low lowLimit
int highLimit = 120; //psi high limit
float psi = 0; //initiaize psi
float avgPSI = 180; //start the psi high to not activate the pump on boot
float combinedPSI = 0; //float for adding psi together before dividing for avgPSI
boolean pumpState = false; //boolean for if the relay is active or not. start as not
unsigned long psiReadInterval = 1000; //interval for reading psi in the smoothing loop
int pswSmoothCount = 10; //smooth psi readings over x psiReadInterval
int pswSmoothCurCount = 0; //Counter for keeping track of smooth interations
unsigned long pumpOverRunStop = 300000; //milliseconds for max run time on the pump
unsigned long pumpMinInterval = 600000; //milliseconds for min pump run intervals

//Variables for mister  //Activate relay if psi is too low and flot returns 0

int misterRelayPin = 4;
boolean misterState = false;
unsigned long mistInterval = 5; //mist active for X minutes, converts to millis later
unsigned long mistAct = 4; //mist for Y seconds

//Timers for pressure switch
unsigned long pswIntervalPrev = 0; //millis since last psw activation
unsigned long psiSmoothTimer = 0; //counts to psiReadInterval for smoothing
unsigned long psiIntervalTimer = 0; //timer for psi smoothing
unsigned long pumpRunTimer = 0; //counts time pump is running to not exceed X min
unsigned long pumpLastStartTimer = 0; //used to count time between pump runs

//Float smoother Variables
boolean lowWaterInst = true; //instantaneous lowWater sensor value before smoothing. Set to true by default so the program has time to smooth and not turn on the pump right away
boolean lowWater; //Boolean for float sensor
int lowWaterSmoothCounter = 0;
int lowWaterTCount = 0;
int lowWaterFCount = 0;

//Timers for mister
unsigned long mistIntervalPrev = 0; //millis since last mist activation
unsigned long mistActiveTimer = 0; //timer for keeping mist active


//Function to return current psi reading
float readPressure() {
  //psi transducer returns a value 0-1023 which needs to be converted in a voltage
  //Voltage returned needs to be 0.5-5.0v and is converted to psi
  //to get the 0.03 divide the transducer max psi by the voltage range
  //so 150 psi / 4.5v = 0.03, the 1 psi is equal to 0.03v

  int psiSensorRaw = analogRead(psiTransPin);
  float voltage = psiSensorRaw * (5.0 / 1023.0);
  float psi = (voltage - 0.5) / 0.03;
  return psi;
}
//function to return float switch status
boolean floatStatus() {
  if (digitalRead(floatSwitchPin) == LOW) {
    //Low means sensor is floating
    lowWater = false;
  } else if (digitalRead(floatSwitchPin) == HIGH) {
    //High means sensor is not floating
    lowWater = true;
  }
  return lowWater;
}


void setup() {
  Serial.begin(9600);
  //Setup Pins
  pinMode(pswRelayPin, OUTPUT); //pressure switch relay
  pinMode(floatSwitchPin, INPUT_PULLUP); //float pin set for reading
  pinMode(misterRelayPin, OUTPUT); //mister solenoid relay
    // put your main code here, to run repeatedly:
  //For the relays, HIGH is not active, LOW is active
  //Set relays to high at beginning of program so they don't mist
  digitalWrite(pswRelayPin, HIGH);
  digitalWrite(misterRelayPin, HIGH);
}

void loop() {
  //Get current millis and store it for this loop
  unsigned long currentMillis = millis();

  //BEGIN MISTER ACTIVATION

  if ((currentMillis - mistIntervalPrev) >= (mistInterval * 60 * 1000)) {
    digitalWrite(misterRelayPin, LOW);
    mistIntervalPrev = currentMillis;
    mistActiveTimer = currentMillis;
    misterState = true;
  }
  else if ((currentMillis - mistActiveTimer) >= (mistAct * 1000) && (misterState == true)) {
    digitalWrite(misterRelayPin, HIGH);
    misterState = false;
  }
//END MISTER ACTIVATION

//BEGIN PRESSURE SWITCH

//Average the psi reading over multiple loops to smooth it
  if ((unsigned long)(currentMillis - psiSmoothTimer) >= psiReadInterval) {
    float psi = readPressure();
    combinedPSI = combinedPSI + psi;
    if (pswSmoothCurCount == pswSmoothCount) {
      avgPSI = combinedPSI/pswSmoothCount;
      pswSmoothCurCount = 0;
      combinedPSI = 0;
      psiSmoothTimer = currentMillis;
    } else {
      pswSmoothCurCount = pswSmoothCurCount + 1;
    }
  }

  Serial.print(avgPSI);
  Serial.print("\n");

  //Get instantaneous float switch status
  lowWaterInst = floatStatus();

  if (lowWaterInst == true) {
    lowWaterTCount = lowWaterTCount + 1;
  } else if (lowWaterInst == false) {
    lowWaterFCount = lowWaterFCount + 1;
  }
  if (lowWaterSmoothCounter < 101) {
    lowWaterSmoothCounter = lowWaterSmoothCounter + 1;
  } else if (lowWaterSmoothCounter == 100) {
    if (lowWaterTCount > 60) {
      lowWater = true;
    } else if (lowWaterFCount > 60) {
      lowWater = false;
    }
    //Reset all lowWater counters for next set
    lowWaterTCount = 0;
    lowWaterFCount = 0;
    lowWaterSmoothCounter = 0;
  }

  //Activate relay if psi is too low and lowWater returns false
  //if ((avgPSI < lowLimit) && (pumpState == false) && (lowWater == false) && ((currentMillis - pumpLastStartTimer) > 600000))  {
  if ((avgPSI < lowLimit) && (pumpState == false) && (lowWater == false))  {
    if ((currentMillis < pumpMinInterval) || ((currentMillis - pumpLastStartTimer) > pumpMinInterval)) {
      digitalWrite(pswRelayPin, LOW);
      pumpLastStartTimer = currentMillis; //set time the pump last turned on
      pumpRunTimer = currentMillis; //set time pump turned on to make sure it doesn't exceed max runtime
      pumpState = true;
    }
  //} else if (((avgPSI > highLimit) || (lowWater = true)) && pumpState == true) || ((currentMillis - pumpRunTimer) > 300000)) {
  } else if ((avgPSI > highLimit) || ((pumpState == true) && (lowWater == true)) || ((currentMillis - pumpRunTimer) > pumpOverRunStop)) {
    digitalWrite(pswRelayPin, HIGH);
    pumpState = false;
  }
  ///END PRESSURE SWITCH
}
