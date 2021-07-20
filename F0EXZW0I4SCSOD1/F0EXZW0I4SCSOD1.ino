// LIBRARIES
#include <Servo.h>


#include <WiFi.h>                   //Blynk Libraries
#include <WiFiClient.h>
#include <BlynkSimpleEsp32.h>

#include <Wire.h>                   //LED Display libraries
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define BLYNK_PRINT Serial
#define SCREEN_WIDTH 128            // OLED display width, in pixels
#define SCREEN_HEIGHT 64            // OLED display height, in pixels


Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1); // Declaration for an SSD1306 display connected to I2C (SDA, SCL pins), -1 no reset button


// VARIABLES

Servo reServo;
char auth[] = "LlDiGQ5tYh2f93u85fnd7dMOXdKL5V4J";
char ssid[] = "Hidden Network";
char pass[] = "$eR@j112123";
static const int servoPin = 18; 
const int PSENSE = 33; // Analog input pin that the pressure sensor is attached to
const int PSENSE_SUPPLY = 5;
int offset;
int offsetSize;
int meanSize;
int zeroSpan;
int pos = 0;    
int pb1 = 15;
int pb2 = 5; 
int pb3 = 4;
int posDegrees = 0;
int exCount = 0;
int breathCounter = 0;
int selectedMode = 0;      // (1) OFFLINE, (2) ONLINE
int getMode;
int sensorValue = 0;        // value read from the pot
////////////////////////////////////////////////////////////////////////////////////////Smoothing of pressure readings////////
const int numReadings = 3;
int readings[numReadings];
int readIndex = 0;              // the index of the current reading
int total = 0;                  // the running total
int average = 0;  
////////////////////////////////////////////////////////////////////////////////////////Calculate BPM////////////////////////
float minuteTotal;
int breathBuff2 = 0;
int breathBuff1 = 0;
int BPM = 0;
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
float breathTimer;
int fev1 = 0.0;     //not float
int newBreath = 0;
int snatch = 0;
int fundex = 0;
float vol[30]; //this is the total volume that stored with time...
float volSec[30];//this is the the liter/sec messured
float rho = 1.225; //Demsity of air in kg/m3;
float area_1 = 0.0003801; //these are the areas taken carefully from the 3D printed venturi 2M before constriction  D = 10mm = 10*(10^-3) m, A = ((0.5D)^2)*pi
float area_2 = 0.0000785;// this is area of venturi throat
float maxFlow = 0.0;
float TimerNow = 0.0;
float secondsBreath;
int volumeTotal = 0; //not float
float massFlow = 0;
float volFlow = 0;

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


void setup() {
  // initialize serial communications at 115200 bps:
  Serial.begin(115200); 
  //analogReference(EXTERNAL);
  pinMode(pb1,INPUT_PULLUP);
  pinMode(pb2,INPUT_PULLUP);
  pinMode(pb3,INPUT_PULLUP);
  minuteTotal = millis();

  reServo.attach(servoPin);
  delay(100);

  //  for ( int ii =0;ii < offsetSize ;ii ++){
  //  offset += analogRead(PSENSE)-(4096/2) ; // offset for MPXV7002DP
  //  offset = offset/offsetSize; 
      
 if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) { // Address 0x3C for 128x64
    Serial.println(F("SSD1306 allocation failed"));
    for(;;);
  }

  for (int thisReading = 0; thisReading < numReadings; thisReading++) { //zeroing the array
    readings[thisReading] = 0;
  }
  
  //WELCOME SCREEN
  delay(1000);
  display.clearDisplay();
  display.setTextSize(2);
  display.setTextColor(WHITE);
  display.setCursor(0, 5);
  display.println("SMART OPEP");
  display.setCursor(0, 25);
  display.println("_ DEVICE _");  
  display.setTextSize(1);
  display.setCursor(0, 50);
  display.println(" Press OK to start!");
  display.display(); 
  delay(1000);

  //PROMPT USER
  while (1){
    
    int buttonState = digitalRead(pb2);
    Serial.println(buttonState);
    if (buttonState == 0){
      display.clearDisplay();
      display.display();
      break;
     }
    }
   getMode = modeSelection();      
   }


