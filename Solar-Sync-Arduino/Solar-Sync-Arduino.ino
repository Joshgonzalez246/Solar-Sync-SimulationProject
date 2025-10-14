// -----------------------------------------------------------------------------
// ASSTA Hybrid MPPT (Algorithm 4, current-zone clamp)
// -----------------------------------------------------------------------------
// Inputs:
//   A0 -> PV input voltage sensor (82k/18k divider)
//   A1 -> PV current sensor (ACS712-30A, 66 mV/A)
//   A2 -> Buck output voltage sensor (82k/18k divider)
// Output:
//   D9 -> 50 kHz PWM duty to DC/DC converter (Timer1, ICR1 = 319)
// Display:
//   I2C OLED 128x64 (address 0x3D)
// -----------------------------------------------------------------------------
// Control tick: 1 ms (1 kHz)
// PWM frequency: 50.00 kHz (Timer1, Fast PWM mode 14, prescaler = 1)
// -----------------------------------------------------------------------------

#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
#define OLED_ADDR 0x3D
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// ---- MPPT tunables ----
const float C        = 0.003;
const float Z        = 0.02;
const float SF       = 0.10;
const float Dmin     = 0.00;
const float Dmax     = 0.95;
const float Dinit    = 0.50;

const float EPS_DV   = 1e-4;
const float EPS_TH   = 1e-3;
const float EPS_M    = 1e-5;

const float Ts_ctrl  = 0.001;     // 1 ms
const float SLEW_RATE = 5.0;      // max duty change per second
const float DSTEPMAX  = SLEW_RATE * Ts_ctrl;

const float ALPHA     = 0.20;
const float IZONE_PCT = 0.03;
const float ALPHA_IMP = 0.05;

// ---- Persistent states ----
float Vold, Iold, Pold, Dold;
float Vfil, Ifil, Imp_hat;
float Vout_fil = 0;
bool init_done = false;

// ---- Hardware pins ----
const int pinVpv  = A0;  // PV input voltage
const int pinIpv  = A1;  // PV current
const int pinVout = A2;  // Buck output voltage
const int pwmPin  = 9;   // PWM output

// ---- Scaling ----
// 82k / 18k divider => correction factor 5.556
// 1 ADC count = 0.004887 V at pin
// PV voltage per count = 0.004887 * 5.556 ≈ 0.02714 V
const float V_SCALE = (5.0 / 1023.0) * ((82.0 + 18.0) / 18.0);

// ACS712-30A current sensor: 66 mV/A, zero-current ~2.5 V
const float ACS_OFFSET = 2.50;
const float ACS_SENS   = 0.066;  // nominal
const float I_CAL      = 1.00;

// -----------------------------------------------------------------------------
//  SETUP
// -----------------------------------------------------------------------------
void setup() {
  Serial.begin(112500);
  pinMode(pwmPin, OUTPUT);
  analogReference(DEFAULT);

  // --- Configure Timer1 for 50 kHz Fast PWM (Mode 14, ICR1 = 319) ---
  TCCR1A = _BV(COM1A1) | _BV(WGM11);
  TCCR1B = _BV(WGM13) | _BV(WGM12) | _BV(CS10);
  ICR1 = 319;   // 16 MHz / (1 * (319 + 1)) = 50 kHz

  // --- Initialize OLED display ---
  if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)) {
    if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
      Serial.println(F("SSD1306 init failed"));
      for(;;);
    }
  }

  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.println(F("ASSTA Hybrid MPPT"));
  display.println(F("50kHz Dual ADC Ready"));
  display.display();

  Dold = Dinit;
  Vold = Iold = Pold = 0.0;
  setDuty(Dold);

  Serial.println(F("ASSTA Hybrid MPPT initialized (50kHz + ACS712 + OLED)"));
}

