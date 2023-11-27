#include <LiquidCrystal_I2C.h>
#include <Wire.h>
#include <ADS1115_lite.h>
#include <math.h>

ADS1115_lite adc(ADS1115_DEFAULT_ADDRESS);

int decimalPlaces = 6;
int lcdColumns = 16; int lcdRows = 2; LiquidCrystal_I2C lcd(0x27, lcdColumns, lcdRows);  

float sampleRate = 120;
const int sampleSize = 1000;
int currentIndex = 0;

// saved data points
float instantaneous_power_samples[sampleSize];
float v_samples_squared[sampleSize];
float i_samples_squared[sampleSize];

float calibrate_offset = -0.0214; // calibrated using scopy
float v_offset = -1.50; // centered by subtracting the offset (make it negative)

int useSimulator = 0; // 0 = physical
int shouldPrintLabels = false;

int shouldPrintPP = false;
int useLCD = true;
// this program will measure real and apparent power, voltage, and current samples
// it uses a rolling window of 1000 samples to do so
// to calcualte real power, for every sample, we take the multiply the instantaneous voltage and current, and average this sample set/
// to calcualte apparent power, for every sample, we process voltage and and current seperately where we square the measurement, and take the average of the sample set, then square root them to obtain the RMS value
// then, the apparent power is calculated by the multiplying the voltage and current RMS values..

int hasEnoughSamples = false;
int16_t rawResult;
float v = 0; // sampled 
float i = 0; // sampled

// simulated sin waves factors
int sampleRateMS = 0.5; // this value is 1 sample per samplerateMS (HZ = 1/(sampleRateMS * 0.001))
int v_amplitude = 1; int v_phaseShift = 0; int v_freq = 60;
int i_amplitude = 1; int i_phaseShift = 3; int i_freq = 60;

float calibrated_p_real = 0;
float p_real = 0;
float p_app = 0;

float v_rms = 0;
float i_rms = 0;
float v_rms_sum = 0;
float i_rms_sum = 0;
float instantaneous_sum = 0;
float measureTime = 3.259; // in ms

unsigned long starttime;
float freq = 0;

void setup() {
  Serial.begin(250000);
  setLCDSettings();
  Wire.begin();
  Wire.setClock(100000);
  setADCSettings();
}

void loop() {
  if (useSimulator == 1) {
    v = v_amplitude * sin(2 * PI * v_freq * (currentIndex * sampleRateMS * 0.001) + v_phaseShift);
    i = i_amplitude * sin(2 * PI * i_freq * (currentIndex * sampleRateMS * 0.001) + i_phaseShift);
  } else {
    // take instantanous measurements
    v = read_A0();
    float startMeasure = micros();
    i = read_A1();
    float endMeasure = micros();
    sampleDelay = (endMeasure - startMeasure) * 0.001; // time to measure i in ms
    // Serial.print("SAMPEL DELAY = " + String(sampleDelay) + "  ");

    // calculate freq
    if (currentIndex % 2 == 0) {
      starttime = micros();
    } else {
      float endtime = micros();
      float sampleTime = (endtime - starttime); // in us
      // Serial.print("sampleTime = " + String(sampleTime, 10) + "  ");
      freq = 1.0/(sampleTime * 0.001 * 0.001);
    }
    // Serial.print("FREQ = " + String(freq, 3) + "  ");
  }

  float new_instant_power = v * i;
  float new_v_squared = v * v;
  float new_i_squared = i * i;
  if (hasEnoughSamples == false) { 
    // populate starting samples
    // collect for instantaneous
    instantaneous_power_samples[currentIndex] = new_instant_power;
    instantaneous_sum += new_instant_power;

    // collect for RMS
    v_samples_squared[currentIndex] = new_v_squared;
    i_samples_squared[currentIndex] = new_i_squared;
    v_rms_sum += new_v_squared;
    i_rms_sum += new_i_squared;
    
    currentIndex += 1;
    if (useLCD && currentIndex % 100 == 0) {
      lcd.setCursor(0, 1);
      lcd.print(String(currentIndex) + "/1000");
    }
    
    if (currentIndex == sampleSize - 1) {
      hasEnoughSamples = true;
      // calculate p_real
      p_real = instantaneous_sum / sampleSize;
      // calculate p_app
      v_rms = sqrt(v_rms_sum / sampleSize);
      i_rms = sqrt(i_rms_sum / sampleSize);
      p_app = v_rms * i_rms;
      currentIndex = 0;
    }
  } else if (hasEnoughSamples == true) {
    // update p_app using new v and i measurement and overwrite old_squared
    float old_v_squared = v_samples_squared[currentIndex];
    float old_i_squared = i_samples_squared[currentIndex];
    v_rms_sum += new_v_squared - old_v_squared;
    i_rms_sum += new_i_squared - old_i_squared;
    v_rms = sqrt(v_rms_sum / sampleSize);
    i_rms = sqrt(i_rms_sum / sampleSize);

    p_app = v_rms * i_rms;

    // update p_real using new_instant_power and overwrite old_instant_power
    float old_instant_power = instantaneous_power_samples[currentIndex];
    p_real = ((p_real * sampleSize) - old_instant_power + new_instant_power) / sampleSize;

    // account for sample delay of i to update p_real
    float currentPhaseOffset = acos(p_real/p_app); // i used this with phase offset 0 to get manyal delayPhase offset
    float delayPhaseOffset = 1.04; // offset in rad when phase diff should be 0 (took from currentPhaseOffset when setting phase diff to be 0)
    // Serial.print("Measyred phase offset = " + String(currentPhaseOffset) + "  ");
    calibrated_p_real = p_app * (cos(currentPhaseOffset)*cos(delayPhaseOffset) + sin(currentPhaseOffset)*sin(delayPhaseOffset)); // cos(phase offset - delay) identity formula
    // Serial.print("phase offset = " + String((currentPhaseOffset - delayPhaseOffset) * 180 / M_PI) + "  ");
       // only update LCD display every 500 samples
    if (currentIndex % 500 == 0) {
      printPowerToLCD(calibrated_p_real, p_app);
    }
    // overwrite old
    instantaneous_power_samples[currentIndex] = new_instant_power;
    // overwrite old
    v_samples_squared[currentIndex] = new_v_squared;
    i_samples_squared[currentIndex] = new_i_squared;
    currentIndex = (currentIndex + 1) % sampleSize;
  }
  printInstantaneous(v, i);
  if (shouldPrintPP) {
    printVpp();
    printIpp();
  }
  if (hasEnoughSamples == true) {
    printRMS(v_rms, i_rms);
    printPower(calibrated_p_real, p_app);
  }
  Serial.println(" ");
}