//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
  void loop() {
  reServo.write(posDegrees);

  if (digitalRead(pb2) == 0){
    posDegrees = 30;
   }
  if (digitalRead(pb3) == 0){
    posDegrees = 120;
   }
  if (digitalRead(pb1) == 0){
    posDegrees = 60;
   }

  //Smoothing process //////////////////////////////////////////
  total = total - readings[readIndex];   
  readings[readIndex] = analogRead(PSENSE);       // read from the sensor:
  total = total + readings[readIndex];            // add the reading to the total:
  readIndex = readIndex + 1;
  // if we're at the end of the array...
  if (readIndex >= numReadings) {
    // ...wrap around to the beginning:
    readIndex = 0;
  }
  // calculate the average:
  average = total / numReadings;
  //////////////////////////////////////////////////////////////

  // read the analog in value:
  sensorValue = average;                        //analogRead(PSENSE) if we were not to use smoothing    
  Serial.println(sensorValue);
  
  float sensorVoltageR2 = (3.3 / 4096 * sensorValue) + 0.25;  //Voltage across R2 = 15kOhm /0.2 offset from datasheet
  Serial.println(sensorVoltageR2);
  
  float sensorVoltage = sensorVoltageR2 * PSENSE_SUPPLY/3;  //Sensor output voltage
  Serial.println(sensorVoltage);
  
  float pressureValueOffset = sensorVoltage - 2.5; //pressure equation from MPXV7002DP datasheet
                                                  //sensor behavior is assumed to be linear, +-6.25% accuracy
  float pressureValuekPa = (sensorVoltage - 2.5); //- 0.13; //de-offset
  Serial.print(pressureValuekPa);Serial.println(" kPa");
      
  delay(2); 

  display.clearDisplay();
  display.setCursor(0, 0);
  display.println("P (kPa)"); //display a static text
  display.setCursor(50, 0);
  display.println(pressureValuekPa);
  display.display();
  
   if (pressureValuekPa > 0.5){
    
    display.setCursor(0, 15);
    display.println("EXHALATION"); //display a static text
    display.display();
    exCount = exCount + 1;

     if (newBreath < 1) {                         // intiliazing a new breath
        breathTimer = millis();
        fev1 = 0.0;
        snatch = 0;
        fundex = 0;
        /*if(volumeTotal > 100){
        BPM = BPM + 1; //counts breath
        //Serial.print("VolumeTotal= ");
        //Serial.println(volumeTotal);
        }*/
        
        volumeTotal = 0;
        newBreath = 1;
        
      }
  
      if(!(snatch%5)){
        vol[fundex] = volumeTotal;
        volSec[fundex] = volFlow;
        fundex++;
        
      }
      snatch++;
//////////////////////////////

     if((millis() - breathTimer >= 1000) && (fev1 == 0)) fev1 = volumeTotal;

     massFlow = 1000*sqrt( (abs(pressureValuekPa)*2*rho) / ((1/(pow(area_2,2))) - ( 1/(pow(area_1,2)))) ); //mass flow obtained from bernoulli equation (instantaneous value)
     volFlow = massFlow/rho;                                                                               //volumetric flow of air (instantaneous value)
     
     if(volFlow > maxFlow) maxFlow = volFlow;

     volumeTotal = volFlow * (millis() - TimerNow) + volumeTotal;//integrates volumes over units of time to get total volume
    // print the results to the Serial Monitor:
    //Serial.print("sensor = ");
    //Serial.print(sensorValue);
    Serial.print("VolumeTotal= ");
    Serial.println(volumeTotal);
 
    //Serial.print("\t pressure = ");
    //Serial.println(Pa);

   }    

          else if (pressureValuekPa < -0.1){
    display.setCursor(0, 15);
    display.println("INHALATION"); //display a static text
    display.display();
  }

  else {
    display.setCursor(0, 15);
    display.println("WAITING..."); //display a static text
    
      if (exCount >= 1){
        breathCounter = breathCounter + (exCount / exCount);
        exCount = 0;
      }
    
      display.setCursor(0, 25);
      display.println("Breath Count: "); //display a static text
      display.setCursor(80, 25);
      display.println(breathCounter); //display a static text
      display.display();
      
        if((millis()- minuteTotal) > 60000){                       //millis() Returns the number of milliseconds passed since the Arduino board began running the current program. 
        minuteTotal = millis();
        breathBuff1 = breathCounter;
        BPM = breathBuff1 - breathBuff2;
        breathBuff2 = breathBuff1;
        Serial.print("Breaths per minute:");
        Serial.println(BPM);
       // logfile.print("                                        ");
        //logfile.println(totalBreath);
        //totalBreath = 0;
        }

        display.setCursor(0, 35);
        display.println("BPM: "); //display a static text
        display.setCursor(80, 35);
        display.println(BPM); //display a static text
        display.display(); 

         if(newBreath){    //if your done figure out the results
          newBreath = 0;
          //Serial.print("time for breath");
          if(volumeTotal > 200){
          secondsBreath = (millis() - breathTimer)/1000;
          Serial.print("FEV1  ");
          Serial.println(fev1);
          Serial.print("FVC  ");
          Serial.print(volumeTotal);
          Serial.print("FV1/FVC");
          Serial.println(fev1/volumeTotal);
          Serial.print("Duration");
          Serial.println(breathTimer/1000);
          Serial.print("MaxFlow");
          Serial.println(maxFlow);

          display.setCursor(0, 45);
          display.println("FEV1:"); //display a static text
          display.setCursor(30, 45);
          display.println(fev1);
          display.setCursor(70, 45);
          display.println("FVC:"); //display a static text
          display.setCursor(90, 45);
          display.println(volumeTotal);
          display.display();

        
          //Serial.println(secondsBreath);
          delay(1500);
     
         //printer();
         //dataDump();
         //if(Blynk.connected())blynkPrint();
         
          }
          for(int p = 0;p < 30; p++){
          vol[p] = 0;
          volSec[p] = 0;
        }
         /* tft.setRotation(1);
          tft.fillScreen(TFT_BLACK);
          tft.setTextColor(TFT_GREEN, TFT_BLACK);
       tft.drawCentreString("BLOW.....",125,55,4); */
          maxFlow = 0;
        
        }
        
        TimerNow = millis();
        delay(20);

      }
    
   delay(100); 
}



 

