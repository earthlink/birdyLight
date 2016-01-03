 /*
    GNU GENERAL PUBLIC LICENSE
    Version 3, 29 June 2007

    Copyright (C) 2007 Free Software Foundation, Inc. <http://fsf.org/>
    Everyone is permitted to copy and distribute verbatim copies
    of this license document, but changing it is not allowed.
  */
  
#include <time.h>
#include <LiquidCrystal.h>
#include <Wire.h> 
#include <Time.h>
#include <math.h>
#include <util/eu_dst.h>
#include "RTClib.h"

#define MY_LATITUDE 37.2 
#define MY_LONGITUDE 23.4

#define STATE_SUNRISE 1
#define STATE_SUNSET 2
#define STATE_DAYLIGHT 3
#define STATE_NIGHTLIGHT 4

#define SUNCHANGE_DURATION_SECONDS 1800
//1: day
//2: sunrise
//3: sunset
int state;
time_t systime;
RTC_DS1307 rtc;

unsigned long DAWN_SECONDS=-1;
unsigned long DUSK_SECONDS=-1;

LiquidCrystal lcd(12,11,5,4,3,2);

const char *monthName[12] = {
  "Jan", "Feb", "Mar", "Apr", "May", "Jun",
  "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
};
  
void setup()
{
  Serial.begin(57600);
  // Set current time
  bool parse=false;
  bool config=false;
  char buffer[30];
  
  //init pwm output controlling the light ballast
  pinMode(10, OUTPUT);   // sets the pin 10 as output
  pinMode(9, OUTPUT);   // sets the pin 10 as output  

  // The LiquidCrystal library can be used with many different
  // LCD sizes. We're using one that's 2 lines of 16 characters,
  // so we'll inform the library of that:
  lcd.begin(16, 2);
  lcd.clear();

  // Display welcom message on the LCD
  lcd.print("Birdy v3");
  lcd.setCursor(0,1); //Move to second line
  lcd.print("Welcome!");
  delay(1000);
  lcd.clear();

  while (! rtc.begin()) {
    lcd.print("RTC Not Found");
    lcd.setCursor(0,1); //Move to second line
    lcd.print("Check Battery!");
    delay(1000);
  }
  lcd.clear();

  //rtc.adjust(DateTime(2016, 1, 1, 00, 53, 25));
  
  // Get Time from RTC and initialize system time
  DateTime now = rtc.now();
  lcd.clear(); 

  // Initialize system time
  tmstruct tmptr;
  tmptr.tm_year = now.year()-1900;
  tmptr.tm_mon = now.month()-1;
  tmptr.tm_mday = now.day();
  tmptr.tm_hour = now.hour();
  tmptr.tm_min = now.minute();
  tmptr.tm_sec = now.second();
  tmptr.tm_isdst = 1;
  set_position( MY_LATITUDE * ONE_DEGREE, MY_LONGITUDE * ONE_DEGREE);
  set_zone(2 * ONE_HOUR);
  set_dst(eu_dst);
  systime = mk_gmtime( &tmptr );
  set_system_time(systime);
  
  //strftime(buffer,30,"%m-%d-%Y  %T",&tmptr);
  lcd.clear();
  printDate(&systime);
  delay(3000);

  // Print sunrise during initializaion
  time_t tseconds = sun_rise(&systime);
  strftime(buffer,30,"%T",localtime(&tseconds));
  lcd.setCursor(0,0);
  lcd.print("SUNRISE TODAY");
  lcd.setCursor(0,1);  
  lcd.print(buffer);
  delay(3000);

  // Print sunset during initializaion
  tseconds = sun_set(&systime);    
  strftime(buffer,30,"%T",localtime(&tseconds));
  lcd.clear();
  lcd.setCursor(0,0);
  lcd.print("SUNSET TODAY");
  lcd.setCursor(0,1);  
  lcd.print(buffer); 
  delay(4000);  
  lcd.clear();
}
 
