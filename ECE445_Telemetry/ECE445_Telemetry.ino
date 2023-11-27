#include <ArduinoJson.h>
#include <ArduinoJson.hpp>

#include <cstdlib>
#include <string.h>
#include <time.h>

// Libraries for MQTT client and WiFi connection
#include <WiFi.h>
#include <mqtt_client.h>

// Azure IoT SDK for C includes
#include <az_core.h>
#include <az_iot.h>
#include <azure_ca.h>

// Additional sample headers
#include "AzIoTSasToken.h"
#include "SerialLogger.h"
#include "iot_configs.h"
#include <ArduinoJson.h>

// When developing for your own Arduino-based platform,
// please follow the format '(ard;<platform>)'.
#define AZURE_SDK_CLIENT_USER_AGENT "c%2F" AZ_SDK_VERSION_STRING "(ard;esp32)"

// Utility macros and defines
#define sizeofarray(a) (sizeof(a) / sizeof(a[0]))
#define NTP_SERVERS "pool.ntp.org", "time.nist.gov"
#define MQTT_QOS1 1
#define DO_NOT_RETAIN_MSG 0
#define SAS_TOKEN_DURATION_IN_MINUTES 60
#define UNIX_TIME_NOV_13_2017 1510592825

#define PST_TIME_ZONE -6
#define PST_TIME_ZONE_DAYLIGHT_SAVINGS_DIFF 1

#define GMT_OFFSET_SECS (PST_TIME_ZONE * 3600)
#define GMT_OFFSET_SECS_DST ((PST_TIME_ZONE + PST_TIME_ZONE_DAYLIGHT_SAVINGS_DIFF) * 3600)

#include <time.h>

#define UNIX_EPOCH_START_YEAR 1900

// Translate iot_configs.h defines into variables used by the sample
static const char* ssid = IOT_CONFIG_WIFI_SSID;
static const char* password = IOT_CONFIG_WIFI_PASSWORD;
static const char* host = IOT_CONFIG_IOTHUB_FQDN;
static const char* mqtt_broker_uri = "mqtts://" IOT_CONFIG_IOTHUB_FQDN;
static const char* device_id = IOT_CONFIG_DEVICE_ID;
static const int mqtt_port = AZ_IOT_DEFAULT_MQTT_CONNECT_PORT;

// Memory allocated for the sample's variables and structures.
static esp_mqtt_client_handle_t mqtt_client;
static az_iot_hub_client client;

static char mqtt_client_id[128];
static char mqtt_username[128];
static char mqtt_password[200];
static uint8_t sas_signature_buffer[256];
static unsigned long next_telemetry_send_time_ms = 180000; // currenlty uploading every 3 minute
static char telemetry_topic[128];
static uint32_t telemetry_send_count = 0;
static String telemetry_payload = "";

#define INCOMING_DATA_BUFFER_SIZE 128
static char incoming_data[INCOMING_DATA_BUFFER_SIZE];

// Auxiliary functions
#ifndef IOT_CONFIG_USE_X509_CERT
static AzIoTSasToken sasToken(
    &client,
    AZ_SPAN_FROM_STR(IOT_CONFIG_DEVICE_KEY),
    AZ_SPAN_FROM_BUFFER(sas_signature_buffer),
    AZ_SPAN_FROM_BUFFER(mqtt_password));
#endif // IOT_CONFIG_USE_X509_CERT

static void connectToWiFi()
{
  Logger.Info("Connecting to WIFI SSID " + String(ssid));

  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  delay(100);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");
  }

  Serial.println("");

  Logger.Info("WiFi connected, IP address: " + WiFi.localIP().toString());
}

static void initializeTime()
{
  Logger.Info("Setting time using SNTP");

  configTime(GMT_OFFSET_SECS, GMT_OFFSET_SECS_DST, NTP_SERVERS);
  time_t now = time(NULL);
  while (now < UNIX_TIME_NOV_13_2017)
  {
    delay(500);
    Serial.print(".");
    now = time(nullptr);
  }
  Serial.println("");
  Logger.Info("Time initialized!");
}

