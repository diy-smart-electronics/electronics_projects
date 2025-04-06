#include <DHT.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SH110X.h>
#include <Adafruit_SCD30.h>
#include <PMS.h>
#include <HardwareSerial.h>
#include <Adafruit_CCS811.h>
#include <SPI.h>
#include <TFT_eSPI.h>
#include <TS_Display.h>


/* Global definitions */
#define SCREEN_WIDTH 320  /* TFT Display screen dimensions */
#define SCREEN_HEIGHT 240
#define FONT_SIZE 4

/* Pins */
#define DHT22_PIN  4 /* ESP32 pin GPIO4 connected to DHT22 temperature sensor */
#define XPT2046_IRQ 36   // T_IRQ touch screen pins start
#define XPT2046_MOSI 32  // T_DIN
#define XPT2046_MISO 39  // T_OUT
#define XPT2046_CLK 25   // T_CLK
#define XPT2046_CS 33    // T_CS touch screen pins end
#define SEN0564_PIN 34   // Co sensor pin connected to GPIO34

/* Declarations */
Adafruit_SCD30  scd30;       /* SCD30 CO2 Sensor */
DHT dht22(DHT22_PIN, DHT22); /* DHT22 Temperature Sensor */
Adafruit_CCS811 ccs;         /* CCS811 TVOC Sensor */
HardwareSerial MySerial(2);  /* Configure UART1 for PMS7002 */
PMS pms(MySerial);           /* Assign PMS7003 to write in UART1 */
PMS::DATA data;             /* PMS7002 Paticulate Matter Sensor */
TFT_eSPI tft = TFT_eSPI();  /* SPI touch display */
SPIClass touchscreenSPI = SPIClass(VSPI); /* SPI touch display */
XPT2046_Touchscreen touchscreen(XPT2046_CS, XPT2046_IRQ); /* SPI touch display */

/* debugger */
bool debug_mode= false;


/* Variables */
int counter = 0;
int pmsreadinterval = 30;
int pmsreadcounter = 0;
bool pmsreader;
bool pmswakeup = true;

/* SCD30 sensor*/
int co2_sensor(int in_co2)
{  
  int co2 = in_co2;
  if (scd30.dataReady()){
    if (!scd30.read()){ Serial.println("Error reading sensor data"); return 0; }
    co2 = scd30.CO2;
    if(debug_mode)  
    {     
      Serial.println("");
      Serial.print("CO2: "); 
      Serial.print(co2);
      Serial.println("");        
    }
  }
  return co2;
}
/* DHT22 Sensor */
void dht22_sensor(int *temp,int *humid)
{
  float humidity  = dht22.readHumidity();                 /* read humidity   */
  float temperature = dht22.readTemperature();            /* read temperature in Celsius */

  if ( isnan(temperature) || isnan(humidity)) {          /* check if the reading is successful or not  */
    if(debug_mode) 
    {
      Serial.println("Failed to read from DHT22 sensor!");
    }
  } 
  else {
    *temp = (int)temperature;
    *humid = (int)humidity;
    if(debug_mode) 
    {
      Serial.println(""); 
      Serial.print("Humidity: ");
      Serial.print((int)humidity);
      Serial.print("%");
      Serial.print(" Temperature: ");
      Serial.print((int)temperature);
      Serial.print("°C");
      Serial.println("");
    }  
  }
}

/* CCS811 Sensor */
int ccs811_sensor(int in_tvoc)
{
  int tvoc = in_tvoc;
  if (ccs.available()) {
    if (!ccs.readData()) {                        
      tvoc = ccs.getTVOC();
      if(debug_mode)
      {
        Serial.println("");
        Serial.print("TVOC: ");
        Serial.print(tvoc);
        Serial.print(" ppb");
        Serial.println("");
      }
    return tvoc;        
    }
  }
}

/* SEN0564 CO Sensor */
int sen0564_co_sensor()
{
  int co_value = 0;
  co_value = analogRead(SEN0564_PIN);
  if (co_value <= 90)
    co_value = 0;
  if(debug_mode)
      {
        Serial.println("");
        Serial.print("CO: ");
        Serial.print(co_value);
        Serial.println("");
      }
  return co_value;

}

