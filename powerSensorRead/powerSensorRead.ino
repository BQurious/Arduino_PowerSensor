#include <EEPROM.h>
#include <math.h>

//set to true to run the blink-pattern with test values first
#define TEST_BLINK false

//set to true to log ADC sample time
#define PROFILE_ADC false

//true to save to EEPROM periodically
#define SAVE_EEPROM false
#if SAVE_EEPROM
/* write to EEPROM when these many samples have been collected (and RMS
 * value has changed since last write)
 */
const int BATCHES_PER_STORAGE = 10;
#endif
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

/* Offsetting is done first, then multiply
 */
const int ANALOG_OFFSET = -512;
/* value of (5000/1023) shows millivolts directly,
 * calibrated for current sensor with resolution of 0.1 amps
 */
const float ANALOG_MULTIPLIER = (5000.0/1023.0)/(18.5);

//to minimize fluctuation, take these many samples at a time
const int SAMPLES_PER_BATCH = 3076;
/* value selected to have only full waves in the batch at both 50/60 Hz
 * since partial waves will skew the result
 * (~20/24 waves sampled respectively)
 */

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
	#if (TEST_BLINK)
		blinkTest(0.0);
		blinkTest(100.0);
		blinkTest(12345.0);
		blinkTest(67890.0);
	#endif
}

float batchValue;
float currentSample;
float rollingAvgOfSquares;
float rollingRMS;
float totalBatches = 0;
int samplesInBatch;
int writeAddress = 0;

#if (PROFILE_ADC)
	unsigned int time;
	#define profileStart() time = millis()
	#define profileEnd() time = millis() - time
	#define logProfile() printTitledValue("sample time: ", time)
#else
	#define profileStart()
	#define profileEnd()
	#define logProfile()
#endif

void loop()
{
	batchValue = 0;
	profileStart();
	for (samplesInBatch = 0; samplesInBatch < SAMPLES_PER_BATCH; samplesInBatch++)
	{
		// read the input on analog pin 0:
		currentSample = (analogRead(PIN_ANALOG_IN)+ANALOG_OFFSET);
		//take square for rms
		batchValue += currentSample*currentSample;
	}
	profileEnd();
	batchValue = batchValue / samplesInBatch;
	// calculate rolling RMS & print
	rollingAvgOfSquares = ((rollingAvgOfSquares * totalBatches) + batchValue) / (totalBatches + 1);
	totalBatches++;
	rollingRMS = sqrt(rollingAvgOfSquares) * ANALOG_MULTIPLIER;

	//Only calculate for serial output if required (i.e, if any message is received, it will start sending)
	if (Serial.available())
	{
		// print out the value you read:
		printTitledValue("current val: ", sqrt(batchValue) * ANALOG_MULTIPLIER);
		logProfile();
		printTitledValue("rolling RMS: ", rollingRMS);
		Serial.println("");
	}
	
	#if (SAVE_EEPROM)
	if(((int)totalBatches % BATCHES_PER_STORAGE) == 0)
	{
		if (rollingRMS != EEPROM.read(writeAddress))
		{
			Serial.println("writing to memory");
			writeAddress++;
			if(writeAddress == EEPROM.length())
			{
				writeAddress = 0;
			}
			//only one byte (max value of 256) is written to memory
			EEPROM.write(writeAddress, (char)rollingRMS);
		}
		else
		{
			Serial.println("skipped memory write");
		}
	}
	#endif
	//show blink pattern
	blinkPattern(rollingRMS);
}
