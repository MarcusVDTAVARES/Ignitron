#define CONFIG_LITTLEFS_SPIFFS_COMPAT


#include <Arduino.h>
#include <NimBLEDevice.h> // github NimBLE
#include <SPI.h>
#include <Wire.h>
#include <string>
#include <FastLED.h>

#include "src/SparkButtonHandler.h"
#include "src/SparkDataControl.h"
#include "src/SparkDisplayControl.h"
#include "src/SparkLEDControl.h"
#include "src/SparkPresetControl.h"

using namespace std;

// Device Info Definitions
const string DEVICE_NAME = "Ignitron";

// Control classes
SparkDataControl spark_dc;
SparkButtonHandler spark_bh;
SparkLEDControl spark_led;
SparkDisplayControl spark_display;
SparkPresetControl &presetControl = SparkPresetControl::getInstance();

unsigned long lastInitialPresetTimestamp = 0;
unsigned long currentTimestamp = 0;
int initialRequestInterval = 3000;

#define NUM_LEDS 1
#define RGB_LED_PIN 48
#define BRIGHTNESS  50  // Adjust brightness (0-255)
#define LED_TYPE    SK6812  //WS2812B
#define COLOR_ORDER GRB
CRGB leds[NUM_LEDS];


/* 
// Task handle for the LED thread
TaskHandle_t LEDTaskHandle;

void LED_Task(void *pvParameters) {
    uint8_t hue = 0; // Start hue at 0
    while (true) {
        leds[0] = CHSV(hue++, 255, BRIGHTNESS);
        FastLED.show();
        vTaskDelay(pdMS_TO_TICKS(100)); // Prevent CPU overload
    }
}
 */





// Check for initial boot
bool isInitBoot;
int operationMode = SPARK_MODE_APP;



#define OLED_SDA 6  // OLED I2C pins
#define OLED_SCL 7  // OLED I2C pins
#define i2c_Address 0x3c //initialize with the I2C addr 0x3C Typically eBay OLED's
#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 64 // OLED display height, in pixels
#define OLED_RESET -1   //   QT-PY / XIAO
Adafruit_SH1106G display = Adafruit_SH1106G(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

/////////////////////////////////////////////////////////
//
// INIT AND RUN
//
/////////////////////////////////////////////////////////

void setup() {
    Serial.setRxBufferSize(1024); 
    //delay(2000);  // Give time
    Serial.begin(115200);
    //delay(2000);  // Give time
    Serial.println("STARTUP");

    FastLED.addLeds<LED_TYPE, RGB_LED_PIN, COLOR_ORDER>(leds, NUM_LEDS);
    FastLED.setBrightness(BRIGHTNESS);    
    
    Wire.begin(OLED_SDA, OLED_SCL, 400000);

    leds[0] = CRGB::Blue;
    FastLED.show();
    Serial.println("STARTUP: BLUE LED is ON");
    delay(500);

    Serial.println("Initializing");


    if (!LittleFS.begin(true)) {
        Serial.println("❌ LittleFS Mount failed. Restarting...");
        
        for (int i = 0; i < 5; i++) { // Blink 5 times to indicate failure
            leds[0] = CRGB::Red;
            FastLED.show();
            Serial.println("STARTUP: BLUE LED is ON");
            delay(500);  // Give time
            leds[0] = CRGB::Black;
            FastLED.show();
            Serial.println("STARTUP: LED is OFF");
            delay(500);  // Give time
        }
        ESP.restart(); // Restart board
    }
    
    // spark_dc = new SparkDataControl();
    spark_bh.setDataControl(&spark_dc);

    Serial.println("Checking boot operation mode...");
    operationMode = spark_bh.checkBootOperationMode();
    Serial.printf("Boot operation mode: %d\n", operationMode);
    

    // Setting operation mode before initializing
    operationMode = spark_dc.init(operationMode);
    spark_bh.configureButtons();
    Serial.printf("Operation mode: %d\n", operationMode);

    switch (operationMode) {
    case SPARK_MODE_APP:
        Serial.println("======= Entering APP mode =======");
        break;
    case SPARK_MODE_AMP:
        Serial.println("======= Entering AMP mode =======");
        break;
    case SPARK_MODE_KEYBOARD:
        Serial.println("======= Entering Keyboard mode =======");
        break;
    }
    
    spark_display.setDataControl(&spark_dc);
    spark_dc.setDisplayControl(&spark_display);
    spark_display.init(operationMode);
    // Assigning data control to buttons;
    spark_bh.setDataControl(&spark_dc);
    // Initializing control classes
    spark_led.setDataControl(&spark_dc);

    Serial.println("Initialization done.");


/* 
    // Create a FreeRTOS task for the LED color cycling
    BaseType_t result = xTaskCreatePinnedToCore(
        LED_Task,        // Task function
        "LED_Task",      // Task name
        2048,            // Stack size (increased)
        NULL,            // Task parameters
        1,               // Priority (higher number = higher priority)
        &LEDTaskHandle,  // Task handle
        1                // Run on Core 1 (ESP32 has 2 cores: 0 and 1)
    ); */

    
    leds[0] = CRGB::Black;
    FastLED.show();
    Serial.println("STARTUP: LED is OFF");

}

void loop() {

    // Methods to call only in APP mode
    if (operationMode == SPARK_MODE_APP) {
        while (!(spark_dc.checkBLEConnection())) {
            spark_display.update(spark_dc.isInitBoot());
            spark_led.updateLEDs();
            spark_bh.readButtons();
        }

        // After connection is established, continue.
        //  On first boot, get the amp type and set the preset to Hardware setting 1.

        if (spark_dc.isInitBoot()) { // && !spark_dc.isInitHWRead()) {
            // This is only done once after the connection has been established
            // Read AMP name to determine special parameters
            // TEST: spark_dc.getSerialNumber();
            spark_dc.getAmpName();
            // delay(100);
            // spark_dc.getCurrentPresetFromSpark();
            spark_dc.isInitBoot() = false;
            // spark_dc.configureLooper();
        }
    }

    // Check if presets have been updated (not needed in Keyboard mode)
    if (operationMode != SPARK_MODE_KEYBOARD) {
        spark_dc.checkForUpdates();
    }
    // Reading button input
    spark_bh.configureButtons();
    spark_bh.readButtons();
#ifdef ENABLE_BATTERY_STATUS_INDICATOR
    // Update battery level
    spark_dc.updateBatteryLevel();
#endif
    // Update LED status
    spark_led.updateLEDs();
    // Update display
    spark_display.update();
}