void loop()
{
  time_t systime = setSystemTimeFromRTC();
  printDate(&systime);
  printTime(&systime);
  tmstruct* tm=gmtime(&systime);

  // cumulative seconds after start of today
  unsigned long currsec = (unsigned long) tm->tm_hour*3600 + tm->tm_min*60 + tm->tm_sec;
  char buffer[30];

  // calculate sunset and sunrise time once a day
  if (1){
    time_t t_sunrise, t_sunset,d;
    // sunrise in seconds of the day
    set_dst(eu_dst);
    t_sunrise = sun_rise(&systime);
    tmstruct* tmp=gmtime(&t_sunrise);
    
    strftime(buffer,30,"%T",localtime(&t_sunrise));
    Serial.print("SunRise Today=");
    Serial.println(buffer);

    unsigned long tmp_hour = (unsigned long)tmp->tm_hour*3600;
    unsigned long tmp_minute = (unsigned long)tmp->tm_min*60;
    unsigned long tmp_sec = (unsigned long)tmp->tm_sec;    
    DAWN_SECONDS = tmp_hour+tmp_minute+tmp_sec;
    Serial.print("DAWN_SECONDS=");
    Serial.println(DAWN_SECONDS);
    
    // sunset in seconds of the day
    t_sunset = sun_set(&systime);
    tmp=gmtime(&t_sunset);
    strftime(buffer,30,"%T",localtime(&t_sunset));
    Serial.print("SunSet Today=");
    Serial.println(buffer);
    tmp_hour = (unsigned long)tmp->tm_hour*3600;
    tmp_minute = (unsigned long)tmp->tm_min*60;
    tmp_sec = (unsigned long)tmp->tm_sec;
    DUSK_SECONDS = tmp_hour+tmp_minute+tmp_sec;
    Serial.print("DUSK_SECONDS=");
    Serial.println(DUSK_SECONDS);
    Serial.println(currsec);
  }

  if (currsec > DUSK_SECONDS || currsec < DAWN_SECONDS){
    state=0;
    // night time
    lcd.setCursor(10,1);
    lcd.print("    ");
    lcd.setCursor(13,1);
    lcd.print("  ");
    lcd.setCursor(15,1);   
    lcd.print("N");
  
    // output pwm with a different duty cycle
    analogWrite(10, 0);  
    digitalWrite(9,HIGH);
  }else if (currsec > DAWN_SECONDS+SUNCHANGE_DURATION_SECONDS && currsec < DUSK_SECONDS-SUNCHANGE_DURATION_SECONDS){
    // day time
    state=1;    
    lcd.setCursor(10,1);
    lcd.print("    ");
    lcd.setCursor(13,1);
    lcd.print("  ");
    lcd.setCursor(15,1);   
    lcd.print("D");
  
    // output pwm with a different duty cycle
    analogWrite(10, 255);  
    digitalWrite(9,LOW);
  }else if (currsec >= DAWN_SECONDS && currsec <= DAWN_SECONDS+SUNCHANGE_DURATION_SECONDS){
    state=2;
    float percentage = (float)(DAWN_SECONDS+SUNCHANGE_DURATION_SECONDS-currsec)/SUNCHANGE_DURATION_SECONDS;
    int level = round(255-(percentage * 255));
    int percent_int = round(100-100*percentage);    
    
    digitalWrite(9,LOW);
    analogWrite(10, level);
    lcd.setCursor(10,1);
    lcd.print("SR");
    lcd.setCursor(13,1);
    lcd.print(percent_int);
    if (percent_int<10){
      lcd.setCursor(14,1);
      lcd.print(" ");
    }
    lcd.setCursor(15,1);
    lcd.print("%");    
        
  }else if (currsec <= DUSK_SECONDS && currsec >= DUSK_SECONDS-SUNCHANGE_DURATION_SECONDS){
    state=3;
    // output pwm with a different duty cycle
    float percentage = (float)(currsec-DUSK_SECONDS+SUNCHANGE_DURATION_SECONDS)/SUNCHANGE_DURATION_SECONDS;
    int level = 255-round(percentage * 255);
    int percent_int = round(100-100*percentage);

    analogWrite(10, level);
    digitalWrite(9,LOW);
    lcd.setCursor(10,1);
    lcd.print("SS");
    lcd.setCursor(13,1);
    lcd.print(percent_int);
    if (percent_int<10){
      lcd.setCursor(14,1);
      lcd.print(" ");
    }
    lcd.setCursor(15,1);
    lcd.print("%");
  }
  delay(1000);
}

time_t setSystemTimeFromRTC(){
  DateTime now = rtc.now();
  tmstruct tmptr;
  tmptr.tm_year = now.year()-1900;
  tmptr.tm_mon = now.month()-1;
  tmptr.tm_mday = now.day();
  tmptr.tm_hour = now.hour();
  tmptr.tm_min = now.minute();
  tmptr.tm_sec = now.second();
  tmptr.tm_isdst = 1;
  set_position( MY_LATITUDE * ONE_DEGREE, MY_LONGITUDE * ONE_DEGREE);
  set_zone(2 * ONE_HOUR);
  set_dst(eu_dst);
  systime = mk_gmtime( &tmptr );
  set_system_time(systime);
  return systime;
}

void printDate(const time_t* timer){
  // get the time from the time library
  tmstruct* tm=gmtime(timer);
  lcd.setCursor(0,0);
  lcd.print(tm->tm_mday);
  int i=1;
  if (tm->tm_mday<10)
    lcd.setCursor(i,0);
  else{
    i++;
    lcd.setCursor(i,0);
  }
  lcd.print("-");
  i++;
  lcd.setCursor(i,0);
  int month=tm->tm_mon+1;
  lcd.print(month);
  i++;
  if (month<10)
    lcd.setCursor(i,0);
  else{
    i++;
    lcd.setCursor(i,0);
  }
  lcd.print("-");
  i++;
  lcd.setCursor(i,0);
  int year=tm->tm_year+1900;
  lcd.print(year);
}

void printTime(const time_t* timer){
  // get the time from the time library
  tmstruct* tm=gmtime(timer);
  lcd.setCursor(0,1);
  if (tm->tm_hour<10){
    lcd.print(0);
    lcd.setCursor(1,1);
  }
  lcd.print(tm->tm_hour);
  lcd.setCursor(2,1);
  lcd.print(":");
  lcd.setCursor(3,1); 
  if (tm->tm_min<10){
    lcd.print(0);
    lcd.setCursor(4,1);
  }
  lcd.print(tm->tm_min);  
  lcd.setCursor(5,1);
  lcd.print(":");
  lcd.setCursor(6,1);
  if (tm->tm_sec<10){
    lcd.print(0);
    lcd.setCursor(7,1);
  }
  lcd.print(tm->tm_sec);
}

void printLCD(int delayMsec, const char* msgL1, const char* msgL2){
  lcd.clear();
  lcd.setCursor(0,0);
  lcd.print(msgL1);
  lcd.setCursor(0,1);  
  lcd.print(msgL2); 
  delay(delayMsec);  
  lcd.clear();
}

