         /////////////////////////////////////////////  
        // IoT Bookmark and Reading (Book Ratings) //
       //          Tracker w/ Qubitro             //
      //             ---------------             //
     //          (Arduino Nano 33 IoT)          //           
    //             by Kutluhan Aktar           // 
   //                                         //
  /////////////////////////////////////////////

//
// While reading, use RFID tags as bookmarks to record your ratings. Then, inspect them on Qubitro to grasp and review chapters deliberately.
//
// For more information:
// https://www.theamplituhedron.com/projects/IoT_Bookmark_and_Reading_Tracker_w_Qubitro
//
//
// Connections
// Arduino Nano 33 IoT :  
//                                MFRC522
// D3  --------------------------- RST
// D4  --------------------------- SDA
// D11 --------------------------- MOSI
// D12 --------------------------- MISO
// D13 --------------------------- SCK
//                                SH1106 OLED Display (128x64)
// D6  --------------------------- SDA
// D5  --------------------------- SCK
// D7  --------------------------- RES
// D8  --------------------------- DC
// D9  --------------------------- CS
//                                JoyStick (R)
// A0  --------------------------- VRY
// A1  --------------------------- VRX
// A2  --------------------------- SW
//                                JoyStick (L)
// A3  --------------------------- VRY
// A4  --------------------------- VRX
// A5  --------------------------- SW
//                                Button (Up)
// A6  --------------------------- S
//                                Button (Right)
// A7  --------------------------- S
//                                Button (Left)
// D2  --------------------------- S
//                                Button (Down)
// D10 --------------------------- S
//                                5mm Green LED
// D0  --------------------------- +


// Include the required libraries:
#include <QubitroMqttClient.h>
#include <WiFiNINA.h> 
#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SH110X.h>
#include <MFRC522.h>

// Initiate the Wi-Fi and MQTT clients.
WiFiClient wifiClient;
QubitroMqttClient mqttClient(wifiClient);

// Define the Wi-Fi settings.
char ssid[] = "<_SSID_>";   
char pass[] = "<_PASSWORD_>";

// Define the Qubitro device settings and information.
char deviceID[] = "<_DEVICE_ID_>";
char deviceToken[] = "<_DEVICE_TOKEN_>";
char host[] = "broker.qubitro.com";
int port = 1883;

// Define the SH1106 screen settings:
#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 64 // OLED display height, in pixels
#define OLED_MOSI     6
#define OLED_CLK      5
#define OLED_DC       8
#define OLED_CS       9
#define OLED_RST      7

// Create the SH1106 OLED screen.
Adafruit_SH1106G display = Adafruit_SH1106G(SCREEN_WIDTH, SCREEN_HEIGHT, OLED_MOSI, OLED_CLK, OLED_DC, OLED_RST, OLED_CS);

// Create the MFRC522 instance.
#define RST_PIN   3
#define SS_PIN    4
MFRC522 mfrc522(SS_PIN, RST_PIN);

// Define the MFRC522 module key input.
MFRC522::MIFARE_Key key;

// Define joystick pins:
#define VRY_1     A0
#define VRX_1     A1
#define SW_1      A2
#define VRY_2     A3
#define VRX_2     A4
#define SW_2      A5

// Define control buttons:
#define b_up      A6
#define b_right   A7
#define b_left    2
#define b_down    10

#define control_led 0

// Define the data holders:
volatile boolean uid_activated = false;
String lastRead = "";
int options[8] = {0, 0, 0, 0, 0, 0};
int joystick_x_1, joystick_x_2, joystick_y_1, joystick_y_2, joystick_sw_1, joystick_sw_2, up, right, left, down;


void setup() {
  // Initialize the serial communication:
  Serial.begin(9600);
  
  pinMode(b_up, INPUT_PULLUP);
  pinMode(b_right, INPUT_PULLUP);
  pinMode(b_left, INPUT_PULLUP);
  pinMode(b_down, INPUT_PULLUP);
  pinMode(SW_1, INPUT);
  pinMode(SW_2, INPUT);
  digitalWrite(SW_1, HIGH);
  digitalWrite(SW_2, HIGH);
  pinMode(control_led, HIGH);
  
  // Initialize the SH1106 screen:
  display.begin(0, true);
  display.display();
  delay(1000);

  display.clearDisplay();   
  display.setTextSize(2); 
  display.setTextColor(SH110X_BLACK, SH110X_WHITE);
  display.setCursor(0,0);
  display.println("IoT");
  display.println("Bookmark");
  display.display();
  delay(1000);

  // Connect to the given Wi-Fi network.
  Serial.println("Connecting to the Wi-Fi network...");
  while(WiFi.begin(ssid, pass) != WL_CONNECTED){
    Serial.print(".");
    delay(1000);
  }
  Serial.println("Connected to the Wi-Fi network!");

  // Set the Qubitro device ID and token for authentication.
  mqttClient.setId(deviceID); 
  mqttClient.setDeviceIdToken(deviceID, deviceToken);
  Serial.println("Connecting to the Qubitro device...");

  // Connect to the Qubitro device via the Qubitro MQTT broker.
  if(!mqttClient.connect(host, port)){
    Serial.println("Qubitro: Connection failed! Error code => ");
    Serial.println(mqttClient.connectError());
    while(1);
  }
  Serial.println("Connected to the Qubitro device!"); 

  // Activate the two-way communication with the Qubitro device.
  mqttClient.onMessage(receivedMessage);                                       
  mqttClient.subscribe(deviceID);

  // Initialize the MFRC522 hardware:
  SPI.begin();          
  mfrc522.PCD_Init();
  Serial.println("\n----------------------------------\nApproximate a New Card or Key Tag : \n----------------------------------\n");
  delay(3000);
}

