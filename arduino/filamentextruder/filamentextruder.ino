//Include libraries
#include "Nextion.h"
#include <Adafruit_MAX31865.h>
#include <PID_v1.h>

//Define defaults
#define NOMINAL_TEMP 165 //[°C]
#define SPOOL_WIDTH 73 //[mm]
#define SPOOL_PICKUP_DIAMETER 53 //[mm]
#define SPOOL_INNER_DIAMETER 100 //[mm]
#define SPOOL_OUTER_DIAMETER 200 //[mm]

//Define other process parameters
#define UPDATE_INTERVAL 1000 //[ms]
#define ALLOWED_TEMP_DEVIATION 5 //[°C]

//Define temperature sensor pins 
#define MAX31865_CS 45 //Any digital pin
#define MAX31865_DI 47 //Any digital pin
#define MAX31865_DO 49 //Any digital pin
#define MAX31865_CLK 51 //Any digital pin  

//Define heater pins
#define HEATER_1 2 //PWM pin
#define HEATER_2 3 //PWM pin
#define HEATER_3 4 //PWM pin

//Define extruder motor pin 
#define EXTRUDER_MOTOR 53 //Any digital pin 

//Define windup signal pins
#define INIT_SIGNAL 5 //Any digital pin
#define REF_SIGNAL 6 //Any digital pin
#define WINDUP_SIGNAL 7 //Any digital pin

//Define PT100 parameters
#define RREF 430.0
#define RNOMINAL 100.0

//Define PID parameters
#define K_P 150.0
#define K_I 1.8
#define K_D 1080.0

//Declare Extruder FSM states
enum states {initialize, refStep, idle, settings, heatup, ready, extrude, windup};
states currentState = initialize;

//Declare extruder process variables
long updateTime = 0L;
double nominalTemp = NOMINAL_TEMP; //[°C]
double actualTemp = 0.0; //[°C]
int spoolWidth = SPOOL_WIDTH; 
int spoolPickupDiameter = SPOOL_PICKUP_DIAMETER;
int spoolInnerDiameter = SPOOL_INNER_DIAMETER;
int spoolOuterDiameter = SPOOL_OUTER_DIAMETER;
int speedOffset = 0; //[1/min]

//Declare temperature sensor 
Adafruit_MAX31865 pt100(MAX31865_CS, MAX31865_DI, MAX31865_DO, MAX31865_CLK);

//Declare PID output variable
double heaterOutput;
PID pid(&actualTemp, &heaterOutput, &nominalTemp, K_P, K_I, K_D, DIRECT);

//Declare stepper motors and corresponding variables
double windupSpeed = 0.0; //[1/min]
int turnsPerLayer = 0;
int stepsPerSecond = 0; //[1/s]
int currentLayer = -1;