/* PMS7003 Sensor */
void pms7003_sensor(int *p_1_0,int *p_2_5,int *p_10_0)
{	
	while (MySerial.available()) { MySerial.read(); } /* clears the buffer */	
	pms.requestRead();
	if(debug_mode)
    {
      Serial.println("PMS sensor going to read data");    
    }
	pmsreader = pms.readUntil(data);
	
  *p_1_0 = (int)data.PM_AE_UG_1_0;
  *p_2_5 = (int)data.PM_AE_UG_2_5;
  *p_10_0 =(int)data.PM_AE_UG_10_0;

  if(debug_mode)
  {
	/* PMS7003 Debugger */
    Serial.println("PM 1.0 (ug/m3): ");
    Serial.println(data.PM_AE_UG_1_0);
    Serial.print("PM 2.5 (ug/m3): ");
    Serial.println(data.PM_AE_UG_2_5);
    Serial.print("PM 10.0 (ug/m3): ");
    Serial.println(data.PM_AE_UG_10_0);
    Serial.println();
    Serial.println("PMS sensor going to sleep");
  }
	pms.sleep();
	pmswakeup=false;
	pmsreadcounter=0;
	counter=0;
}

void printTouchToSerial(int touchX, int touchY, int touchZ) {
  Serial.print("X = ");
  Serial.print(touchX);
  Serial.print(" | Y = ");
  Serial.print(touchY);
  Serial.print(" | Pressure = ");
  Serial.print(touchZ);
  Serial.println();
}

void initialise_display()
{
  tft.fillScreen(TFT_BLACK);
  tft.drawRect(0, 0, 120, 100, TFT_GREEN);            /* co2 box */

  tft.drawRect(0, 100, 120, 100, TFT_GREEN);          /* pm2.5 box */
  tft.drawRect(0, 200, SCREEN_WIDTH, 40, TFT_GREEN);  /* scale box */
  tft.drawRect(120, 0, 120, 100, TFT_GREEN);          /* tvoc box */
  tft.drawRect(120, 100, 120, 100, TFT_GREEN);        /* pm10.0 box */
  tft.drawRect(240, 0, 80, 200, TFT_GREEN);           /* right most box */

  tft.drawRect(240, 0, 80, 50, TFT_GREEN);            /* temperature box */
  tft.drawRect(240, 50, 80, 50, TFT_GREEN);           /* humidity box */
  tft.drawRect(240, 100, 80, 50, TFT_GREEN);           /* CO box*/
  
  tft.setTextColor(TFT_WHITE, TFT_WHITE);
  tft.drawString("CO2", 10, 10, 4);

  tft.setTextColor(TFT_WHITE, TFT_WHITE);
  tft.drawString("PM2.5", 10, 110, 4);

  tft.setTextColor(TFT_WHITE, TFT_WHITE);
  tft.drawString("PM10.0", 130, 110, 4);

  tft.setTextColor(TFT_WHITE, TFT_WHITE);
  tft.drawString("TVOC", 130, 10, 4);

  tft.setTextColor(TFT_WHITE, TFT_WHITE);
  tft.drawString("TEMP.", 250, 5, 2);

  tft.setTextColor(TFT_WHITE, TFT_WHITE);
  tft.drawString("HUMID.", 250, 55, 2);

  tft.setTextColor(TFT_WHITE, TFT_WHITE);
  tft.drawString("CO", 250, 105, 2);

  tft.setTextColor(TFT_WHITE, TFT_WHITE);
  tft.drawString("Scale", 40, 210, 2);

  tft.fillRect(80, 214, 30, 10, TFT_GREEN);
  tft.fillRect(110, 214, 30, 10, TFT_YELLOW);
  tft.fillRect(140, 214, 30, 10, TFT_ORANGE);
  tft.fillRect(170, 214, 30, 10, TFT_RED);
}