void setADCSettings() {
  if (!adc.testConnection()) {
    Serial.println("ADS1115 Connection failed");
    return;
  }
  adc.setGain(ADS1115_REG_CONFIG_PGA_2_048V); // set +- voltage input range
  adc.setSampleRate(ADS1115_REG_CONFIG_DR_860SPS);
}

void setLCDSettings() {
  lcd.init();
  // turn on LCD backlight                      
  lcd.backlight();
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Sampling...");
}

void printPowerToLCD(float p_real, float p_app) {
  if (useLCD) {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("REAL:" + String(p_real, 5) + " W");
    lcd.setCursor(0, 1);
    lcd.print("APP:" + String(p_app, 5) + " W");
  }
}

void printInstantaneous(float v, float i) {
    if (shouldPrintLabels) { Serial.print("v_inst: "); }
    if (v >= 0) { // handle spacing
        Serial.print(" ");
    }
    Serial.print(String(v, decimalPlaces) + "  ");

    // Serial Monitor Logging
    if (shouldPrintLabels) { Serial.print("i_inst: "); }
    if (i >= 0) { // handle spacing
        Serial.print(" ");
    }
    Serial.print(String(i, decimalPlaces) + "  ");
}

void printRMS(float v_rms, float i_rms) {
  if (shouldPrintLabels) { Serial.print("v_rms: "); } 
  Serial.print(String(v_rms, decimalPlaces) + "  ");
  if (shouldPrintLabels) { Serial.print("i_rms: "); }
  Serial.print(String(i_rms, decimalPlaces) + "  ");
}

void printPower(float p_real, float p_app) {
  if (shouldPrintLabels) { Serial.print("p_real: "); } 
  Serial.print(String(p_real, decimalPlaces) + "  ");
  if (shouldPrintLabels) { Serial.print("p_app: "); }
  Serial.print(String(p_app, decimalPlaces) + "  ");
}

float read_A0() {
  adc.setMux(ADS1115_REG_CONFIG_MUX_SINGLE_0); //Set single ended mode between AIN0 and GND
  adc.triggerConversion(); 
  rawResult = adc.getConversion(); //This polls the ADS1115 and wait for conversion to finish, THEN returns the value
  return (((rawResult * 1.0 / 32768) * 2048) / 1000) + v_offset + calibrate_offset;  // (rawResult * 1.0 / ADS1115_REG_FACTOR) * voltageRange;
}

float read_A1() {
  adc.setMux(ADS1115_REG_CONFIG_MUX_SINGLE_1); //Set single ended mode between AIN1 and GND
  adc.triggerConversion();
  rawResult = adc.getConversion(); //This polls the ADS1115 and wait for conversion to finish, THEN returns the value
  return (((rawResult * 1.0 / 32768) * 2048) / 1000) + v_offset + calibrate_offset;  // raw -> voltage formula = (rawResult * 1.0 / ADS1115_REG_FACTOR) * voltageRange;
}

void printVpp() {
  float v_max = sqrt(v_samples_squared[sampleSize/2]);
  float v_min = sqrt(v_samples_squared[sampleSize/2]);

  for (int i = 1; i < sampleSize; i++) {
    if (sqrt(v_samples_squared[i]) < v_min) {
      v_min = sqrt(v_samples_squared[i]);
    }
    if (sqrt(v_samples_squared[i]) > v_max) {
      v_max = sqrt(v_samples_squared[i]);
    }
  }

  if (shouldPrintLabels) { Serial.print("v_max: "); }
  if (v_max >= 0) { // handle spacing
    Serial.print(" ");
  }

  Serial.print(String(v_max) + "  ");

  if (shouldPrintLabels) { Serial.print("v_min: "); }
  if (v_min >= 0) { // handle spacing
    Serial.print(" ");
  }
  Serial.print(String(v_min) + "  ");
}

void printIpp() {
  float i_max = sqrt(i_samples_squared[sampleSize/2]);
  float i_min = sqrt(i_samples_squared[sampleSize/2]);

  for (int i = 1; i < sampleSize; i++) {
    if (sqrt(i_samples_squared[i]) < i_min) {
      i_min = sqrt(i_samples_squared[i]);
    }
    if (sqrt(i_samples_squared[i]) > i_max) {
      i_max = sqrt(i_samples_squared[i]);
    }
  }

  if (shouldPrintLabels) { Serial.print("i_max: "); }
  if (i_max >= 0) { // handle spacing
    Serial.print(" ");
  }

  Serial.print(String(i_max) + "  ");

  if (shouldPrintLabels) { Serial.print("i_min: "); }
  if (i_min >= 0) { // handle spacing
    Serial.print(" ");
  }
  Serial.print(String(i_min) + "  ");
}