//Declare Nextion objects
//Page 0: init
NexPage initPage(0, 0, "init");
NexPicture init_pInitSensors(0, 6, "pInitSensors");
NexPicture init_pInitPID(0, 7, "pInitPID");
NexPicture init_pInitMotors(0, 8, "pInitMotors");
NexPicture init_pRefStepper(0, 9, "pRefStepper");
//Page 1: idle
NexPage idlePage(1, 0, "idle");
NexButton idle_bHeatUp(1, 3, "bHeatUp");
NexButton idle_bSettings(1, 4, "bSettings");
//Page 2: heatup
NexPage heatupPage(2, 0, "heatup");
NexButton heatup_bCooldown(2, 2, "bCooldown");
NexButton heatup_bStartExt(2, 13, "bStartExt");
NexText heatup_tReady(2, 15, "tReady");
NexButton heatup_bTempMinus5(2, 8, "bTempMinus5");
NexButton heatup_bTempMinus1(2, 10, "bTempMinus1");
NexButton heatup_bTempPlus1(2, 11, "bTempPlus1");
NexButton heatup_bTempPlus5(2, 9, "bTempPlus5");
NexNumber heatup_nNominalTemp(2, 16, "nNominalTemp");
NexNumber heatup_nActualTemp(2, 4, "nActualTemp");
NexProgressBar heatup_jTempBar(2, 3, "jTempBar");
//Page 3: extrude
NexPage extrudePage(3, 0, "extrude");
NexButton extrude_bCooldown(3, 2, "bCooldown");
NexButton extrude_bPauseExt(3, 11, "bPauseExt");
NexButton extrude_bStartWind(3, 10, "bStartWind");
NexButton extrude_bTempMinus1(3, 8, "bTempMinus1");
NexButton extrude_bTempPlus1(3, 9, "bTempPlus1");
NexNumber extrude_nNominalTemp(3, 14, "nNominalTemp");
NexNumber extrude_nActualTemp(3, 4, "nActualTemp");
NexProgressBar extrude_jTempBar(3, 3, "jTempBar");
//Page 4: windup
NexPage windupPage(4, 0, "windup");
NexButton windup_bCooldown(4, 2, "bCooldown");
NexButton windup_bPauseWind(4, 10, "bPauseWind");
NexButton windup_bTempMinus1(4, 8, "bTempMinus1");
NexButton windup_bTempPlus1(4, 9, "bTempPlus1");
NexButton windup_bSpeedMin01(4, 14, "bSpeedMin01");
NexButton windup_bSpeedMin001(4, 15, "bSpeedMin001");
NexButton windup_bSpeedPlus01(4, 12, "bSpeedPlus01");
NexButton windup_bSpeedPlus001(4, 13, "bSpeedPlus001");
NexNumber windup_nNominalTemp(4, 10, "nNominalTemp");
NexText windup_tSpeed(4, 16, "tSpeed");
NexNumber windup_nActualTemp(4, 4, "nActualTemp");
NexProgressBar windup_jTempBar(4, 3, "jTempBar");
//Page 5: settings
NexPage settingsPage(5, 0, "settings");
NexButton settings_bSave(5, 27, "bSave");
NexButton settings_bTempMinus5(5, 2, "bTempMinus5");
NexButton settings_bTempMinus1(5, 4, "bTempMinus1");
NexButton settings_bTempPlus1(5, 5, "bTempPlus1");
NexButton settings_bTempPlus5(5, 3, "bTempPlus5");
NexButton settings_bWidthMinus5(5, 29, "bWidthMinus5");
NexButton settings_bWidthMinus1(5, 30, "bWidthMinus1");
NexButton settings_bWidthPlus1(5, 32, "bWidthPlus1");
NexButton settings_bWidthPlus5(5, 33, "bWidthPlus5");
NexButton settings_bPickupMinus5(5, 9, "bPickupMinus5");
NexButton settings_bPickupMinus1(5, 11, "bPickupMinus1");
NexButton settings_bPickupPlus1(5, 12, "bPickupPlus1");
NexButton settings_bPickupPlus5(5, 10, "bPickupPlus5");
NexButton settings_bInnerMinus5(5, 21, "bInnerMinus5");
NexButton settings_bInnerMinus1(5, 23, "bInnerMinus1");
NexButton settings_bInnerPlus1(5, 24, "bInnerPlus1");
NexButton settings_bInnerPlus5(5, 22, "bInnerPlus5");
NexButton settings_bOuterMinus5(5, 27, "bOuterMinus5");
NexButton settings_bOuterMinus1(5, 29, "bOuterMinus1");
NexButton settings_bOuterPlus1(5, 30, "bOuterPlus1");
NexButton settings_bOuterPlus5(5, 28, "bOuterPlus5");
NexNumber settings_nTemp(5, 7, "nTemp");
NexText settings_tPickup(5, 14, "tPickup");
NexText settings_tWidth(5, 20, "tWidth");
NexText settings_tInnerDia(5, 26, "tInnerDia");
NexText settings_tOuterDia(5, 32, "tOuterDia");