void loop(){
  read_UID();
  // Maintain the MQTT connection with the Qubitro broker.
  mqttClient.poll(); 
  
  if(lastRead != ""){
    uid_activated = true; 
    // If an RFID card or tag is detected, open the rating settings menu.
    while(uid_activated){
      read_controls();
      mqttClient.poll(); 
      display.clearDisplay();
      display.setTextSize(1);
      display.setTextColor(SH110X_WHITE);
      display.setCursor(0,5);
      display.print("UID: "); display.println(lastRead);
      display.print("Plot :        "); display.println(options[0]);
      display.print("Delineation : "); display.println(options[1]);
      display.print("Immersion :   "); display.println(options[2]);
      display.print("Prolixity :   "); display.println(options[3]);
      display.print("Characters :  "); display.println(options[4]);
      display.print("Editing :     "); display.println(options[5]);
      display.display();
      delay(150);
      // Adjust book ratings by utilizing joysticks and control buttons:
      if(joystick_x_1 >= 850){ if(!up) options[0] = 1; if(!right) options[0] = 2; if(!left) options[0] = 3; if(!down) options[0] = 4; }
      if(joystick_x_1 <= 150){ if(!up) options[1] = 1; if(!right) options[1] = 2; if(!left) options[1] = 3; if(!down) options[1] = 4; }
      if(joystick_y_1 >= 850){ if(!up) options[2] = 1; if(!right) options[2] = 2; if(!left) options[2] = 3; if(!down) options[2] = 4; }
      if(joystick_x_2 >= 850){ if(!up) options[3] = 1; if(!right) options[3] = 2; if(!left) options[3] = 3; if(!down) options[3] = 4; }
      if(joystick_x_2 <= 150){ if(!up) options[4] = 1; if(!right) options[4] = 2; if(!left) options[4] = 3; if(!down) options[4] = 4; }
      if(joystick_y_2 >= 850){ if(!up) options[5] = 1; if(!right) options[5] = 2; if(!left) options[5] = 3; if(!down) options[5] = 4; }
      // Send the given book ratings to the Qubitro device via the Qubitro MQTT broker.
      if(!joystick_sw_2){
       digitalWrite(control_led, HIGH);
       mqttClient.beginMessage(deviceID);   
       // Create a string in the JSON format to transfer data to the Qubitro device successfully.
       mqttClient.print("{\"UID\":\"" + lastRead + "\",\"plot\":" + String(options[0]) + ",\"delineation\":" + String(options[1]) + ",\"immersion\":" + String(options[2]) + ",\"prolixity\":" + String(options[3]) + ",\"characters\":" + String(options[4]) + ",\"editing\":" + String(options[5]) + "}");
       mqttClient.endMessage();             
       delay(3000); 
       Serial.println("Message Sent!");
       // Exit and clear:
       lastRead = "";
       display.clearDisplay();   
       display.setTextSize(2); 
       display.setTextColor(SH110X_BLACK, SH110X_WHITE);
       display.setCursor(0,0);
       display.println("IoT");
       display.println("Bookmark");
       display.display();
       digitalWrite(control_led, LOW);
       uid_activated = false;
      }

      // Exit and clear:
      if(!joystick_sw_1){
          lastRead = "";
          display.clearDisplay();   
          display.setTextSize(2); 
          display.setTextColor(SH110X_BLACK, SH110X_WHITE);
          display.setCursor(0,0);
          display.println("IoT");
          display.println("Bookmark");
          display.display();
          uid_activated = false;
      }
    }
  }
}

int read_UID() {
  // Detect the new card or tag UID. 
  if ( ! mfrc522.PICC_IsNewCardPresent()) { 
    return 0;
  }
  if ( ! mfrc522.PICC_ReadCardSerial()) {
    return 0;
  }

  // Display the detected UID on the serial monitor.
  Serial.print("\n----------------------------------\nNew Card or Key Tag UID : ");
  for (int i = 0; i < mfrc522.uid.size; i++) {
    lastRead += mfrc522.uid.uidByte[i] < 0x10 ? " 0" : " ";
    lastRead += String(mfrc522.uid.uidByte[i], HEX);
  }
  lastRead.trim();
  lastRead.toUpperCase();
  Serial.print(lastRead);
  Serial.print("\n----------------------------------\n\n");
  mfrc522.PICC_HaltA();
  return 1;
}

void read_controls(){
  // Joysticks:
  joystick_x_1 = analogRead(VRX_1);
  joystick_y_1 = analogRead(VRY_1);
  joystick_sw_1 = digitalRead(SW_1);
  joystick_x_2 = analogRead(VRX_2);
  joystick_y_2 = analogRead(VRY_2);
  joystick_sw_2 = digitalRead(SW_2);
  // Buttons:
  up = digitalRead(b_up);
  right = digitalRead(b_right);
  left = digitalRead(b_left);
  down = digitalRead(b_down);
}

void receivedMessage(int messageSize){ Serial.print("Qubitro: A new message is received!\n\n"); }