int modeSelection(){

  delay(1000); 
  display.setTextSize(2);
  display.setTextColor(WHITE);
  display.setCursor(10, 5);
  display.println("MODE:");
  display.setCursor(0, 30);
  display.setTextSize(1);
  display.println("OFFLINE"); 
  display.setCursor(50, 30);
  display.println("ONLINE");
  display.setCursor(0, 50);
  display.println("Press OK to confirm!");
  display.display(); 

    while(1){
  
      if (digitalRead(pb1) == 0){
        display.setTextColor(BLACK, WHITE);
        display.setCursor(0, 30);
        display.println("OFFLINE");
        display.setTextColor(WHITE, BLACK);
        display.setCursor(50, 30);
        display.println("ONLINE");
        display.display(); 
  
        selectedMode = 1;
  
      }

      if (digitalRead(pb2) == 0){                //OK button
        display.setTextColor(BLACK, WHITE);
        display.setCursor(0, 30);
        display.println("OFFLINE");
        display.setTextColor(WHITE, BLACK);
        display.setCursor(50, 30);
        display.println("ONLINE");
        display.display(); 

  
        if(selectedMode == 1 || selectedMode == 2){
          return selectedMode;
        }

        else continue;
  
      }
        
  
      if (digitalRead(pb3) == 0){
  
      display.setTextColor(BLACK, WHITE);
      display.setCursor(50, 30);
      display.println("ONLINE");
      display.setTextColor(WHITE, BLACK);
      display.setCursor(0, 30);
      display.println("OFFLINE");
      display.display(); 
  
      selectedMode = 2;
  


        
      }
    }
 


}