//Create touch event list
NexTouch *nexListenList[] = {
  //Page 1: idle
  &idle_bHeatUp,
  &idle_bSettings,
  //Page 2: heatup
  &heatup_bCooldown,
  &heatup_bStartExt,
  &heatup_tReady,
  &heatup_bTempMinus5,
  &heatup_bTempMinus1,
  &heatup_bTempPlus1,
  &heatup_bTempPlus5,
  //Page 3: extrude
  &extrude_bCooldown,
  &extrude_bPauseExt,
  &extrude_bStartWind,
  &extrude_bTempMinus1,
  &extrude_bTempPlus1,
  //Page 4: windup
  &windup_bCooldown,
  &windup_bPauseWind,
  &windup_bTempMinus1,
  &windup_bTempPlus1,
  &windup_bSpeedMin01,
  &windup_bSpeedMin001,
  &windup_bSpeedPlus01,
  &windup_bSpeedPlus001,
  //Page 5: settings
  &settings_bSave,
  &settings_bTempMinus5,
  &settings_bTempMinus1,
  &settings_bTempPlus1,
  &settings_bTempPlus5,
  &settings_bWidthMinus5,
  &settings_bWidthMinus1,
  &settings_bWidthPlus1,
  &settings_bWidthPlus5,
  &settings_bPickupMinus5,
  &settings_bPickupMinus1,
  &settings_bPickupPlus1,
  &settings_bPickupPlus5,
  &settings_bInnerMinus5,
  &settings_bInnerMinus1,
  &settings_bInnerPlus1,
  &settings_bInnerPlus5,
  &settings_bOuterMinus5,
  &settings_bOuterMinus1,
  &settings_bOuterPlus1,
  &settings_bOuterPlus5,
  NULL
};