void receivedCallback(char* topic, byte* payload, unsigned int length)
{
  Logger.Info("Received [");
  Logger.Info(topic);
  Logger.Info("]: ");
  for (int i = 0; i < length; i++)
  {
    Serial.print((char)payload[i]);
  }
  Serial.println("");
}

static esp_err_t mqtt_event_handler(esp_mqtt_event_handle_t event)
{
  switch (event->event_id)
  {
    int i, r;

    case MQTT_EVENT_ERROR:
      Logger.Info("MQTT event MQTT_EVENT_ERROR");
      break;
    case MQTT_EVENT_CONNECTED:
      Logger.Info("MQTT event MQTT_EVENT_CONNECTED");

      r = esp_mqtt_client_subscribe(mqtt_client, AZ_IOT_HUB_CLIENT_C2D_SUBSCRIBE_TOPIC, 1);
      if (r == -1)
      {
        Logger.Error("Could not subscribe for cloud-to-device messages.");
      }
      else
      {
        Logger.Info("Subscribed for cloud-to-device messages; message id:" + String(r));
      }

      break;
    case MQTT_EVENT_DISCONNECTED:
      Logger.Info("MQTT event MQTT_EVENT_DISCONNECTED");
      break;
    case MQTT_EVENT_SUBSCRIBED:
      Logger.Info("MQTT event MQTT_EVENT_SUBSCRIBED");
      break;
    case MQTT_EVENT_UNSUBSCRIBED:
      Logger.Info("MQTT event MQTT_EVENT_UNSUBSCRIBED");
      break;
    case MQTT_EVENT_PUBLISHED:
      Logger.Info("MQTT event MQTT_EVENT_PUBLISHED");
      break;
    case MQTT_EVENT_DATA:
      Logger.Info("MQTT event MQTT_EVENT_DATA");

      for (i = 0; i < (INCOMING_DATA_BUFFER_SIZE - 1) && i < event->topic_len; i++)
      {
        incoming_data[i] = event->topic[i];
      }
      incoming_data[i] = '\0';
      Logger.Info("Topic: " + String(incoming_data));

      for (i = 0; i < (INCOMING_DATA_BUFFER_SIZE - 1) && i < event->data_len; i++)
      {
        incoming_data[i] = event->data[i];
      }
      incoming_data[i] = '\0';
      Logger.Info("Data: " + String(incoming_data));

      break;
    case MQTT_EVENT_BEFORE_CONNECT:
      Logger.Info("MQTT event MQTT_EVENT_BEFORE_CONNECT");
      break;
    default:
      Logger.Error("MQTT event UNKNOWN");
      break;
  }

  return ESP_OK;
}

static void initializeIoTHubClient()
{
  az_iot_hub_client_options options = az_iot_hub_client_options_default();
  options.user_agent = AZ_SPAN_FROM_STR(AZURE_SDK_CLIENT_USER_AGENT);

  if (az_result_failed(az_iot_hub_client_init(
          &client,
          az_span_create((uint8_t*)host, strlen(host)),
          az_span_create((uint8_t*)device_id, strlen(device_id)),
          &options)))
  {
    Logger.Error("Failed initializing Azure IoT Hub client");
    return;
  }

  size_t client_id_length;
  if (az_result_failed(az_iot_hub_client_get_client_id(
          &client, mqtt_client_id, sizeof(mqtt_client_id) - 1, &client_id_length)))
  {
    Logger.Error("Failed getting client id");
    return;
  }

  if (az_result_failed(az_iot_hub_client_get_user_name(
          &client, mqtt_username, sizeofarray(mqtt_username), NULL)))
  {
    Logger.Error("Failed to get MQTT clientId, return code");
    return;
  }

  Logger.Info("Client ID: " + String(mqtt_client_id));
  Logger.Info("Username: " + String(mqtt_username));
}

