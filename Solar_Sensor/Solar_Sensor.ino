/*************************************************
Author: Charles Kipping
Date: 08/10/2025

Description:
Prototype single axis solar tracking
Outputs angle to perpendicular sun
Updated to send output angle via I2C
*************************************************/
// Libraries:
#include <Servo.h>
#include <Wire.h>

// LDR Input Pins:
#define LDR_COARSE_PIN A0
#define LDR_FINE_PIN A1

// LED Status and UI Pins:
#define MOVEMENT_LED_PIN 2
#define LOCKED_LED_PIN 13
#define ERROR_LED_PIN 4
#define HALT_SYSTEM_PB_PIN 7

// Servo
#define EAST_WEST_SERVO_PIN 9
Servo eastWestServo;

// Fixed Thresholdes (Tune):
#define ADC_SENSITIVITY_MASK 0b1111111111

#define SERVO_ANGLE_MIN 45
#define SERVO_ANGLE_MAX 135
#define SERVO_ANGLE_STEP_SIZE 5
#define SERVO_ANGLE_STEP_DELAY 200 //[ms]

#define ACTIVE_TRACK_DELAY 50 //[ms]
#define DIFF_BUFFER 100 

#define SLAVE_ADDRESS 8

// Global Vaiables:
int measuredCoarseVal = 0;
int measuredFineVal = 0;
int LDRDiffVal = 0;

int maxLDRDiffVal = 0;
int maxLDRDiffAng = SERVO_ANGLE_MIN;

int eastWestServoAngle = 90; // [Degrees]
int prevEastWestServoAngle = 90;

bool resetSystem = true;

// UI Global Variables;
bool systemHalt = false;
bool systemHaltPbVal = true;
bool movementLEDState = false;
bool lockedLEDState = false;
bool errorLEDState = false;

void setup() {
  // Set Port Impedance:
  pinMode(LDR_COARSE_PIN, INPUT);
  pinMode(LDR_FINE_PIN, INPUT);
  pinMode(HALT_SYSTEM_PB_PIN, INPUT_PULLUP);

  pinMode(MOVEMENT_LED_PIN, OUTPUT);
  pinMode(LOCKED_LED_PIN, OUTPUT);
  pinMode(ERROR_LED_PIN, OUTPUT);

  pinMode(EAST_WEST_SERVO_PIN, OUTPUT);

  // Set up Servo:
  eastWestServo.attach(EAST_WEST_SERVO_PIN);

  // Debug Serial Coms:
  Serial.begin(9600);

  // Tx data
  Wire.begin();
}

void loop() {

  if(resetSystem == true){
    systemReset();
  }

  activeTrack();

  if(eastWestServoAngle != prevEastWestServoAngle){

    sendData();

    prevEastWestServoAngle = eastWestServoAngle;
  }

  //sendData();
  //Serial.println(eastWestServoAngle);

  //debug();

  delay(100);

  // Stop Tracking:
  while(systemHalt == true){
    //Set UI:
    movementLEDState = false;
    lockedLEDState = false;
    errorLEDState = true;
    setUI();
    delay(10);
  }

}

void sendData(void){
  Wire.beginTransmission(SLAVE_ADDRESS);

  // Send int as two bytes (MSB first)
  Wire.write(highByte(eastWestServoAngle));
  Wire.write(lowByte(eastWestServoAngle));

  Wire.endTransmission();
}

/*************************************************
Used for debugging and printing values to terminal
*************************************************/
void debug(void){
  Serial.print(measuredCoarseVal);
  Serial.print("\t");
  Serial.print(measuredFineVal);
  Serial.print("\t");
  Serial.print(LDRDiffVal);
  Serial.print("\t");
  Serial.print(eastWestServoAngle);
  Serial.print("\t");
  Serial.print("\t");
  Serial.print(maxLDRDiffVal);
  Serial.print("\t");
  Serial.println(maxLDRDiffAng);
  
}

/*************************************************
Initialize System - Reset sensor and initilise:
*************************************************/
void systemReset(void){
  // Reset all global variables:
  measuredCoarseVal = 0;
  measuredFineVal = 0;
  LDRDiffVal = 0;

  maxLDRDiffVal = 0;
  maxLDRDiffAng = SERVO_ANGLE_MIN;

  eastWestServoAngle = 90; // [Degrees]

  resetSystem = false;

  //Set UI:
  movementLEDState = true;
  lockedLEDState = false;
  errorLEDState = false;
  setUI();

  // Set servo config position for 3s:
  driveServo();
  delay(3000);

  // Peform Sweep:
  sweepMode();

}

/*************************************************
Used to control angle of servo
*************************************************/
void driveServo(void){
  eastWestServo.write(eastWestServoAngle);
}