//Define button callback functions
//Page 1: idle
void idle_bHeatup_callback() {
  heatupPage.show();
  currentState = heatup;
  heatup_nNominalTemp.setValue((int)nominalTemp);
  heatup_nActualTemp.setValue((int)actualTemp);
  dbSerialPrintln("currentState = heatup");
}
void idle_bSettings_callback() {
  settingsPage.show();
  currentState = settings;
  settings_nTemp.setValue((int)nominalTemp);
  settings_tWidth.setText(String(spoolWidth) + "mm");
  settings_tPickup.setText(String(spoolPickupDiameter) + "mm");
  settings_tInnerDia.setText(String(spoolInnerDiameter) + "mm");
  settings_tOuterDia.setText(String(spoolOuterDiameter) + "mm");
  dbSerialPrintln("currentState = settings");
}
//Page 2: heatup
void heatup_bCooldown_callback() {
  idlePage.show();
  currentState = idle;
  analogWrite(HEATER_1, 0);
  analogWrite(HEATER_2, 0);
  analogWrite(HEATER_3, 0);
  dbSerialPrintln("currentState = idle");
}
void heatup_bStartExt_callback() {
  extrudePage.show();
  currentState = extrude;
  extrude_nNominalTemp.setValue((int)nominalTemp);
  extrude_nActualTemp.setValue((int)actualTemp);
  digitalWrite(EXTRUDER_MOTOR, HIGH);
  dbSerialPrintln("currentState = extrude");
}
void heatup_bTempMinus5_callback() {
  nominalTemp -= 5;
  heatup_nNominalTemp.setValue((int)nominalTemp);
}
void heatup_bTempMinus1_callback() {
  nominalTemp -= 1;
  heatup_nNominalTemp.setValue((int)nominalTemp);
}
void heatup_bTempPlus1_callback() {
  nominalTemp += 1;
  heatup_nNominalTemp.setValue((int)nominalTemp);
}
void heatup_bTempPlus5_callback() {
  nominalTemp += 5;
  heatup_nNominalTemp.setValue((int)nominalTemp);
}
//Page 3: extrude
void extrude_bCooldown_callback() {
  idlePage.show();
  currentState = idle;
  digitalWrite(EXTRUDER_MOTOR, LOW);
  analogWrite(HEATER_1, 0);
  analogWrite(HEATER_2, 0);
  analogWrite(HEATER_3, 0);
  dbSerialPrintln("currentState = idle");
}
void extrude_bPauseExt_callback() {
  heatupPage.show();
  currentState = heatup;
  digitalWrite(EXTRUDER_MOTOR, LOW);
  heatup_nNominalTemp.setValue((int)nominalTemp);
  heatup_nActualTemp.setValue((int)actualTemp);
  dbSerialPrintln("currentState = heatup");
}
void extrude_bStartWind_callback() {
  windupPage.show();
  startWindup();
  currentState = windup;
  windup_tSpeed.setText(String(windupSpeed).substring(0, 4) + " RPM");
  windup_nNominalTemp.setValue((int)nominalTemp);
  windup_nActualTemp.setValue((int)actualTemp);
  dbSerialPrintln("currentState = windup");
}
void extrude_bTempMinus1_callback() {
  nominalTemp -= 1;
  extrude_nNominalTemp.setValue((int)nominalTemp);
}
void extrude_bTempPlus1_callback() {
  nominalTemp += 1;
  extrude_nNominalTemp.setValue((int)nominalTemp);
}
//Page 4: windup
void windup_bCooldown_callback() {
  idlePage.show();
  stopWindup();
  currentState = idle;
  digitalWrite(EXTRUDER_MOTOR, LOW);
  analogWrite(HEATER_1, 0);
  analogWrite(HEATER_2, 0);
  analogWrite(HEATER_3, 0);
  dbSerialPrintln("currentState = idle");
}
void windup_bPauseWind_callback() {
  extrudePage.show();
  stopWindup();
  currentState = extrude;
  extrude_nNominalTemp.setValue((int)nominalTemp);
  extrude_nActualTemp.setValue((int)actualTemp);
  dbSerialPrintln("currentState = extrude");
}
void windup_bTempMinus1_callback() {
  nominalTemp -= 1;
  windup_nNominalTemp.setValue((int)nominalTemp);
}
void windup_bTempPlus1_callback() {
  nominalTemp += 1;
  windup_nNominalTemp.setValue((int)nominalTemp);
}
void windup_bSpeedMin01_callback() {
  speedOffset -= 10;
  startWindup();
}
void windup_bSpeedMin001_callback() {
  speedOffset -= 1;
  startWindup();
}
void windup_bSpeedPlus01_callback() {
  speedOffset += 10;
  startWindup();
}
void windup_bSpeedPlus001_callback() {
  speedOffset += 1;
  startWindup();
}
//Page 5: settings
void settings_bSave_callback() {
  idlePage.show();
  currentState = idle;
  dbSerialPrintln("currentState = idle");
}
void settings_bTempMinus5_callback() {
  nominalTemp -= 5;
  settings_nTemp.setValue((int)nominalTemp);
}
void settings_bTempMinus1_callback() {
  nominalTemp -= 1;
  settings_nTemp.setValue((int)nominalTemp);
}
void settings_bTempPlus1_callback() {
  nominalTemp += 1;
  settings_nTemp.setValue((int)nominalTemp);
}
void settings_bTempPlus5_callback() {
  nominalTemp += 5;
  settings_nTemp.setValue((int)nominalTemp);
}
void settings_bWidthMinus5_callback() {
  spoolWidth -= 5;
  settings_tWidth.setText(String(spoolWidth) + "mm");
}
void settings_bWidthMinus1_callback() {
  spoolWidth -= 1;
  settings_tWidth.setText(String(spoolWidth) + "mm");
}
void settings_bWidthPlus1_callback() {
  spoolWidth += 1;
  settings_tWidth.setText(String(spoolWidth) + "mm");
}
void settings_bWidthPlus5_callback() {
  spoolWidth += 5;
  settings_tWidth.setText(String(spoolWidth) + "mm");
}
void settings_bPickupMinus5_callback() {
  spoolPickupDiameter -= 5;
  settings_tPickup.setText(String(spoolPickupDiameter) + "mm");
}
void settings_bPickupMinus1_callback() {
  spoolPickupDiameter -= 1;
  settings_tPickup.setText(String(spoolPickupDiameter) + "mm");
}
void settings_bPickupPlus1_callback() {
  spoolPickupDiameter += 1;
  settings_tPickup.setText(String(spoolPickupDiameter) + "mm");
}
void settings_bPickupPlus5_callback() {
  spoolPickupDiameter += 5;
  settings_tPickup.setText(String(spoolPickupDiameter) + "mm");
}
void settings_bInnerMinus5_callback() {
  spoolInnerDiameter -= 5;
  settings_tInnerDia.setText(String(spoolInnerDiameter) + "mm");
}
void settings_bInnerMinus1_callback() {
  spoolInnerDiameter -= 1;
  settings_tInnerDia.setText(String(spoolInnerDiameter) + "mm");
}
void settings_bInnerPlus1_callback() {
  spoolInnerDiameter += 1;
  settings_tInnerDia.setText(String(spoolInnerDiameter) + "mm");
}
void settings_bInnerPlus5_callback() {
  spoolInnerDiameter += 5;
  settings_tInnerDia.setText(String(spoolInnerDiameter) + "mm");
}
void settings_bOuterMinus5_callback() {
  spoolOuterDiameter -= 5;
  settings_tOuterDia.setText(String(spoolOuterDiameter) + "mm");
}
void settings_bOuterMinus1_callback() {
  spoolOuterDiameter -= 1;
  settings_tOuterDia.setText(String(spoolOuterDiameter) + "mm");
}
void settings_bOuterPlus1_callback() {
  spoolOuterDiameter += 1;
  settings_tOuterDia.setText(String(spoolOuterDiameter) + "mm");
}
void settings_bOuterPlus5_callback() {
  spoolOuterDiameter += 5;
  settings_tOuterDia.setText(String(spoolOuterDiameter) + "mm");
}