static int initializeMqttClient()
{
#ifndef IOT_CONFIG_USE_X509_CERT
  if (sasToken.Generate(SAS_TOKEN_DURATION_IN_MINUTES) != 0)
  {
    Logger.Error("Failed generating SAS token");
    return 1;
  }
#endif

  esp_mqtt_client_config_t mqtt_config;
  memset(&mqtt_config, 0, sizeof(mqtt_config));
  mqtt_config.uri = mqtt_broker_uri;
  mqtt_config.port = mqtt_port;
  mqtt_config.client_id = mqtt_client_id;
  mqtt_config.username = mqtt_username;

#ifdef IOT_CONFIG_USE_X509_CERT
  Logger.Info("MQTT client using X509 Certificate authentication");
  mqtt_config.client_cert_pem = IOT_CONFIG_DEVICE_CERT;
  mqtt_config.client_key_pem = IOT_CONFIG_DEVICE_CERT_PRIVATE_KEY;
#else // Using SAS key
  mqtt_config.password = (const char*)az_span_ptr(sasToken.Get());
#endif

  mqtt_config.keepalive = 30;
  mqtt_config.disable_clean_session = 0;
  mqtt_config.disable_auto_reconnect = false;
  mqtt_config.event_handle = mqtt_event_handler;
  mqtt_config.user_context = NULL;
  mqtt_config.cert_pem = (const char*)ca_pem;

  mqtt_client = esp_mqtt_client_init(&mqtt_config);

  if (mqtt_client == NULL)
  {
    Logger.Error("Failed creating mqtt client");
    return 1;
  }

  esp_err_t start_result = esp_mqtt_client_start(mqtt_client);

  if (start_result != ESP_OK)
  {
    Logger.Error("Could not start mqtt client; error code:" + start_result);
    return 1;
  }
  else
  {
    Logger.Info("MQTT client started");
    return 0;
  }
}

/*
 * @brief           Gets the number of seconds since UNIX epoch until now.
 * @return uint32_t Number of seconds.
 */
static uint32_t getEpochTimeInSecs() { return (uint32_t)time(NULL); }

static void establishConnection()
{
  connectToWiFi();
  initializeTime();
  initializeIoTHubClient();
  (void)initializeMqttClient();
}

// arrays to uploading to clouud
#include <ArduinoJson.h>
int cloudIndex = 0;
const int cloudArraySize = 1020; // 17 samples/sec * 60 sec/min * 3 min = 1020 samples uploaded every 3 min
char time_arr[cloudArraySize][20]; // 20 = size of time stamp "2023/11/26 18:03:21"
float vrms_arr[cloudArraySize];
float irms_arr[cloudArraySize];
float p_real_arr[cloudArraySize];
float p_app_arr[cloudArraySize];

static void generateTelemetryPayload()
{

  DynamicJsonDocument jsonDoc(cloudArraySize * 5 * sizeof(int) + cloudArraySize * 20);
  JsonObject root = jsonDoc.to<JsonObject>();
  JsonArray dataArray = root.createNestedArray("data");

  for (int i = 0; i < cloudArraySize; ++i) {
    JsonObject entry = dataArray.createNestedObject();
    entry["timestamp"] = time_arr[i];
    entry["vrms"] = vrms_arr[i];
    entry["irms"] = irms_arr[i];
    entry["p_real"] = p_real_arr[i];
    entry["p_app"] = p_app_arr[i];
  }

  // Serialize the JSON document to the existing telemetry_payload string
  serializeJson(root, telemetry_payload);

  // Print the updated JSON string
  Serial.println("Telemetry Payload:");
  Serial.println(telemetry_payload);
}

static void sendTelemetry()
{
  Logger.Info("Sending telemetry: TIME_STAMP: " + String(getTime()));

  // The topic could be obtained just once during setup,
  // however if properties are used the topic need to be generated again to reflect the
  // current values of the properties.
  if (az_result_failed(az_iot_hub_client_telemetry_get_publish_topic(
          &client, NULL, telemetry_topic, sizeof(telemetry_topic), NULL)))
  {
    Logger.Error("Failed az_iot_hub_client_telemetry_get_publish_topic");
    return;
  }

  generateTelemetryPayload();

  if (esp_mqtt_client_publish(
          mqtt_client,
          telemetry_topic,
          (const char*)telemetry_payload.c_str(),
          telemetry_payload.length(),
          MQTT_QOS1,
          DO_NOT_RETAIN_MSG)
      == 0)
  {
    Logger.Error("Failed publishing");
  }
  else
  {
    Logger.Info("Message published successfully");
  }
}