void setup() {
  
  Serial.begin(9600);
  MySerial.begin(9600, SERIAL_8N1, 16, 17); /* serial for pms7003 sensor */

  /*------- Initializations ---------*/
  if (!scd30.begin()) { Serial.println("Failed to start sensor! SCD30");}    
  if (!ccs.begin()) { Serial.println("Failed to start sensor! CCS811");}  
  pms.passiveMode();
  pms.wakeUp();
  dht22.begin();

    // Start the SPI for the touchscreen and init the touchscreen
  touchscreenSPI.begin(XPT2046_CLK, XPT2046_MISO, XPT2046_MOSI, XPT2046_CS);
  touchscreen.begin(touchscreenSPI);
  // Set the Touchscreen rotation in landscape mode
  // Note: in some displays, the touchscreen might be upside down, so you might need to set the rotation to 3: touchscreen.setRotation(3);
  touchscreen.setRotation(3);
  // Start the tft display
  tft.init();
  // Set the TFT display rotation in landscape mode
  tft.setRotation(3);
  // Clear the screen before writing to it
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  
  // Set X and Y coordinates for center of display
  int centerX = SCREEN_WIDTH / 2;
  int centerY = SCREEN_HEIGHT / 2;
  
  delay(2000); /* wait two seconds for initializing */
  if(debug_mode)
    Serial.println("Program Started...."  );
  initialise_display();
}

int co2 = 0;
int temperature = 0;
int humidity = 0;
int tvoc = 0;
int p_1_0 = 0;
int p_2_5 = 0;
int p_10_0 = 0;
int co = 0;