// -----------------------------------------------------------------------------
//  MAIN LOOP — runs every 1 ms
// -----------------------------------------------------------------------------
void loop() {
  static unsigned long lastMicros = 0;
  unsigned long now = micros();
  if (now - lastMicros < 1000) return; // 1 ms tick
  lastMicros = now;

  // --- ADC sampling ---
  float Vpv_raw  = analogRead(pinVpv)  * V_SCALE;
  float Vadc_I   = analogRead(pinIpv)  * (5.0 / 1023.0);
  float Ipv_raw  = ((Vadc_I - ACS_OFFSET) / ACS_SENS) * I_CAL;   // signed, calibrated current (A)
  float Vout_raw = analogRead(pinVout) * V_SCALE;

  // --- Init on first run ---
  if (!init_done) {
    Vfil = Vpv_raw;
    Ifil = Ipv_raw;
    Vout_fil = Vout_raw;
    Vold = Vfil;
    Iold = Ifil;
    Pold = Vfil * Ifil;
    Imp_hat = max(Ifil, 0.0f);
    init_done = true;
  }

  // --- Exponential moving average filtering ---
  Vfil     = (1 - ALPHA) * Vfil + ALPHA * Vpv_raw;
  Ifil     = (1 - ALPHA) * Ifil + ALPHA * Ipv_raw;
  Vout_fil = (1 - ALPHA) * Vout_fil + ALPHA * Vout_raw;

  // --- Compute deltas ---
  float Ppv  = Vfil * Ifil;
  float dVpv = Vfil - Vold;
  float dIpv = Ifil - Iold;
  float dPpv = Ppv  - Pold;

  float dV_eff = dVpv;
  if (fabs(dV_eff) < EPS_DV)
    dV_eff = (dV_eff >= 0 ? 1 : -1) * EPS_DV;

  // --- ASSTA core metrics ---
  float M = fabs(dPpv + dIpv * dVpv);
  if (M < EPS_M) M = 0;
  float ratio = dPpv / dV_eff;
  float theta = atan(ratio);
  float dtheta_dv = 1.0 / (1.0 + ratio * ratio);
  bool near_mpp = (fabs(theta) < EPS_TH) && (dtheta_dv > Z);

  // --- Update I_MPP estimate when near MPP ---
  if (near_mpp)
    Imp_hat = (1 - ALPHA_IMP) * Imp_hat + ALPHA_IMP * max(Ifil, 0.0f);

  // --- Current-zone clamp ---
  float I_low  = Imp_hat * (1 - IZONE_PCT);
  float I_high = Imp_hat * (1 + IZONE_PCT);
  bool in_zone = (Ifil >= I_low) && (Ifil <= I_high);

  // --- Decision logic ---
  float Duty = Dold;
  const char* status = "TRACK";

  if (in_zone) {
    Duty = Dold;
    status = "ZONE";
  } else if (M >= C) {
    float delta = SF * M;
    if ((fabs(theta) < EPS_TH) && (dtheta_dv > Z)) {
      Duty = Dold;
      status = "HOLD";
    } else {
      if (theta > 0) Duty = Dold - delta;
      else           Duty = Dold + delta;
      status = "TRACK";
    }
  }

  // --- Slew + Clamp ---
  Duty = constrain(Duty, Dold - DSTEPMAX, Dold + DSTEPMAX);
  Duty = constrain(Duty, Dmin, Dmax);

  // --- Store and output ---
  Dold = Duty;
  Vold = Vfil;
  Iold = Ifil;
  Pold = Ppv;
  setDuty(Duty);

  // --- OLED display ---
  display.clearDisplay();
  display.setCursor(0, 0);
  display.println(F("ASSTA Hybrid MPPT"));
  display.println(F("------------------"));
  display.print(F("Vpv: "));  display.print(Vfil, 2);     display.println(F(" V"));
  display.print(F("Ipv: "));  display.print(Ifil, 2);     display.println(F(" A"));
  display.print(F("Vout: ")); display.print(Vout_fil, 2); display.println(F(" V"));
  display.print(F("Ppv: "));  display.print(Ppv, 2);      display.println(F(" W"));
  display.print(F("D: "));    display.print(Duty * 100.0, 1); display.println(F(" %"));
  display.print(F("Mode: ")); display.println(status);
  display.display();

  // --- VS Code Serial Plotter output ---
  Serial.print(">");
  Serial.print("Vpv:");   Serial.print(Vfil, 3);   Serial.print(",");
  Serial.print("Ipv:");   Serial.print(Ifil, 3);   Serial.print(",");
  Serial.print("Vout:");  Serial.print(Vout_fil, 3); Serial.print(",");
  Serial.print("Ppv:");   Serial.print(Ppv, 3);    Serial.print(",");
  Serial.print("Duty:");  Serial.print(Duty * 100.0, 2); Serial.print(",");
  Serial.print("Imp:");   Serial.print(Imp_hat, 3);
  Serial.println(); // \r\n
}

// -----------------------------------------------------------------------------
//  SET PWM DUTY (0–1 range → OCR1A relative to ICR1)
// -----------------------------------------------------------------------------
void setDuty(float duty) {
  duty = constrain(duty, 0.0, 0.95);
  OCR1A = (uint16_t)(duty * ICR1);
}