//Define init functions
void initSensors() {
  pt100.begin(MAX31865_2WIRE);
}
void initPID() {
  pinMode(HEATER_1, OUTPUT);
  pinMode(HEATER_2, OUTPUT);
  pinMode(HEATER_3, OUTPUT);
  pid.SetMode(AUTOMATIC);
}
void initExtruderMotor() {
  pinMode(EXTRUDER_MOTOR, OUTPUT);
}
void initWindup() {
  pinMode(INIT_SIGNAL, INPUT);
  pinMode(REF_SIGNAL, INPUT);
  pinMode(WINDUP_SIGNAL, OUTPUT);
}

//Define windup functions
void startWindup() {
  //String architecture: "SPOOL_WIDTH;SPOOL_PICKUP_DIAMETER;SPOOL_INNER_DIAMETER;SPOOL_OUTER_DIAMETER;"
  Serial1.print(String(spoolWidth) + ";"); 
  Serial1.print(String(spoolPickupDiameter) + ";");
  Serial1.print(String(spoolInnerDiameter) + ";");
  Serial1.print(String(spoolOuterDiameter) + ";");
  Serial1.print(String(speedOffset) + ";");
  delay(100);
  digitalWrite(WINDUP_SIGNAL, HIGH);
}
void stopWindup() {
  digitalWrite(WINDUP_SIGNAL, LOW);
}

