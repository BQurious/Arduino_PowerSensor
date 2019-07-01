#include <EEPROM.h>

/*
 * Read value at analog pin (multiplied/offset by some value as required),
 * calculate rolling average and display this using internal LED blinks and serial output
 * and save it to EEPROM to be retrieved later by another sketch
 * 
 * Blink pattern:
 * Short blink     => +1 to current digit
 * Medium blink    => +3 to current digit
 * Medium gap(off) => next digit
 * Long blink      => starting over
 */
const int PIN_ANALOG_IN = A0;
const int PIN_LED_OUT = LED_BUILTIN;

//in millisec
const int BLINK_SHORT_TIME = 150;
const int BLINK_MED_TIME   = 750;
const int BLINK_LONG_TIME  = 2000;
const int GAP_DEFAULT      = 350;
const int GAP_MEDIUM       = 1050;

/**
 * Offsetting is done first, then multiply
 */
const int ANALOG_OFFSET = -512;
/* 
 * value of (5000/1023) shows millivolts directly,
 * calibrated for current sensor with resolution of 0.1 amps
 */
const float ANALOG_MULTIPLIER = (5000.0/1023.0)/(18.5);

//to minimize fluctuation, take these many samples at a time
const int SAMPLES_PER_BATCH = 500;

//write to EEPROM when this many samples have been collected (and average has changed)
const int SAMPLES_IN_AVG_PER_STORAGE = 10;

/**
 * Output a value with heading to serial (eg.: "Illusion: 100")
 */
 void printTitledValue(char str[],float val)
{
  Serial.print(str);
  Serial.println(val);
}

/**
 * keep a pin HI then LOW for specified durations
 */
void blinkWithGap(int pin, int onTime, int offTime)
{
  digitalWrite(pin,HIGH);
  delay(onTime);
  digitalWrite(pin,LOW);
  delay(offTime);
}

/* 
 * helper funtion to use blink pattern as specified in main doc to display value
 * decimal part is truncated
 */
void blinkPattern(float value)
{
  //quick and dirty fix for value==0
  if(value == 0)
  {
    value = 0.1;
  }
  
  //quick and dirty fix to reverse digits
  float val = value;
  while (val >= 10.0) {
    val/=10;
  }

  int digit;
  
  do {
    digit = ((int)val)%10;
    if(digit == 0)
    {
      //blink to 10 if digit is 0 instead of skipping digit
      digit = 10;
    }

    //the actual blinking
    while (digit >= 3)
    {
      blinkWithGap(PIN_LED_OUT, BLINK_MED_TIME, GAP_DEFAULT);
      digit-=3;
    }
    while (digit > 0)
    {
      blinkWithGap(PIN_LED_OUT, BLINK_SHORT_TIME, GAP_DEFAULT);
      digit--;
    }

    //move to next digit
    delay(GAP_MEDIUM);
    val*=10;
  }while (val <= value);
  
  blinkWithGap(PIN_LED_OUT, BLINK_LONG_TIME, GAP_MEDIUM);
}

void blinkTest(float val) {
  Serial.println(val);
  delay(1000);
  blinkPattern(val);
}

void setup() {
  // initialize serial communication at 9600 bits per second:
  Serial.begin(9600);
  pinMode(PIN_LED_OUT, OUTPUT);
  
  printTitledValue("multiplier: ",ANALOG_MULTIPLIER);
  Serial.println("");

  /**
   * Tests!
   */
   bool TEST_MODE = false;
   if (TEST_MODE)
   {
      blinkTest(0.0);
      blinkTest(100.0);
      blinkTest(12345.0);
      blinkTest(67890.0);
   }
  
}

float batchValue;
float displayValue;
float rollingAvg;
float samplesInAvg = 0;
int samplesInBatch;

int writeAddress = 0;

void loop() {
  samplesInBatch = 0;
  batchValue = 0;
  while(samplesInBatch < SAMPLES_PER_BATCH)
  {
    // read the input on analog pin 0:
    displayValue = (analogRead(PIN_ANALOG_IN)+ANALOG_OFFSET);
    
    // take absolute value
    if(displayValue<0)
    {
      displayValue = -displayValue;
    }
    
    batchValue += displayValue;
    samplesInBatch++;
  }
  
  batchValue = (batchValue/SAMPLES_PER_BATCH)*ANALOG_MULTIPLIER;
  // print out the value you read:
  printTitledValue("current val: ",batchValue);

  // calculate rolling avg & print
  rollingAvg = (rollingAvg*samplesInAvg+batchValue)/(samplesInAvg+1);
  samplesInAvg++;
  
  printTitledValue("rolling avg: ",rollingAvg);
  Serial.println("");

  if(((int)samplesInAvg % SAMPLES_IN_AVG_PER_STORAGE) == 0)
  {
    if(rollingAvg != EEPROM.read(writeAddress))
    {
      Serial.println("writing to memory");
      writeAddress++;
      if(writeAddress == EEPROM.length())
      {
        writeAddress = 0;
      }
      EEPROM.write(writeAddress, (char)rollingAvg);
    }
    else
    {
      Serial.println("skipped memory write");
    }
  }
  
  //show blink pattern
  blinkPattern(rollingAvg);
}