// ======== ECE445 CODE==========  //

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

int showPower = 1; // dispaly vrms + irms / p_real + p_app flag

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

void printUploadSuccessToLCD() {
  if (useLCD) {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("UPLOADED DATA");
    lcd.setCursor(0, 1);
    lcd.print("SUCCESSFULLY");
  }
}

void printPowerToLCD(float p_real, float p_app) {
  if (useLCD) {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("REAL:" + String(p_real, 5) + " W");
    lcd.setCursor(0, 1);
    lcd.print("APP:" + String(p_app, 5) + " VA");
  }
}

void printRMSToLCD(float v_rms, float i_rms) {
  if (useLCD) {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("VRMS:" + String(v_rms, 5) + " V");
    lcd.setCursor(0, 1);
    lcd.print("IRMS:" + String(i_rms, 5) + " A");
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

void setup() {
  establishConnection(); 
  setLCDSettings();
  Wire.begin();
  Wire.setClock(100000);
  setADCSettings();
}

void loop()
{
  if (WiFi.status() != WL_CONNECTED)
  {
    connectToWiFi();
  }
#ifndef IOT_CONFIG_USE_X509_CERT
  else if (sasToken.IsExpired())
  {
    Logger.Info("SAS token expired; reconnecting with a new one.");
    (void)esp_mqtt_client_destroy(mqtt_client);
    initializeMqttClient();
  }
#endif
  else if (millis() > next_telemetry_send_time_ms)
  {
    sendTelemetry();
    next_telemetry_send_time_ms = millis() + TELEMETRY_FREQUENCY_MILLISECS;
  }
  // ========= ECE 445 CODE ===========
  if (useSimulator == 1) {
    v = v_amplitude * sin(2 * PI * v_freq * (currentIndex * sampleRateMS * 0.001) + v_phaseShift);
    i = i_amplitude * sin(2 * PI * i_freq * (currentIndex * sampleRateMS * 0.001) + i_phaseShift);
  } else {
    // take instantanous measurements
    v = read_A0();
    float startMeasure = micros();
    i = read_A1();
    float endMeasure = micros();
    // sampleDelay = (endMeasure - startMeasure) * 0.001; // time to measure i in ms
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
    if (currentIndex % 800 == 0 && currentIndex != 0) {
      if (showPower == 1) {
        printPowerToLCD(calibrated_p_real, p_app);
        showPower = 0;
      } else {
        printRMSToLCD(v_rms, i_rms);
        showPower = 1;
      }
    }
    // overwrite old
    instantaneous_power_samples[currentIndex] = new_instant_power;
    // overwrite old
    v_samples_squared[currentIndex] = new_v_squared;
    i_samples_squared[currentIndex] = new_i_squared;

    if (currentIndex % 10 == 0) { // record every 10th sample, so that's 170 Hz / 10 = 17 Hz record rate
      // add to upload array for each value, then combine all that into one json
      getTime().toCharArray(time_arr[cloudIndex], sizeof(time_arr[cloudIndex]));
      vrms_arr[cloudIndex] = v_rms;
      irms_arr[cloudIndex] = i_rms;
      p_real_arr[cloudIndex] = calibrated_p_real;
      p_app_arr[cloudIndex] = p_app;
      cloudIndex = (cloudIndex + 1) % cloudArraySize;
    }
    
    currentIndex = (currentIndex + 1) % sampleSize;

  }
}

String getTime()
{
  struct tm *ptm;
  time_t now = time(NULL);

  ptm = localtime(&now);

  char buffer[20]; // Assuming the formatted time will fit in 20 characters

  sprintf(buffer, "%d/%02d/%02d %02d:%02d:%02d",
          ptm->tm_year + UNIX_EPOCH_START_YEAR,
          ptm->tm_mon + 1,
          ptm->tm_mday,
          ptm->tm_hour,
          ptm->tm_min,
          ptm->tm_sec);

  return String(buffer);
}