//Define state machine evaluations
void evalStates() {
  switch(currentState) {
    case initialize:
      dbSerialPrintln("currentState = init");
      initPage.show();
      initSensors();
      init_pInitSensors.show();
      initPID();
      init_pInitPID.show();
      initExtruderMotor();
      initWindup();
      while(digitalRead(INIT_SIGNAL) == LOW) {}
      init_pInitMotors.show();
      currentState = refStep;
      dbSerialPrintln("currentState = refStep");
      break;
    case refStep:
      while(digitalRead(REF_SIGNAL) == LOW) {}
      init_pRefStepper.show();
      delay(500);
      idlePage.show();
      currentState = idle;
      dbSerialPrintln("currentState = idle");
      break;
    case idle:
      break;
    case settings:
      break;
    case heatup:
      actualTemp = pt100.temperature(RNOMINAL, RREF);
      if(millis() >= updateTime + UPDATE_INTERVAL) {
        heatup_nActualTemp.setValue((int)actualTemp);
        heatup_jTempBar.setValue(actualTemp / nominalTemp * 100);
        updateTime = millis();
      }
      pid.Compute();
      analogWrite(HEATER_1, heaterOutput);
      analogWrite(HEATER_2, heaterOutput);
      analogWrite(HEATER_3, heaterOutput);
      if(actualTemp >= nominalTemp - ALLOWED_TEMP_DEVIATION && actualTemp <= nominalTemp + ALLOWED_TEMP_DEVIATION) {
        heatup_bStartExt.show();
        heatup_tReady.show();
        currentState = ready;
        dbSerialPrintln("currentState = ready");
      }
      break;
    case ready:
      actualTemp = pt100.temperature(RNOMINAL, RREF);
      if(millis() >= updateTime + UPDATE_INTERVAL) {
        heatup_nActualTemp.setValue((int)actualTemp);
        heatup_jTempBar.setValue(actualTemp / nominalTemp * 100);
        updateTime = millis();
      }
      pid.Compute();
      analogWrite(HEATER_1, heaterOutput);
      analogWrite(HEATER_2, heaterOutput);
      analogWrite(HEATER_3, heaterOutput);
      if(actualTemp > nominalTemp + ALLOWED_TEMP_DEVIATION || actualTemp < nominalTemp - ALLOWED_TEMP_DEVIATION) {
        heatup_bStartExt.hide();
        heatup_tReady.hide();
        currentState = heatup;
        heatup_nNominalTemp.setValue((int)nominalTemp);
        heatup_nActualTemp.setValue((int)actualTemp);
        dbSerialPrintln("currentState = heatup");
      }
      break;
    case extrude:
      actualTemp = pt100.temperature(RNOMINAL, RREF);
      if(millis() >= updateTime + UPDATE_INTERVAL) {
        extrude_nActualTemp.setValue((int)actualTemp);
        extrude_jTempBar.setValue(actualTemp / nominalTemp * 100);
        updateTime = millis();
      }
      pid.Compute();
      analogWrite(HEATER_1, heaterOutput);
      analogWrite(HEATER_2, heaterOutput);
      analogWrite(HEATER_3, heaterOutput);
      if(actualTemp > nominalTemp + ALLOWED_TEMP_DEVIATION || actualTemp < nominalTemp - ALLOWED_TEMP_DEVIATION) {
        heatup_bStartExt.hide();
        heatup_tReady.hide();
        currentState = heatup;
        digitalWrite(EXTRUDER_MOTOR, LOW);
        heatup_nNominalTemp.setValue((int)nominalTemp);
        heatup_nActualTemp.setValue((int)actualTemp);
        dbSerialPrintln("currentState = heatup");
      }
      break;
    case windup:
      actualTemp = pt100.temperature(RNOMINAL, RREF);
      if(millis() >= updateTime + UPDATE_INTERVAL) {
        windup_nActualTemp.setValue((int)actualTemp);
        windup_jTempBar.setValue(actualTemp / nominalTemp * 100);
        updateTime = millis();
      }
      pid.Compute();
      analogWrite(HEATER_1, heaterOutput);
      analogWrite(HEATER_2, heaterOutput);
      analogWrite(HEATER_3, heaterOutput);
      if(Serial1.available()) {
        windupSpeed = Serial1.readStringUntil(';').toFloat();
        windup_tSpeed.setText(String(windupSpeed).substring(0, 4) + " RPM");
      }
      if(actualTemp > nominalTemp + ALLOWED_TEMP_DEVIATION || actualTemp < nominalTemp - ALLOWED_TEMP_DEVIATION) {
        heatup_bStartExt.hide();
        heatup_tReady.hide();
        stopWindup();
        currentState = heatup;
        digitalWrite(EXTRUDER_MOTOR, LOW);
        heatup_nNominalTemp.setValue((int)nominalTemp);
        heatup_nActualTemp.setValue((int)actualTemp);
        dbSerialPrintln("currentState = heatup");
      }
      break;
    default:
      break;
  }
}

