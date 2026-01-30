/*
 * VEML6030 Light Sensor with ESP32/ESP8266 Web Server (AP Mode)
 * 
 * Hardware Connections:
 * VEML6030 SDA  -> GPIO 8 (or 21 for ESP32)
 * VEML6030 SCL  -> GPIO 9 (or 22 for ESP32)
 * VEML6030 INT  -> GPIO 7 (optional)
 * VEML6030 VDD  -> 3.3V
 * VEML6030 GND  -> GND
 * VEML6030 ADDR -> GND (for I2C address 0x10) or VDD (for 0x48)
 */

#include <Wire.h>

// Choose your board type - uncomment the one you're using
// #define ESP8266_BOARD
#define ESP32_BOARD

#ifdef ESP8266_BOARD
  #include <ESP8266WiFi.h>
  #include <ESP8266WebServer.h>
  ESP8266WebServer server(80);
#else
  #include <WiFi.h>
  #include <WebServer.h>
  WebServer server(80);
#endif

// Pin definitions
#define SDA_PIN 8   // Change to 21 for standard ESP32 I2C
#define SCL_PIN 9   // Change to 22 for standard ESP32 I2C
#define INT_PIN 7   // Optional interrupt pin

// VEML6030 I2C Address
// Use 0x48 if ADDR pin connected to VDD
// Use 0x10 if ADDR pin connected to GND
#define VEML6030_ADDR 0x10

// VEML6030 Register Addresses
#define ALS_CONF      0x00  // Configuration register
#define ALS_WH        0x01  // High threshold
#define ALS_WL        0x02  // Low threshold
#define POWER_SAVING  0x03  // Power saving mode
#define ALS           0x04  // ALS output data
#define WHITE         0x05  // White channel output data
#define ALS_INT       0x06  // Interrupt status

// Access Point credentials
const char* ap_ssid = "VEML6030_Sensor";
const char* ap_password = "12345678";  // Minimum 8 characters

// Global variables for sensor data
float luxValue = 0.0;
uint16_t alsRawValue = 0;
uint16_t whiteRawValue = 0;
bool sensorInitialized = false;

// Function prototypes
void initVEML6030();
void readVEML6030();
void writeRegister(uint8_t reg, uint16_t value);
uint16_t readRegister(uint8_t reg);
void handleRoot();
void handleData();

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("\n\nVEML6030 Light Sensor Web Server");
  
  // Initialize I2C with custom pins
  Wire.begin(SDA_PIN, SCL_PIN);
  delay(100);
  
  // Initialize VEML6030 sensor
  initVEML6030();
  
  // Setup WiFi Access Point
  Serial.println("Setting up Access Point...");
  WiFi.mode(WIFI_AP);
  WiFi.softAP(ap_ssid, ap_password);
  
  IPAddress IP = WiFi.softAPIP();
  Serial.println("\n=================================");
  Serial.print("AP SSID: ");
  Serial.println(ap_ssid);
  Serial.print("AP Password: ");
  Serial.println(ap_password);
  Serial.print("AP IP address: ");
  Serial.println(IP);
  Serial.println("=================================\n");
  Serial.println("Connect to the AP and open:");
  Serial.print("http://");
  Serial.println(IP);
  Serial.println();
  
  // Setup web server routes
  server.on("/", handleRoot);
  server.on("/data", handleData);
  
  // Start server
  server.begin();
  Serial.println("HTTP server started");
}

void loop() {
  // Handle client requests
  server.handleClient();
  
  // Read sensor data every 500ms
  static unsigned long lastRead = 0;
  if (millis() - lastRead >= 500) {
    readVEML6030();
    lastRead = millis();
    
    // Print to Serial for debugging
    Serial.print("Light Level: ");
    Serial.print(luxValue);
    Serial.print(" lux | ALS Raw: ");
    Serial.print(alsRawValue);
    Serial.print(" | White Raw: ");
    Serial.println(whiteRawValue);
  }
}

// Initialize VEML6030 sensor
void initVEML6030() {
  Serial.println("Initializing VEML6030...");
  
  // Configuration:
  // Bit 15-13: Reserved (000)
  // Bit 12-11: ALS_GAIN = 00 (gain x1)
  // Bit 10: Reserved (0)
  // Bit 9-6: ALS_IT = 0000 (100ms integration time)
  // Bit 5-4: ALS_PERS = 00 (persistence = 1)
  // Bit 3-2: Reserved (00)
  // Bit 1: ALS_INT_EN = 0 (interrupt disabled)
  // Bit 0: ALS_SD = 0 (power on)
  
  uint16_t config = 0x0000;  // Default: gain x1, 100ms, power on
  writeRegister(ALS_CONF, config);
  delay(100);
  
  // Verify sensor is responding
  uint16_t readBack = readRegister(ALS_CONF);
  if (readBack == config) {
    Serial.println("VEML6030 initialized successfully!");
    Serial.print("I2C Address: 0x");
    Serial.println(VEML6030_ADDR, HEX);
    sensorInitialized = true;
  } else {
    Serial.println("ERROR: VEML6030 initialization failed!");
    Serial.print("Expected: 0x");
    Serial.print(config, HEX);
    Serial.print(" Got: 0x");
    Serial.println(readBack, HEX);
    sensorInitialized = false;
  }
}

// Read sensor data
void readVEML6030() {
  if (!sensorInitialized) {
    Serial.println("Sensor not initialized, retrying...");
    initVEML6030();
    return;
  }
  
  // Read ALS (Ambient Light) register
  alsRawValue = readRegister(ALS);
  
  // Read White channel register
  whiteRawValue = readRegister(WHITE);
  
  // Calculate lux value
  // Formula: lux = raw_value * resolution
  // Default resolution with gain x1 and 100ms integration = 0.0336 lx/bit
  float resolution = 0.0336;  // Change based on your gain and integration time settings
  luxValue = alsRawValue * resolution;
}