void loop() {

  co2 = co2_sensor(co2);                   /* Read CO2 readings */
  dht22_sensor(&temperature,&humidity);    /* Read temperature,humidity readings */
  tvoc = ccs811_sensor(tvoc);              /* Read tvoc readings */
  co = sen0564_co_sensor();         /* Read CO sensore readings */

  /* PMS7003 passive read handling. It is required to preserve longitivity of the sensor */
  if (counter == pmsreadinterval)
  {
    pms.wakeUp();
    pmswakeup=true;
    pmsreadcounter=0;
    counter = 0;
    pmsreadinterval = 100; //make it 450, for every 15mins
    if(debug_mode)
      {
        Serial.println("PMS sensor going to wakeup");
      }
  }

  if (pmswakeup)
  {
    pmsreadcounter++;
  }

  if (pmsreadcounter == 19)
  {
    pms7003_sensor(&p_1_0,&p_2_5,&p_10_0);
  } 
    
  delay(2000);  /* wait for 2 seconds for the next readings from sensors */
  counter++;
  
  tft.setTextWrap(false, true);

  if(co2 <= 1000)
    tft.setTextColor(TFT_GREEN, TFT_BLACK);
  if(co2 > 1000 && co2 <= 2000)
    tft.setTextColor(TFT_YELLOW, TFT_BLACK);
  if(co2 > 2000 && co2 < 5000)
    tft.setTextColor(TFT_ORANGE, TFT_BLACK);
  if(co2 > 5000)
    tft.setTextColor(TFT_RED, TFT_BLACK);    
  tft.setTextPadding(55);
  tft.fillRect(10, 60, 100, 30, TFT_BLACK);
  if(co2 >= 9999)
    tft.drawString(String(co2), 10, 60, 4);
  else
    tft.drawString(String(co2), 60, 60, 4);

  if(p_2_5 <= 25)
    tft.setTextColor(TFT_GREEN, TFT_BLACK);
  if(p_2_5 > 25 && p_2_5 <= 50)
    tft.setTextColor(TFT_YELLOW, TFT_BLACK);
  if(p_2_5 > 50 && p_2_5 <= 100)
    tft.setTextColor(TFT_ORANGE, TFT_BLACK);
  if(p_2_5 > 100)
    tft.setTextColor(TFT_RED, TFT_BLACK);   
  tft.setTextPadding(55);
  tft.fillRect(10, 160, 100, 30, TFT_BLACK);
  if(p_2_5 >= 9999)
    tft.drawString(String(p_2_5), 10, 160, 4);
  else
    tft.drawString(String(p_2_5), 60, 160, 4);

  if(p_10_0 <= 50)
    tft.setTextColor(TFT_GREEN, TFT_BLACK);
  if(p_10_0 > 50 && p_2_5 <= 100)
    tft.setTextColor(TFT_YELLOW, TFT_BLACK);
  if(p_10_0 > 100 && p_2_5 <= 150)
    tft.setTextColor(TFT_ORANGE, TFT_BLACK);
  if(p_10_0 > 150)
    tft.setTextColor(TFT_RED, TFT_BLACK);
  tft.setTextPadding(55);
  tft.fillRect(130, 160, 100, 30, TFT_BLACK);
  if(p_10_0 >= 9999)
    tft.drawString(String(p_10_0), 130, 160, 4);
  else
    tft.drawString(String(p_10_0), 180, 160, 4);

  if(tvoc <= 400)
    tft.setTextColor(TFT_GREEN, TFT_BLACK);
  if(tvoc > 400 && tvoc <= 2200)
    tft.setTextColor(TFT_YELLOW, TFT_BLACK);
  if(tvoc > 2200)
    tft.setTextColor(TFT_RED, TFT_BLACK);
  tft.setTextPadding(55);
  tft.fillRect(130, 60, 100, 30, TFT_BLACK);
  if(tvoc >= 9999)
    tft.drawString(String(tvoc), 130, 60, 4);
  else
    tft.drawString(String(tvoc), 180, 60, 4);

  if(temperature >=18 && temperature <=27)
    tft.setTextColor(TFT_GREEN, TFT_BLACK);
  if(temperature >=4 && temperature <18)
    tft.setTextColor(TFT_YELLOW, TFT_BLACK); 
  if(temperature <4 || temperature >27)
    tft.setTextColor(TFT_RED, TFT_BLACK);     
  tft.setTextPadding(20);
  tft.drawString(String(temperature)+" C", 285, 30, 2);

  if(humidity >= 30 && humidity <= 50)
    tft.setTextColor(TFT_GREEN, TFT_BLACK);
  if(humidity < 30)
    tft.setTextColor(TFT_YELLOW, TFT_BLACK);
  if(humidity > 50 && co2 <= 70)
    tft.setTextColor(TFT_ORANGE, TFT_BLACK);
  if(humidity > 70)
    tft.setTextColor(TFT_RED, TFT_BLACK);
  tft.setTextPadding(20);
  tft.drawString(String(humidity)+" %", 285, 80, 2);

  if(co >= 0 && co <=120)
    tft.setTextColor(TFT_GREEN, TFT_BLACK);
  if(co > 120)    
    tft.setTextColor(TFT_RED, TFT_BLACK);
  tft.setTextPadding(33);
  tft.fillRect(250, 130, 60, 15, TFT_BLACK);
  if(co > 9999)
    tft.drawString(String(co), 250, 130, 2);
  else
    tft.drawString(String(co), 285, 130, 2);
  


  if (touchscreen.tirqTouched() && touchscreen.touched()) {
    int x,y,z;
    // Get Touchscreen points
    TS_Point p = touchscreen.getPoint();
    // Calibrate Touchscreen points with map function to the correct width and height
    x = map(p.x, 200, 3700, 1, SCREEN_WIDTH);
    y = map(p.y, 240, 3800, 1, SCREEN_HEIGHT);
    z = p.z;


    tft.fillRect(35, 30, 250, 180, TFT_BLACK);
    tft.drawRect(35, 30, 250, 180, TFT_GREEN);


    if((x > 0 && x < 110) && (y > 0 && y < 100)) /* CO2 Reference Data */
      {
        tft.setTextColor(TFT_WHITE, TFT_BLACK);
        tft.drawString("CO2", 45, 40, 4);
        
        tft.fillRect(55, 80, 30, 10, TFT_GREEN);
        tft.drawString("Good.", 88, 78, 2);
        tft.drawString("400 - 1000.", 125, 78, 2);
        tft.fillRect(55, 100, 30, 10, TFT_YELLOW);
        tft.drawString("Fair.", 88, 98, 2);
        tft.drawString("1000 - 2000.", 125, 98, 2);
        tft.fillRect(55, 120, 30, 10, TFT_ORANGE);
        tft.drawString("Poor.", 88, 118, 2);
        tft.drawString("2000 - 5000.", 125, 118, 2);
        tft.fillRect(55, 140, 30, 10, TFT_RED);
        tft.drawString("Hazard.", 88, 138, 2);
        tft.drawString("more than 5000.", 140, 138, 2);
      }

    if((x > 120 && x < 230) && (y > 0 && y < 100)) /* TVOC Reference Data */
      {
        tft.setTextColor(TFT_WHITE, TFT_BLACK);
        tft.drawString("TVOC", 45, 40, 4);

        tft.fillRect(55, 80, 30, 10, TFT_GREEN);
        tft.drawString("Good.", 88, 78, 2);
        tft.drawString("0 - 400.", 125, 78, 2);
        tft.fillRect(55, 100, 30, 10, TFT_YELLOW);
        tft.drawString("Fair.", 88, 98, 2);
        tft.drawString("400 - 2200.", 125, 98, 2);
        tft.fillRect(55, 120, 30, 10, TFT_RED);
        tft.drawString("Hazard.", 88, 118, 2);
        tft.drawString("more than 2200", 140, 118, 2);
      }

    if((x > 0 && x < 110) && (y > 100 && y < 180)) /* PM2.5 Reference Data */
      {
        tft.setTextColor(TFT_WHITE, TFT_BLACK);
        tft.drawString("PM2.5", 45, 40, 4);

        tft.fillRect(55, 80, 30, 10, TFT_GREEN);
        tft.drawString("Good.", 88, 78, 2);
        tft.drawString("less than 25.", 125, 78, 2);
        tft.fillRect(55, 100, 30, 10, TFT_YELLOW);
        tft.drawString("Fair.", 88, 98, 2);
        tft.drawString("25 - 50.", 125, 98, 2);
        tft.fillRect(55, 120, 30, 10, TFT_ORANGE);
        tft.drawString("Poor.", 88, 118, 2);
        tft.drawString("50 - 100.", 125, 118, 2);
        tft.fillRect(55, 140, 30, 10, TFT_RED);
        tft.drawString("Hazard.", 88, 138, 2);
        tft.drawString("more than 100.", 140, 138, 2);         
      }

    if((x > 120 && x < 220) && (y > 100 && y < 180)) /* PM10.0 Reference Data */
      {
        tft.setTextColor(TFT_WHITE, TFT_BLACK);
        tft.drawString("PM10.0", 45, 40, 4);

        tft.fillRect(55, 80, 30, 10, TFT_GREEN);
        tft.drawString("Good.", 88, 78, 2);
        tft.drawString("less than 50.", 125, 78, 2);
        tft.fillRect(55, 100, 30, 10, TFT_YELLOW);
        tft.drawString("Fair.", 88, 98, 2);
        tft.drawString("50 - 100.", 125, 98, 2);
        tft.fillRect(55, 120, 30, 10, TFT_ORANGE);
        tft.drawString("Poor.", 88, 118, 2);
        tft.drawString("100 - 150.", 125, 118, 2);
        tft.fillRect(55, 140, 30, 10, TFT_RED);
        tft.drawString("Hazard.", 88, 138, 2);
        tft.drawString("more than 150.", 140, 138, 2);          
      }

    if((x > 240 && x < 310) && (y > 50 && y < 90)) /* HUMID. Reference Data */
      {
        tft.setTextColor(TFT_WHITE, TFT_BLACK);
        tft.drawString("HUMID.", 45, 40, 4);

        tft.fillRect(55, 80, 30, 10, TFT_GREEN);
        tft.drawString("Good.", 88, 78, 2);
        tft.drawString("30 - 50.", 125, 78, 2);
        tft.fillRect(55, 100, 30, 10, TFT_YELLOW);
        tft.drawString("Fair.", 88, 98, 2);
        tft.drawString("less than 30. Too dry.", 125, 98, 2);
        tft.fillRect(55, 120, 30, 10, TFT_ORANGE);
        tft.drawString("Poor.", 88, 118, 2);
        tft.drawString("50 - 70.", 125, 118, 2);
        tft.fillRect(55, 140, 30, 10, TFT_RED);
        tft.drawString("Hazard.", 88, 138, 2);
        tft.drawString("more than 70.", 140, 138, 2);        
      }

    if((x > 240 && x < 320) && (y > 100 && y < 140)) /* CO Reference Data */
      {
        tft.setTextColor(TFT_WHITE, TFT_BLACK);
        tft.drawString("CO", 45, 40, 4);

        tft.fillRect(55, 80, 30, 10, TFT_GREEN);
        tft.drawString("Good.", 88, 78, 2);
        tft.drawString("0 - 120.", 125, 78, 2);
        tft.fillRect(55, 100, 30, 10, TFT_RED);
        tft.drawString("Hazard.", 88, 98, 2);
        tft.drawString("more than 120.", 140, 98, 2);                
      }    
      
    
    delay(5000);
    initialise_display();
  }
  
}