void setup() {
  //Initialize display
  nexInit();

  //Open debug serial 
  Serial.begin(9600);

  //Open windup serial
  Serial1.begin(9600);

  //Register callback functions
  //Page 1: idle
  idle_bHeatUp.attachPop(idle_bHeatup_callback);
  idle_bSettings.attachPop(idle_bSettings_callback);
  //Page 2: heatup
  heatup_bCooldown.attachPop(heatup_bCooldown_callback);
  heatup_bStartExt.attachPop(heatup_bStartExt_callback);
  heatup_bTempMinus5.attachPop(heatup_bTempMinus5_callback);
  heatup_bTempMinus1.attachPop(heatup_bTempMinus1_callback);
  heatup_bTempPlus1.attachPop(heatup_bTempPlus1_callback);
  heatup_bTempPlus5.attachPop(heatup_bTempPlus5_callback);
  //Page 3: extrude
  extrude_bCooldown.attachPop(extrude_bCooldown_callback);
  extrude_bPauseExt.attachPop(extrude_bPauseExt_callback);
  extrude_bStartWind.attachPop(extrude_bStartWind_callback);
  extrude_bTempMinus1.attachPop(extrude_bTempMinus1_callback);
  extrude_bTempPlus1.attachPop(extrude_bTempPlus1_callback);
  //Page 4: windup
  windup_bCooldown.attachPop(windup_bCooldown_callback);
  windup_bPauseWind.attachPop(windup_bPauseWind_callback);
  windup_bTempMinus1.attachPop(windup_bTempMinus1_callback);
  windup_bTempPlus1.attachPop(windup_bTempPlus1_callback);
  windup_bSpeedMin01.attachPop(windup_bSpeedMin01_callback);
  windup_bSpeedMin001.attachPop(windup_bSpeedMin001_callback);
  windup_bSpeedPlus01.attachPop(windup_bSpeedPlus01_callback);
  windup_bSpeedPlus001.attachPop(windup_bSpeedPlus001_callback);
  //Page 5: settings
  settings_bSave.attachPop(settings_bSave_callback);
  settings_bTempMinus5.attachPop(settings_bTempMinus5_callback);
  settings_bTempMinus1.attachPop(settings_bTempMinus1_callback);
  settings_bTempPlus1.attachPop(settings_bTempPlus1_callback);
  settings_bTempPlus5.attachPop(settings_bTempPlus5_callback);
  settings_bWidthMinus5.attachPop(settings_bWidthMinus5_callback);
  settings_bWidthMinus1.attachPop(settings_bWidthMinus1_callback);
  settings_bWidthPlus1.attachPop(settings_bWidthPlus1_callback);
  settings_bWidthPlus5.attachPop(settings_bWidthPlus5_callback);
  settings_bPickupMinus5.attachPop(settings_bPickupMinus5_callback);
  settings_bInnerMinus5.attachPop(settings_bInnerMinus5_callback);
  settings_bInnerMinus1.attachPop(settings_bInnerMinus1_callback);
  settings_bInnerPlus1.attachPop(settings_bInnerPlus1_callback);
  settings_bInnerPlus5.attachPop(settings_bInnerPlus5_callback);
  settings_bOuterMinus5.attachPop(settings_bOuterMinus5_callback);
  settings_bOuterMinus1.attachPop(settings_bOuterMinus1_callback);
  settings_bOuterPlus1.attachPop(settings_bOuterPlus1_callback);
  settings_bOuterPlus5.attachPop(settings_bOuterPlus5_callback);
}

void loop() {
  evalStates();
  nexLoop(nexListenList);
}