// Write 16-bit value to register
void writeRegister(uint8_t reg, uint16_t value) {
  Wire.beginTransmission(VEML6030_ADDR);
  Wire.write(reg);
  Wire.write(value & 0xFF);        // LSB
  Wire.write((value >> 8) & 0xFF); // MSB
  Wire.endTransmission();
}

// Read 16-bit value from register
uint16_t readRegister(uint8_t reg) {
  Wire.beginTransmission(VEML6030_ADDR);
  Wire.write(reg);
  Wire.endTransmission(false);  // Repeated start
  
  Wire.requestFrom(VEML6030_ADDR, (uint8_t)2);
  
  if (Wire.available() >= 2) {
    uint8_t lsb = Wire.read();
    uint8_t msb = Wire.read();
    return (msb << 8) | lsb;
  }
  
  return 0;
}

// Handle root page request
void handleRoot() {
  String html = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>VEML6030 Light Sensor</title>
  <style>
    * {
      margin: 0;
      padding: 0;
      box-sizing: border-box;
    }
    
    body {
      font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif;
      background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
      min-height: 100vh;
      display: flex;
      justify-content: center;
      align-items: center;
      padding: 20px;
    }
    
    .container {
      background: white;
      border-radius: 20px;
      box-shadow: 0 20px 60px rgba(0,0,0,0.3);
      padding: 40px;
      max-width: 600px;
      width: 100%;
    }
    
    h1 {
      text-align: center;
      color: #333;
      margin-bottom: 10px;
      font-size: 2em;
    }
    
    .subtitle {
      text-align: center;
      color: #666;
      margin-bottom: 30px;
      font-size: 0.9em;
    }
    
    .sensor-card {
      background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
      border-radius: 15px;
      padding: 30px;
      margin-bottom: 20px;
      color: white;
      text-align: center;
    }
    
    .sensor-value {
      font-size: 3em;
      font-weight: bold;
      margin: 20px 0;
      text-shadow: 2px 2px 4px rgba(0,0,0,0.2);
    }
    
    .sensor-unit {
      font-size: 1.2em;
      opacity: 0.9;
    }
    
    .info-grid {
      display: grid;
      grid-template-columns: 1fr 1fr;
      gap: 15px;
      margin-top: 20px;
    }
    
    .info-card {
      background: #f8f9fa;
      border-radius: 10px;
      padding: 20px;
      text-align: center;
    }
    
    .info-label {
      color: #666;
      font-size: 0.9em;
      margin-bottom: 5px;
    }
    
    .info-value {
      color: #333;
      font-size: 1.5em;
      font-weight: bold;
    }
    
    .status {
      text-align: center;
      padding: 10px;
      border-radius: 8px;
      margin-top: 20px;
      font-weight: 500;
    }
    
    .status.online {
      background: #d4edda;
      color: #155724;
    }
    
    .status.offline {
      background: #f8d7da;
      color: #721c24;
    }
    
    .light-indicator {
      width: 60px;
      height: 60px;
      border-radius: 50%;
      margin: 0 auto 20px;
      box-shadow: 0 0 20px rgba(255,255,255,0.5);
      transition: all 0.3s ease;
    }
    
    @keyframes pulse {
      0%, 100% { opacity: 1; }
      50% { opacity: 0.7; }
    }
    
    .updating {
      animation: pulse 1s infinite;
    }
  </style>
</head>
<body>
  <div class="container">
    <h1> Light Sensor</h1>
    <div class="subtitle">VEML6030 Ambient Light Sensor</div>
    
    <div class="sensor-card">
      <div class="light-indicator" id="lightIndicator"></div>
      <div class="sensor-value" id="luxValue">--</div>
      <div class="sensor-unit">lux</div>
    </div>
    
    <div class="info-grid">
      <div class="info-card">
        <div class="info-label">ALS Raw</div>
        <div class="info-value" id="alsRaw">--</div>
      </div>
      <div class="info-card">
        <div class="info-label">White Raw</div>
        <div class="info-value" id="whiteRaw">--</div>
      </div>
    </div>
    
    <div class="status online" id="status">
       Connected
    </div>
  </div>

  <script>
    function updateData() {
      fetch('/data')
        .then(response => response.json())
        .then(data => {
          document.getElementById('luxValue').textContent = data.lux.toFixed(2);
          document.getElementById('alsRaw').textContent = data.alsRaw;
          document.getElementById('whiteRaw').textContent = data.whiteRaw;
          
          // Update light indicator color based on lux value
          const indicator = document.getElementById('lightIndicator');
          let brightness = Math.min(data.lux / 1000, 1);
          let color = `rgba(255, 255, 0, ${0.3 + brightness * 0.7})`;
          indicator.style.backgroundColor = color;
          
          // Update status
          document.getElementById('status').className = 'status online';
          document.getElementById('status').textContent = 'Connected';
        })
        .catch(error => {
          console.error('Error:', error);
          document.getElementById('status').className = 'status offline';
          document.getElementById('status').textContent = ' Connection Error';
        });
    }
    
    // Update every 500ms
    setInterval(updateData, 500);
    
    // Initial update
    updateData();
  </script>
</body>
</html>
)rawliteral";
  
  server.send(200, "text/html", html);
}

// Handle data request (JSON)
void handleData() {
  String json = "{";
  json += "\"lux\":" + String(luxValue, 2) + ",";
  json += "\"alsRaw\":" + String(alsRawValue) + ",";
  json += "\"whiteRaw\":" + String(whiteRawValue) + ",";
  json += "\"initialized\":" + String(sensorInitialized ? "true" : "false");
  json += "}";
  
  server.send(200, "application/json", json);
}