/*************************************************
First step to determine approximate sun location (coarse)

From min angle to max angle, record coarse value and save max
*************************************************/
void sweepMode(void){
  eastWestServoAngle = SERVO_ANGLE_MIN;
  int noOfPositions = (SERVO_ANGLE_MAX - SERVO_ANGLE_MIN) / SERVO_ANGLE_STEP_SIZE;

  //Reset max values:
  maxLDRDiffVal = 0;
  maxLDRDiffAng = SERVO_ANGLE_MIN;

  // Poll coarse LDR at Step Size angles
  for(int i = 0; i <= noOfPositions; i++){
    // Check angle is within bounds
    if((eastWestServoAngle >= SERVO_ANGLE_MIN) || (eastWestServoAngle <= SERVO_ANGLE_MAX)){
      // Within bounds!
      driveServo();

      // Measure:
      readLDRs();
      recordMaxDiffVal();

      // Increment Testing Angle:
      eastWestServoAngle = eastWestServoAngle + SERVO_ANGLE_STEP_SIZE;
      delay(SERVO_ANGLE_STEP_DELAY);
    }
    else{
      // Terminate for loop
      i = noOfPositions + 2;
    }
  }

  // Point towards the angle of highest light:
  eastWestServoAngle = maxLDRDiffAng;
  driveServo();

  // Set UI:
  movementLEDState = false;
  lockedLEDState = true;
  errorLEDState = false;
  setUI();

}

/*************************************************
Tracks sun by ensuring sufficient ADC difference between LDR's
*************************************************/
void activeTrack(void){
  delay(ACTIVE_TRACK_DELAY);

  readLDRs();

  // Determine if LDR difference is below threshold:
  if(LDRDiffVal < (maxLDRDiffVal - DIFF_BUFFER)){
    // Check angle slightly West:
    int prevLDRDiffVal = LDRDiffVal;
    eastWestServoAngle = eastWestServoAngle + SERVO_ANGLE_STEP_SIZE;
    driveServo();
    delay(SERVO_ANGLE_STEP_DELAY);

    readLDRs();

    // If increased, find max LDR difference:
    if(LDRDiffVal > prevLDRDiffVal){
      while(LDRDiffVal > prevLDRDiffVal){
        prevLDRDiffVal = LDRDiffVal;
        eastWestServoAngle = eastWestServoAngle + SERVO_ANGLE_STEP_SIZE;
        driveServo();
        delay(SERVO_ANGLE_STEP_DELAY);

        readLDRs();
      }
      // Go back 1 step to get max LDR difference:
      eastWestServoAngle = eastWestServoAngle - SERVO_ANGLE_STEP_SIZE;
      driveServo();
    }

    // If not increased, find max LDR difference in the opposite direction:
    else{
      // Go back 2 steps and see if LDR difference increase:
      eastWestServoAngle = eastWestServoAngle - (2*SERVO_ANGLE_STEP_SIZE);
      driveServo();
      delay(SERVO_ANGLE_STEP_DELAY);

      readLDRs();

      // LDR Difference increases - Track as above in reverse direction:
      if(LDRDiffVal > prevLDRDiffVal){
        while(LDRDiffVal > prevLDRDiffVal){
          prevLDRDiffVal = LDRDiffVal;
          eastWestServoAngle = eastWestServoAngle - SERVO_ANGLE_STEP_SIZE;
          driveServo();
          delay(SERVO_ANGLE_STEP_DELAY);

          readLDRs();
        }
        // Go back 1 step to get max LDR difference:
        eastWestServoAngle = eastWestServoAngle + SERVO_ANGLE_STEP_SIZE;
        driveServo();
      }

      // LDR Difference did not increase - Tracking Lost - RESET SYSTEM!
      else{
        resetSystem = true;
      }
    }
  }
}

/*************************************************
Measurers ADC value of LDR's and applies the sensitivity mask
*************************************************/
void readLDRs(void){
  measuredCoarseVal = analogRead(LDR_COARSE_PIN) & ADC_SENSITIVITY_MASK;
  measuredFineVal = analogRead(LDR_FINE_PIN) & ADC_SENSITIVITY_MASK;
  LDRDiffVal = measuredCoarseVal - measuredFineVal;
}

/*************************************************
When called, checks to find the greatest difference bwteen course and fine LDR!
*************************************************/
void recordMaxDiffVal(void){
  if(maxLDRDiffVal < LDRDiffVal){
    maxLDRDiffVal = LDRDiffVal;
    maxLDRDiffAng = eastWestServoAngle;
  }
}

/*************************************************
Reads user button and displays LED outputs
*************************************************/
void setUI(void){
  bool haltState = digitalRead(HALT_SYSTEM_PB_PIN);

  if(haltState == false && systemHaltPbVal == true){
    systemHalt = !systemHalt;
  }
  systemHaltPbVal = haltState;

  // LED's
  digitalWrite(MOVEMENT_LED_PIN, movementLEDState);
  digitalWrite(LOCKED_LED_PIN, lockedLEDState);
  digitalWrite(ERROR_LED_PIN, errorLEDState);
}
