#include <LiquidCrystal_I2C.h>
#include <Wire.h>
#include<ADS1115_WE.h>
#define I2C_ADDRESS 0x48

ADS1115_WE adc = ADS1115_WE(I2C_ADDRESS);
// set the LCD number of columns and rows
int lcdColumns = 16;
int lcdRows = 2;
LiquidCrystal_I2C lcd(0x27, lcdColumns, lcdRows);  


const int sampleSize = 1000;
int currentIndex = 0;
int sampleRateMS = 0.5; // this value is 1 sample per samplerateMS (HZ = 1/(sampleRateMS * 0.001))

// saved data points
float instantaneous_power_samples[sampleSize];
float v_samples_squared[sampleSize];
float i_samples_squared[sampleSize];

float calibrate_offset = -0.07; // calibrated using scopy
float v_offset = 0; // centered by subtracting the offset (make it negative)

int useSimulator = 0; // 0 = physical
int shouldPrintLabels = true;
int hasEnoughSamples = false;
int shouldPrintPP = false;
int useLCD = false;
// this program will measure real and apparent power, voltage, and current samples at a rate of 1/(sampleRateMS * 0.001) times a second. currenlty it is at 1000 HZ 
// it uses a rolling window of 1000 samples to do so
// to calcualte real power, for every sample, we take the multiply the instantaneous voltage and current, and average this sample set/
// to calcualte apparent power, for every sample, we process voltage and and current seperately where we square the measurement, and take the average of the sample set, then square root them to obtain the RMS value
// then, the apparent power is calculated by the multiplying the voltage and current RMS values..


// notes: for multiple I2D devices (LCD display and ads1115):
// The ADS111x have one address pin, ADDR, that configures the I2C address of the device.
// This pin can be connected to GND, VDD, SDA, or SCL, allowing for four different addresses to be selected with one pin, as
// shown in Table 4.
// The state of address pin ADDR is sampled continuously. Use the GND, VDD and SCL addresses first.


float v = 0; // sampled 
float i = 0; // sampled

// simulated sin waves factors
int v_amplitude = 1;
int v_phaseShift = 0;
int v_freq = 60;
int i_amplitude = 1;
int i_phaseShift = 3;
int i_freq = 60;

float p_real = 0;
float p_rms = 0;

float v_rms = 0;
float i_rms = 0;
float v_rms_sum = 0;
float i_rms_sum = 0;
float instantaneous_sum = 0;

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
    // 12 sec total, 6 sec each
    v = readChannel(ADS1115_COMP_0_GND) + v_offset + calibrate_offset;
    i = readChannel(ADS1115_COMP_1_GND) + v_offset + calibrate_offset;
  }
  printInstantaneous(v, i);

  float new_instant_power = v * i;
  float new_v_squared = v * v;
  float new_i_squared = i * i;
  Serial.print(String(currentIndex) + "  ");
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
    if (useLCD) {
      lcd.setCursor(0, 1);
      lcd.print(String(currentIndex) + "/1000");
    }
    
    // Serial.print(String(currentIndex) + "/1000" + "  ");
    if (currentIndex == sampleSize - 1) {
      hasEnoughSamples = true;
      // calculate p_real
      p_real = instantaneous_sum / sampleSize;

      // calculate p_rms
      v_rms = sqrt(v_rms_sum / sampleSize);
      i_rms = sqrt(i_rms_sum / sampleSize);
      p_rms = v_rms * i_rms;
      printRealPowerToLCD(p_real);
      printRMSPowerToLCD(p_rms);
      currentIndex = 0;
    }
  } else if (hasEnoughSamples == true) {
    // update p_real using new_instant_power and overwrite old_instant_power
    float old_instant_power = instantaneous_power_samples[currentIndex];
    float new_p_real = ((p_real * sampleSize) - old_instant_power + new_instant_power) / sampleSize;
    // only update value and LCD display when changed 
    if (new_p_real != p_real) {
      p_real = new_p_real;
      printRealPowerToLCD(p_real);
    }
    // overwrite old
    instantaneous_power_samples[currentIndex] = new_instant_power;

    // update p_rms using new v and i measurement and overwrite old_squared
    float old_v_squared = v_samples_squared[currentIndex];
    float old_i_squared = i_samples_squared[currentIndex];
    v_rms_sum += new_v_squared - old_v_squared;
    i_rms_sum += new_i_squared - old_i_squared;
    v_rms = sqrt(v_rms_sum / sampleSize);
    i_rms = sqrt(i_rms_sum / sampleSize);

    printRMS(v_rms, i_rms);

    float new_p_rms = v_rms * i_rms;
    // only update value and LCD display when changed
    if (new_p_rms != p_rms) {
      p_rms = new_p_rms;
      printRMSPowerToLCD(p_rms);
    }
    // overwrite old
    v_samples_squared[currentIndex] = new_v_squared;
    i_samples_squared[currentIndex] = new_i_squared;
    currentIndex = (currentIndex + 1) % sampleSize;
    if (shouldPrintPP) {
      printVpp();
      printIpp();
    }
  }
  printPower(p_real, p_rms);

  Serial.println(" ");
  // controls sample rate
 // delay(sampleRateMS);
}

