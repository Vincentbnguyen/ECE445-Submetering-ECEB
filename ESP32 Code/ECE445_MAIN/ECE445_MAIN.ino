#include <LiquidCrystal_I2C.h>
#include <Wire.h>

#define I2C_ADDRESS 0x48

#include <ADS1115_lite.h>
ADS1115_lite adc(ADS1115_DEFAULT_ADDRESS);

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
float v_offset = -1.50; // centered by subtracting the offset (make it negative)

int useSimulator = 0; // 0 = physical
int shouldPrintLabels = false;
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

int16_t rawResult;
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

unsigned long starttime;
unsigned long endtime ;
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
    // A0 = Voltage, A1 = Current's voltage
    starttime = micros();
    adc.setMux(ADS1115_REG_CONFIG_MUX_SINGLE_0); //Set single ended mode between AIN0 and GND
    adc.triggerConversion(); 
    rawResult = adc.getConversion(); //This polls the ADS1115 and wait for conversion to finish, THEN returns the value
    v = (((rawResult * 1.0 / 32768) * 2048) / 1000) + v_offset;  // (rawResult * 1.0 / ADS1115_REG_FACTOR) * voltageRange;

    adc.setMux(ADS1115_REG_CONFIG_MUX_SINGLE_1); //Set single ended mode between AIN1 and GND
    adc.triggerConversion();
    rawResult = adc.getConversion(); //This polls the ADS1115 and wait for conversion to finish, THEN returns the value
    i = (((rawResult * 1.0 / 32768) * 2048) / 1000) + v_offset;  // raw -> voltage formula = (rawResult * 1.0 / ADS1115_REG_FACTOR) * voltageRange;
    endtime = micros();
    freq = 1/((endtime - starttime) * 0.000001);
  }
  // Serial.print(String(freq) + "  ");
  printInstantaneous(v, i);

  float new_instant_power = v * i;
  float new_v_squared = v * v;
  float new_i_squared = i * i;
  // Serial.print(String(currentIndex) + "  ");
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
}

void setADCSettings() {
  if (!adc.testConnection()) {
    Serial.println("ADS1115 Connection failed"); //oh man...something is wrong
    return;
  }
  adc.setGain(ADS1115_REG_CONFIG_PGA_2_048V); // set +- voltage input range
  adc.setSampleRate(ADS1115_REG_CONFIG_DR_860SPS); //Set the slowest and most accurate sample rate
}

void setLCDSettings() {
  lcd.init();
  // turn on LCD backlight                      
  lcd.backlight();
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Sampling...");
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