//Include libraries
#include "Nextion.h"

//Define defaults
#define NOMINAL_TEMP 165
#define WINDUP_SPEED 3.00
#define SPOOL_WIDTH 70
#define SPOOL_INNER_DIAMETER 80
#define SPOOL_OUTER_DIAMETER 200

//Declare Extruder FSM states
enum states {initialize, refStep, idle, heatup, ready, extrude, windUp};
states currentState = initialize;

//Declare extruder process variables
int nominalTemp = NOMINAL_TEMP;
int actualTemp = 0;
float windUpSpeed = WINDUP_SPEED;
int spoolWidth = SPOOL_WIDTH;
int spoolInnerDiameter = SPOOL_INNER_DIAMETER;
int spoolOuterDiameter = SPOOL_OUTER_DIAMETER;

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
NexButton heatup_bStartExt(2, 14, "bStartExt");
NexButton heatup_bTempMinus5(2, 8, "bTempMinus5");
NexButton heatup_bTempMinus1(2, 11, "bTempMinus1");
NexButton heatup_bTempPlus1(2, 12, "bTempPlus1");
NexButton heatup_bTempPlus5(2, 9, "bTempPlus5");
NexText heatup_tTemp(2, 10, "tTemp");
NexNumber heatup_nActualTemp(2, 4, "nActualTemp");
NexProgressBar heatup_jTempBar(2, 3, "jTempBar");
//Page 3: extrude
NexPage extrudePage(3, 0, "extrude");
NexButton extrude_bCooldown(3, 2, "bCooldown");
NexButton extrude_bPauseExt(3, 12, "bPauseExt");
NexButton extrude_bStartWind(3, 11, "bStartWind");
NexButton extrude_bTempMinus1(3, 8, "bTempMinus1");
NexButton extrude_bTempPlus1(3, 9, "bTempPlus1");
NexText extrude_tTemp(3, 10, "tTemp");
NexNumber extrude_nActualTemp(3, 4, "nActualTemp");
NexProgressBar extrude_jTempBar(3, 3, "jTempBar");
//Page 4: windup
NexPage windupPage(4, 0, "windup");
NexButton windup_bCooldown(4, 2, "bCooldown");
NexButton windup_bPauseWind(4, 11, "bPauseWind");
NexButton windup_bTempMinus1(4, 8, "bTempMinus1");
NexButton windup_bTempPlus1(4, 9, "bTempPlus1");
NexButton windup_bSpeedMin01(4, 15, "bSpeedMin01");
NexButton windup_bSpeedMin001(4, 16, "bSpeedMin001");
NexButton windup_bSpeedPlus01(4, 13, "bSpeedPlus01");
NexButton windup_bSpeedPlus001(4, 14, "bSpeedPlus001");
NexText windup_tTemp(4, 10, "tTemp");
NexText windup_tSpeed(4, 17, "tSpeed");
NexNumber windup_nActualTemp(4, 4, "nActualTemp");
NexProgressBar windup_jTempBar(4, 3, "jTempBar");

//Create touch event list
NexTouch *nexListenList[] = {
  //Page 1: idle
  &idle_bHeatUp,
  &idle_bSettings,
  //Page 2: heatup
  &heatup_bCooldown,
  &heatup_bStartExt,
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
  NULL
};

//Define button callback functions
//Page 1: idle
void idle_bHeatup_callback() {
  heatupPage.show();
  currentState = heatup;
}
void idle_bSettings_callback() {
  //show settings page...
}
//Page 2: heatup
void heatup_bCooldown_callback() {
  idlePage.show();
  currentState = idle;
}
void heatup_bStartExt_callback() {
  extrudePage.show();
  currentState = extrude;
}
void heatup_bTempMinus5_callback() {
  nominalTemp -= 5;
  heatup_tTemp.setText(tempText(nominalTemp));
}
void heatup_bTempMinus1_callback() {
  nominalTemp -= 1;
  heatup_tTemp.setText(tempText(nominalTemp));
}
void heatup_bTempPlus1_callback() {
  nominalTemp += 1;
  heatup_tTemp.setText(tempText(nominalTemp));
}
void heatup_bTempPlus5_callback() {
  nominalTemp += 5;
  heatup_tTemp.setText(tempText(nominalTemp));
}
//Page 3: extrude
void extrude_bCooldown_callback() {
  idlePage.show();
  currentState = idle;
}
void extrude_bPauseExt_callback() {
  heatupPage.show();
  currentState = heatup;
}
void extrude_bStartWind_callback() {
  windupPage.show();
  currentState = windUp;
}
void extrude_bTempMinus1_callback() {
  nominalTemp -= 1;
  extrude_tTemp.setText(tempText(nominalTemp));
}
void extrude_bTempPlus1_callback() {
  nominalTemp += 1;
  extrude_tTemp.setText(tempText(nominalTemp));
}
//Page 4: windup
void windup_bCooldown_callback() {
  idlePage.show();
  currentState = idle;
}
void windup_bPauseWind_callback() {
  extrudePage.show();
  currentState = extrude;
}
void windup_bTempMinus1_callback() {
  nominalTemp -= 1;
  windup_tTemp.setText(tempText(nominalTemp));
}
void windup_bTempPlus1_callback() {
  nominalTemp += 1;
  windup_tTemp.setText(tempText(nominalTemp));
}
void windup_bSpeedMin01_callback() {
  windUpSpeed -= 0.1;
  windup_tSpeed.setText(speedText(windUpSpeed));
}
void windup_bSpeedMin001_callback() {
  windUpSpeed -= 0.01;
  windup_tSpeed.setText(speedText(windUpSpeed));
}
void windup_bSpeedPlus01_callback() {
  windUpSpeed += 0.1;
  windup_tSpeed.setText(speedText(windUpSpeed));
}
void windup_bSpeedPlus001_callback() {
  windUpSpeed += 0.01;
  windup_tSpeed.setText(speedText(windUpSpeed));
}

//Define state machine evaluations
void evalStates() {
  switch(currentState) {
    case initialize:
      delay(500);
      init_pInitSensors.show();
      delay(600);
      init_pInitPID.show();
      delay(400);
      init_pInitMotors.show();
      currentState = refStep;
      break;
    case refStep:
      delay(2000);
      init_pRefStepper.show();
      delay(500);
      currentState = idle;
      break;
    case idle:
      break;
    case heatup:
      if(actualTemp == nominalTemp) {
        heatup_bStartExt.show();
        currentState = ready;
      }
      break;
    case ready:
      break;
    case extrude:
      break;
    case windUp:
      break;
    default:
      break;
  }
}

//Define text transform functions
char* tempText(int temp) {
  char* displayText;
  (String(temp) + " °C").toCharArray(displayText, (temp > 99) ? 6 : 5);
  return displayText;
}
char* speedText(float speed) {
  char* displayText;
  (String(speed).substring(0, 3) + " RPM").toCharArray(displayText, 8);
  return displayText;
}

void setup() {
  //Initialize display
  nexInit();

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
}

void loop() {
  evalStates();
  nexLoop(nexListenList);
}