void setADCSettings() {
  if(!adc.init()){
    Serial.println("ADS1115 not connected!");
  }

  /* Set the voltage range of the ADC to adjust the gain
   * Please note that you must not apply more than VDD + 0.3V to the input pins!
   * 
   * ADS1115_RANGE_6144  ->  +/- 6144 mV
   * ADS1115_RANGE_4096  ->  +/- 4096 mV
   * ADS1115_RANGE_2048  ->  +/- 2048 mV (default)
   * ADS1115_RANGE_1024  ->  +/- 1024 mV
   * ADS1115_RANGE_0512  ->  +/- 512 mV
   * ADS1115_RANGE_0256  ->  +/- 256 mV
   */
  adc.setVoltageRange_mV(ADS1115_RANGE_2048); //comment line/change parameter to change range

  adc.setCompareChannels(ADS1115_COMP_0_GND); //comment line/change parameter to change channel
  adc.setCompareChannels(ADS1115_COMP_1_GND);

  adc.setMeasureMode(ADS1115_CONTINUOUS); //comment line/change parameter to change mode

    /* Set the conversion rate in SPS (samples per second)
   * Options should be self-explaining: 
   * 
   *  ADS1115_8_SPS 
   *  ADS1115_16_SPS  
   *  ADS1115_32_SPS 
   *  ADS1115_64_SPS  
   *  ADS1115_128_SPS (default)
   *  ADS1115_250_SPS 
   *  ADS1115_475_SPS 
   *  ADS1115_860_SPS 
   */
  adc.setConvRate(ADS1115_860_SPS); //uncomment if you want to change the default
}

void setLCDSettings() {
  lcd.init();
  // turn on LCD backlight                      
  lcd.backlight();
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Sampling...");
}

float readChannel(ADS1115_MUX channel) {
  float voltage = 0.0;
  adc.setCompareChannels(channel);
  voltage = adc.getResult_V(); // alternative: getResult_mV for Millivolt
  return voltage;
}

void printRealPowerToLCD(float p_real) {
  if (useLCD) {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("REAL:" + String(p_real) + " W");
  }
}

void printRMSPowerToLCD(float p_rms) {
  if (useLCD) {
    lcd.setCursor(0, 1);
    lcd.print("RMS:" + String(p_rms) + " W");
  }
}

void printInstantaneous(float v, float i) {
    if (shouldPrintLabels) {
      Serial.print("v_inst: "); 
      if (v >= 0) { // handle spacing
        Serial.print(" ");
      }
    }
    Serial.print(String(v) + "  ");

    // Serial Monitor Logging
    if (shouldPrintLabels) {
      Serial.print("i_inst: ");
      if (i >= 0) { // handle spacing
        Serial.print(" ");
      }
    }
    Serial.print(String(i) + "  ");
}

void printRMS(float v_rms, float i_rms) {
  if (shouldPrintLabels) { Serial.print("v_rms: "); } 
  Serial.print(String(v_rms) + "  ");
  if (shouldPrintLabels) { Serial.print("i_rms: "); }
  Serial.print(String(i_rms) + "  ");
}

void printPower(float p_real, float p_rms) {
  if (shouldPrintLabels) { Serial.print("p_real: "); } 
  Serial.print(String(p_real) + "  ");
  if (shouldPrintLabels) { Serial.print("p_rms: "); }
  Serial.print(String(p_rms) + "  ");
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