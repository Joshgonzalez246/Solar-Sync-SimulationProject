/**************************************************
Motor_Controller_Script_V2

Author: Charles Kipping

Date: 08/10/2025

Description: 2nd attempt at creating a motor control script using new approach
Old motor driver used due U/S new one
**************************************************/

#include <Wire.h>

// Pins:
#define ENA_PIN 2
#define IN1_PIN 5
#define IN2_PIN 6
#define EXT_PWR_PIN 8
#define PANEL_ANGLE_SENSOR_PIN A0

// Constants:
#define PANEL_MIN_ANGLE 45
#define PANEL_MAX_ANGLE 135
#define PANEL_SENSOR_MIN 150 // ADC Value (for tuning)
#define PANEL_SENSOR_MAX 750 // ADC Value (for tuning)
#define PANEL_SENSITIVITY_ANGLE 5
#define PANEL_BUFFER_ANGLE 10
#define MOTOR_FULL_SPEED 160
#define MOTOR_HALF_SPEED 155

#define SLAVE_ADDRESS 8

// Global Variables:
bool motorOn = false;
bool motorReverse = false;
int motorSpeed = 0;

int panelAngle = 0;
int panelADC = 0;
int refAngle = 90;
int errorAngle = 0;

volatile int receivedAngle = 0;

void setup() {
  // Pinmode:
  pinMode(ENA_PIN, OUTPUT);
  pinMode(IN1_PIN, OUTPUT);
  pinMode(IN2_PIN, OUTPUT);
  pinMode(PANEL_ANGLE_SENSOR_PIN, INPUT);
  pinMode(EXT_PWR_PIN, OUTPUT);
  digitalWrite(EXT_PWR_PIN, HIGH);

  // Set 0:
  driveMotor(0, 0, 0);

  // Debug
  Serial.begin(9600);

  // I2C:
  Wire.begin(SLAVE_ADDRESS);
  Wire.onReceive(recieveData);

}

void loop() {

  getAngleData();
  motorController();

}

void recieveData(int numBytes){

  if (numBytes >= 2) {
    byte high = Wire.read();
    byte low = Wire.read();
    receivedAngle = word(high, low); // Reconstruct int
  }

  //Serial.println(receivedAngle);

}

void motorController(void){
  // Determine if panel is within target bounds:
  if(errorAngle < PANEL_SENSITIVITY_ANGLE && errorAngle > -PANEL_SENSITIVITY_ANGLE){
    // Within Bounds - Stop motor:
    motorOn = false;
    //Serial.println("STOP");
  }

  // Outside of bounds:
  else{
    // Determine if panel is close to bounds (within Buffer)
    if(errorAngle < PANEL_BUFFER_ANGLE && errorAngle > -PANEL_BUFFER_ANGLE){
      // Run at reduced speed:
      motorSpeed = MOTOR_HALF_SPEED;
      //Serial.println("HALF");
    }

    else{
      // Panel needs to move significant distance:
      motorSpeed = MOTOR_FULL_SPEED;
      //Serial.println("FULL");
    }

    // Determine direction of motor:
    if(errorAngle > 0){
      // Positive Error:
      motorReverse = false;
    }
    else{
      // Negative Error:
      motorReverse = true;
    }

    motorOn = true;

  }

  driveMotor(motorOn, motorReverse, motorSpeed);

}

// 155 to start then go to 150
void driveMotor(bool enable, bool reverse, int speed){

  digitalWrite(ENA_PIN, enable);

  if(reverse == true){
    analogWrite(IN1_PIN, speed);
    analogWrite(IN2_PIN, 0);
  }
  else{
    analogWrite(IN1_PIN, 0);
    analogWrite(IN2_PIN, speed);
  }

}

void getAngleData(void){
  panelADC = analogRead(PANEL_ANGLE_SENSOR_PIN);
  panelAngle = map(panelADC, PANEL_SENSOR_MIN, PANEL_SENSOR_MAX, PANEL_MIN_ANGLE, PANEL_MAX_ANGLE);

  // Get Reference Angle: TO DO!!!!
  refAngle = receivedAngle;

  // Error Value:
  errorAngle = refAngle - panelAngle;
  
  getAngleData_debug();
}

void getAngleData_debug(){
  Serial.print("ADC: ");
  Serial.print(panelADC);
  Serial.print(" Ang: ");
  Serial.print(panelAngle);
  Serial.print(" Ref: ");
  Serial.print(refAngle);
  Serial.print(" Err: ");
  Serial.println(errorAngle);
}





void motorTesting(void){
  motorOn = true;
  motorReverse = false;
  motorSpeed = 150;
  driveMotor(motorOn, motorReverse, motorSpeed);
  

  while(1);
}


