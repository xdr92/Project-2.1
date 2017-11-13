#include "AVR_TTC_scheduler.h"
#include <avr/io.h>
#include <avr/interrupt.h>
#define F_CPU 16E6
#include <util/delay.h>
#include "distance.h"
#include <stdint.h>
#include <stdlib.h>
#include <avr/sfr_defs.h>
// output on USB = PD1 = board pin 1
// datasheet p.190; F_OSC = 16 MHz & baud rate = 19.200
#define UBBRVAL 51
#define BAUDRATE 19200
#define BAUD_PRESCALLER (((F_CPU / (BAUDRATE * 16UL))) - 1)
#define F_CPU 16000000UL

const int OMLAAG = 0;
const int UITROLLEN = 1;
const int OPROLLEN = 2;
const int OMHOOG = 3;

int arrayTempMinute[60];
int arrayTemp[60];
int arrayLight[2] = {0, 0};
int distanceShutter;
int open;
int closed;
int hourTemp[60];
int hourLight[60] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
int statusShutter = 0 /*OMHOOG*/; //Dicht is rolluik omlaag, open is rolluik omhoog
int light = 0;
int input = 0; // 0 = auto, 1 = omhoog, 2 = omlaag

int previousTemp1 = 0;
int previousTemp2 = 0;

int closeOnTemp = 30; //SET Default
int closeOnLight = 15; //SET Default

unsigned char taskId1;
unsigned char taskId2;
int *taskPtr1;
int *taskPtr2;
uint16_t i = 0;

volatile uint16_t gv_counter; // 16 bit counter value
volatile uint8_t gv_echo; // a flag

// The array of tasks
sTask SCH_tasks_G[SCH_MAX_TASKS];

//+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//********** start display ***********

void reset_display()
{
    // clear memory - all 16 addresses
    sendCommand(0x40); // set auto increment mode
    write(strobe, LOW);
    shiftOut(0xc0);   // set starting address to 0
    for(uint8_t i = 0; i < 16; i++)
    {
        shiftOut(0x00);
    }
    write(strobe, HIGH);
    sendCommand(0x89);  // activate and set brightness to medium
}

void showDigit(uint16_t cm)
{
                        /*0*/ /*1*/ /*2*/ /*3*/ /*4*/ /*5*/ /*6*/ /*7*/ /*8*/ /*9*/
    uint8_t digits[] = {0x3f, 0x06, 0x5b, 0x4f, 0x66, 0x6d, 0x7d, 0x07, 0x7f, 0x6f};
    uint8_t ar[8] = {0};
    uint8_t digit = 0, i = 0;
    uint8_t temp, nrs, spaces;
    
    // cm=1234 -> ar[0..7] = {4,3,2,1,0,0,0,0}
    while (cm > 0 && i < 8) {
        digit = cm % 10;
        ar[i] = digit;
        cm = cm / 10;
        i++;
    }

    nrs = i;      // 4 digits
    spaces = 8-i; // 4 leading spaces  
    
    // invert array -> ar[0..7] = {0,0,0,0,1,2,3,4}
    uint8_t n = 7;
    for (i=0; i<4; i++) {
        temp = ar[i];
        ar[i] = ar[n];
        ar[n] = temp;
        n--;
    }
    
    write(strobe, LOW);
    shiftOut(0xc0); // set starting address = 0
    // leading spaces
    for (i=0; i<8; i++) {
        if (i < spaces) {
            shiftOut(0x00);
        } else {
            shiftOut(digits[ar[i]]);
        }           
        shiftOut(0x00); // the dot
    }
    
    write(strobe, HIGH);
}

void sendCommand(uint8_t value)
{
    write(strobe, LOW);
    shiftOut(value);
    write(strobe, HIGH);
}

// write value to pin
void write(uint8_t pin, uint8_t val)
{
    if (val == LOW) {
        PORTB &= ~(_BV(pin)); // clear bit
    } else {
        PORTB |= _BV(pin); // set bit
    }
}

// shift out value to data
void shiftOut (uint8_t val)
{
	uint8_t i;
	for (i = 0; i < 8; i++)  {
		write(clock, LOW);   // bit valid on rising edge
		write(data, val & 1 ? HIGH : LOW); // lsb first
		val = val >> 1;
		write(clock, HIGH);
	}
}

//********** end display ***********
















void init_ports(void)
{
	DDRD = 0b00000100;
	DDRB = 0xff;
	//photocell intensiteit meten
	ADMUX = 0b00000000; // set ADC0
	ADCSRA = 0b10000110; //set ADEN, precale by 64
	
	sendCommand(0x89);
}




void USART_init(void){
	
	UBRR0H = (uint8_t)(BAUD_PRESCALLER>>8);
	UBRR0L = (uint8_t)(BAUD_PRESCALLER);
	UCSR0A = 0;
	UCSR0B = (1<<RXEN0)|(1<<TXEN0);
	//UCSR0B = _BV(TXEN0)|_BV(RXEN0);
	UCSR0C = (3<<UCSZ00) | _BV(UCSZ01);
	//UCSR0C = _BV(3<<UCSZ00);
	//UCSR0C = _BV(UCSZ01) | _BV(UCSZ00);
}


/*
void USART_init()
{
	// set the baud rate
	UBRR0H = 0;
	UBRR0L = UBBRVAL;
	// disable U2X mode
	UCSR0A = 0;
	// enable transmitter
	UCSR0B = _BV(TXEN0) | _BV(RXEN0);
	// set frame format : asynchronous, 8 data bits, 1 stop bit, no parity
	UCSR0C = _BV(UCSZ01) | _BV(UCSZ00);
}*/


void init_timer(void)
// prescaling : max time = 2^16/16E6 = 4.1 ms, 4.1 >> 2.3, so no prescaling required
// normal mode, no prescale, stop timer
{
    TCCR0B = 0;
}

void start_timer(void)
{
	TCNT0 = 0;
	TIFR0 = (1<<TOV0);// = 0
	TCCR0B = _BV(CS11);
}

void stop_timer(void)
{
	TCCR0B = 0;
}

void init_ext_int(void)
{
    // any change triggers ext interrupt 1
    EICRA = (1 << ISC10);
    EIMSK = (1 << INT1);
}

/*
uint16_t calc_cm(uint16_t counter)
{
    return 0;
}*/




// Read analog value from Arduino and convert it to 0-1023
unsigned int readAnalogValue(uint16_t pin) {
	ADCSRA = (1<<ADEN) | (1<<ADPS2) | (1<<ADPS0);
	ADMUX=pin; // Input pin
	ADCSRA |= (1<<ADSC); // Start conversion
	while (ADCSRA&(1<<ADSC)); //wait conversion end
	{
		return ADCW;			
	}

}

// Show Distance on screen
/*void showDistance() {
	uint16_t distance = getDistance();
	showDigit(distance);
}
*/


// Eiken. Waardes kloppen nu niet



//+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++


/*------------------------------------------------------------------*-

  SCH_Dispatch_Tasks()

  This is the 'dispatcher' function.  When a task (function)
  is due to run, SCH_Dispatch_Tasks() will run it.
  This function must be called (repeatedly) from the main loop.

-*------------------------------------------------------------------*/

void SCH_Dispatch_Tasks(void)
{
   unsigned char Index;

   // Dispatches (runs) the next task (if one is ready)
   for(Index = 0; Index < SCH_MAX_TASKS; Index++)
   {
      if((SCH_tasks_G[Index].RunMe > 0) && (SCH_tasks_G[Index].pTask != 0))
      {
         (*SCH_tasks_G[Index].pTask)();  // Run the task
         SCH_tasks_G[Index].RunMe -= 1;   // Reset / reduce RunMe flag

         // Periodic tasks will automatically run again
         // - if this is a 'one shot' task, remove it from the array
         if(SCH_tasks_G[Index].Period == 0)
         {
            SCH_Delete_Task(Index);
         }
      }
   }
}

/*------------------------------------------------------------------*-

  SCH_Add_Task()

  Causes a task (function) to be executed at regular intervals 
  or after a user-defined delay

  pFunction - The name of the function which is to be scheduled.
              NOTE: All scheduled functions must be 'void, void' -
              that is, they must take no parameters, and have 
              a void return type. 
                   
  DELAY     - The interval (TICKS) before the task is first executed

  PERIOD    - If 'PERIOD' is 0, the function is only called once,
              at the time determined by 'DELAY'.  If PERIOD is non-zero,
              then the function is called repeatedly at an interval
              determined by the value of PERIOD (see below for examples
              which should help clarify this).


  RETURN VALUE:  

  Returns the position in the task array at which the task has been 
  added.  If the return value is SCH_MAX_TASKS then the task could 
  not be added to the array (there was insufficient space).  If the
  return value is < SCH_MAX_TASKS, then the task was added 
  successfully.  

  Note: this return value may be required, if a task is
  to be subsequently deleted - see SCH_Delete_Task().

  EXAMPLES:

  Task_ID = SCH_Add_Task(Do_X,1000,0);
  Causes the function Do_X() to be executed once after 1000 sch ticks.            

  Task_ID = SCH_Add_Task(Do_X,0,1000);
  Causes the function Do_X() to be executed regularly, every 1000 sch ticks.            

  Task_ID = SCH_Add_Task(Do_X,300,1000);
  Causes the function Do_X() to be executed regularly, every 1000 ticks.
  Task will be first executed at T = 300 ticks, then 1300, 2300, etc.            
 
-*------------------------------------------------------------------*/

unsigned char SCH_Add_Task(void (*pFunction)(), const unsigned int DELAY, const unsigned int PERIOD)
{
   unsigned char Index = 0;

   // First find a gap in the array (if there is one)
   while((SCH_tasks_G[Index].pTask != 0) && (Index < SCH_MAX_TASKS))
   {
      Index++;
   }

   // Have we reached the end of the list?   
   if(Index == SCH_MAX_TASKS)
   {
      // Task list is full, return an error code
      return SCH_MAX_TASKS;  
   }

   // If we're here, there is a space in the task array
   SCH_tasks_G[Index].pTask = pFunction;
   SCH_tasks_G[Index].Delay =DELAY;
   SCH_tasks_G[Index].Period = PERIOD;
   SCH_tasks_G[Index].RunMe = 0;

   // return position of task (to allow later deletion)
   return Index;
}

/*------------------------------------------------------------------*-

  SCH_Delete_Task()

  Removes a task from the scheduler.  Note that this does
  *not* delete the associated function from memory: 
  it simply means that it is no longer called by the scheduler. 
 
  TASK_INDEX - The task index.  Provided by SCH_Add_Task(). 

  RETURN VALUE:  RETURN_ERROR or RETURN_NORMAL

-*------------------------------------------------------------------*/

unsigned char SCH_Delete_Task(const unsigned char TASK_INDEX)
{
   // Return_code can be used for error reporting, NOT USED HERE THOUGH!
   unsigned char Return_code = 0;

   SCH_tasks_G[TASK_INDEX].pTask = 0;
   SCH_tasks_G[TASK_INDEX].Delay = 0;
   SCH_tasks_G[TASK_INDEX].Period = 0;
   SCH_tasks_G[TASK_INDEX].RunMe = 0;

   return Return_code;
}

/*------------------------------------------------------------------*-

  SCH_Init_T1()

  Scheduler initialisation function.  Prepares scheduler
  data structures and sets up timer interrupts at required rate.
  You must call this function before using the scheduler.  

-*------------------------------------------------------------------*/

void SCH_Init_T1(void)
{
	unsigned char i;

	for(i = 0; i < SCH_MAX_TASKS; i++)
	{
		SCH_Delete_Task(i);
	}

	// Set up Timer 1
	// Values for 1ms and 10ms ticks are provided for various crystals

	// Hier moet de timer periode worden aangepast ....!
	OCR1A = (uint16_t)625;   		     // 10ms = (256/16.000.000) * 625
	TCCR1B = (1 << CS12) | (1 << WGM12);  // prescale op 64, top counter = value OCR1A (CTC mode)
	TIMSK1 = 1 << OCIE1A;   		     // Timer 1 Output Compare A Match Interrupt Enable
}

/*------------------------------------------------------------------*-

  SCH_Start()

  Starts the scheduler, by enabling interrupts.

  NOTE: Usually called after all regular tasks are added,
  to keep the tasks synchronised.

  NOTE: ONLY THE SCHEDULER INTERRUPT SHOULD BE ENABLED!!! 
 
-*------------------------------------------------------------------*/

void SCH_Start(void)
{
      sei();
}

/*------------------------------------------------------------------*-

  SCH_Update

  This is the scheduler ISR.  It is called at a rate 
  determined by the timer settings in SCH_Init_T1().

-*------------------------------------------------------------------*/

ISR(TIMER1_COMPA_vect)
{
   unsigned char Index;
   for(Index = 0; Index < SCH_MAX_TASKS; Index++)
   {
      // Check if there is a task at this location
      if(SCH_tasks_G[Index].pTask)
      {
         if(SCH_tasks_G[Index].Delay == 0)
         {
            // The task is due to run, Inc. the 'RunMe' flag
            SCH_tasks_G[Index].RunMe += 1;

            if(SCH_tasks_G[Index].Period)
            {
               // Schedule periodic tasks to run again
               SCH_tasks_G[Index].Delay = SCH_tasks_G[Index].Period;
               SCH_tasks_G[Index].Delay -= 1;
            }
         }
         else
         {
            // Not yet ready to run: just decrement the delay
            SCH_tasks_G[Index].Delay -= 1;
         }
      }
   }
}

// ------------------------------------------------------------------
//////////////////////////////////////////////////////////////////////////
// Distance
//////////////////////////////////////////////////////////////////////////
uint8_t getDistance() {
	uint16_t distance = 0;
	uint16_t counter = 0;
	
	
	PORTD |= _BV(PORTB2);
	//PORTD = 0x01;
	_delay_us(10);
	//PORTD = 0x00;
	PORTD &= ~_BV(PORTD2);
	while(!(PIND & (1<<3))) {} ;
	start_timer();

	while((PIND & (1<<3))) {
		if(TIFR0 & (1 << TOV0)) {
			TIFR0 = (1<<TOV0);// = 0
			counter++;
		}
	}
	stop_timer();
	distance = ((counter * 256 + (uint16_t)TCNT0)*0.002125)/2*8; //vermenigvuldigen met 0.0010625*8 om door te rekenen naar centimeters
	//showDigit(distance);
	if(distance <= 20) {
		distance = 20;
	} else if(distance >= 160) {
		distance = 160;
	} 
	
	return (uint8_t)distance;
}

void showDistance() {
	showDigit((uint16_t)getDistance());
}

//////////////////////////////////////////////////////////////////////////
// Temperature
//////////////////////////////////////////////////////////////////////////
int getTemperature() {
	_delay_ms(10);
	unsigned int resistanceTemperatureCell = readAnalogValue(0x05);
	float millivolts = (resistanceTemperatureCell/1023.0) * 5000;
	float temperature = (millivolts-500)/10;//((-50 + (680 * (1/((4995/millivolts)-1)))) / 1.5);
	_delay_ms(10);
	
	return temperature;
	
}

int getAccurateTemperature() {
	int temperatureAccurate[10];
	int tmp = 0;
	int counter = 0;
	int temptotal = 0;
	
	for(int i = 0; i < 10; i++) {
		temperatureAccurate[i] = getTemperature();
	}	
	for(int j = 0; j < 9; j++) {
		for(int k = j +1; k < 10; k++) {
			if(temperatureAccurate[j] + 3 > temperatureAccurate[k] && temperatureAccurate[j] - 3 < temperatureAccurate[k]) {
				counter++;
				temptotal += temperatureAccurate[j];
			}
		}
	}
	
	return temptotal / counter;	
	
}

void storeTemp(){
	static int t = 0;
	if(t >= 60) t = 0;
	arrayTemp[t] = getAccurateTemperature();
	t++;
}

void storeTempHour(){
	static int minute = 0;
	double totalTempMinute = 0;
	
	for(int i = 0; i < 60; i++){
		totalTempMinute += arrayTemp[i];
	}
	arrayTempMinute[minute] = totalTempMinute / 60;
	minute++;
}

int getAverageTemp() {
	int tempTotal = 0;
	int i = 0;
	while(arrayTemp[i] != 0) {
		tempTotal += arrayTemp[i];
		i++;
	}
	if(arrayTemp[0] == 0){
		return 20;
	}
	return tempTotal / (i);
}

int getHourTemp(){
	int tempTotal = 0;
	int i = 0;
	while(arrayTempMinute[i] != 0) {
		tempTotal += arrayTempMinute[i];
		i++;
	}
	if(arrayTempMinute[0] == 0){
		return getAverageTemp();
	} else {
		return tempTotal / (i);
	}
}	

void showTemp() {
	showDigit(getTemperature());
}

//////////////////////////////////////////////////////////////////////////
// Light
//////////////////////////////////////////////////////////////////////////
int getLight() {
	_delay_ms(50);
	unsigned int lightValue = readAnalogValue(0x00);
	float millivolts= (lightValue/1024.0) * 5000;
	float resistancePhotoCell = (680 * (1/((4995/millivolts)-1))) / 30;
	_delay_ms(50);
	return resistancePhotoCell;
}

void storeLight() {
	static int l = 0;
	
	if(l >= 2) l = 0;
	
	arrayLight[l] = getLight();
	l++;
}

 


int getHourLight(){
	double hourLightTotal = 0;
	int i = 0;
	while(hourLight[i] != 0) {
		hourLightTotal += hourLight[i];
		i++;
	}
	return hourLightTotal / (i);
}

int getAverageLight() {
	if(arrayLight[1] == 0) {
		return arrayLight[0];
	} else {
		return (arrayLight[0] + arrayLight[1]) / 2;
	}
}

void storeHourLight() {
	static  int hl = 0;
	if(hl >= 60) hl = 0;
	hourLight[hl] = getAverageLight();
	hl++;
}

//////////////////////////////////////////////////////////////////////////
// Percentage open/closed
//////////////////////////////////////////////////////////////////////////

int getClosedPercentage() {
	return (uint8_t)((float)(getDistance()-20)/140*100);
}

int getOpenPercentage() {
	return 100 - getClosedPercentage();
}

//////////////////////////////////////////////////////////////////////////
// switchLight
//////////////////////////////////////////////////////////////////////////

void switchLight() {
	if(light == 1) {
		PORTB &= ~_BV(PORTB4);
		light = 0;
	} else {
		PORTB |= _BV(PORTB4);
		light = 1;
	}
}

//////////////////////////////////////////////////////////////////////////
// changeStatus
//////////////////////////////////////////////////////////////////////////
void changeStatus(){
	static unsigned int checkUitrollen = 0;
	static unsigned int checkOprollen = 0;
	static unsigned int statusTmp = 0;
	int distanceShutterNow = getDistance();
	//showDigit(statusShutter);
	
	if(statusTmp == 1 && statusShutter == 2) {
		SCH_Delete_Task(*taskPtr1);
		SCH_Start();
	} else if (statusTmp == 2 && statusShutter == 1){
		SCH_Delete_Task(*taskPtr2);
		SCH_Start();
	}
	statusTmp = statusShutter;
	//////////////////////////////////////////////////////////////////////////
	if(statusShutter == OMLAAG) {
		PORTB |= _BV(PORTB3);
		PORTB &= ~_BV(PORTB4);
		PORTB &= ~_BV(PORTB5);
	//////////////////////////////////////////////////////////////////////////
	} else if(statusShutter == UITROLLEN) {
		//showDigit(12345);
		//_delay_ms(5000);
		PORTB &= ~_BV(PORTB3);
		PORTB &= ~_BV(PORTB5);
		if(!checkUitrollen){
			taskId1 = SCH_Add_Task(switchLight, 0, 50);
			SCH_Start();
			taskPtr1 = &taskId1;
			checkUitrollen = 1;
		}
		if(distanceShutterNow >= 125) {
			SCH_Delete_Task(*taskPtr1);
			SCH_Start();
			statusShutter = OMLAAG;
			checkUitrollen = 0;
		}
	//////////////////////////////////////////////////////////////////////////
	} else if(statusShutter == OPROLLEN) {
		PORTB &= ~_BV(PORTB3);
		PORTB &= ~_BV(PORTB5);

		if(!checkOprollen){
			taskId2 = SCH_Add_Task(switchLight, 0, 50);
			SCH_Start();
			taskPtr2 = &taskId2;
			checkOprollen = 1;
		}					
		if(distanceShutterNow <= 21) {
			SCH_Delete_Task(*taskPtr2);
			SCH_Start();
			statusShutter = OMHOOG;
			checkOprollen = 0;
		}
	//////////////////////////////////////////////////////////////////////////
	} else if(statusShutter == OMHOOG) {
		PORTB &= ~_BV(PORTB3);
		PORTB &= ~_BV(PORTB4);
		PORTB |= _BV(PORTB5);
	}
	
}
		
		

//////////////////////////////////////////////////////////////////////////
// editStatus EDIT
//////////////////////////////////////////////////////////////////////////
void editStatus() {
	int temperature = getAverageTemp();
	int lightInput = getLight();
	
	if(input != 0){// 0 = auto, 1 = omhoog, 2 = omlaag
		if(input == 1 && statusShutter != OMHOOG) {
			statusShutter = OPROLLEN;
		} else if(input == 2 && statusShutter != OMLAAG){
			statusShutter = UITROLLEN;
		}
		
	} else if(lightInput >= closeOnLight || temperature >= closeOnTemp) {
		if(statusShutter != 0 /*OMLAAG*/) {
			statusShutter = UITROLLEN;
		}
	} else if(lightInput < closeOnLight || temperature < closeOnTemp) {
		if(statusShutter != 3 /*OMHOOG*/) {
			statusShutter = OPROLLEN;
		}
	}
}

//////////////////////////////////////////////////////////////////////////
// Transmit
//////////////////////////////////////////////////////////////////////////
void transmit(uint8_t data)
{
	// wait for an empty transmit buffer
	// UDRE is set when the transmit buffer is empty
	loop_until_bit_is_set(UCSR0A, UDRE0);
	// send the data
	UDR0 = data;
}

void sendData() {

	int open = getOpenPercentage();
	transmit(getAccurateTemperature());
	transmit(getAverageLight());
	transmit(statusShutter);
	transmit(open);
	transmit(100 - open);
	transmit(getHourTemp());
	transmit(getHourLight());
}

void startDashboard(){
	transmit(127);
}

//////////////////////////////////////////////////////////////////////////
// Recieve data
//////////////////////////////////////////////////////////////////////////
unsigned char USART_receive(void){
	while (!(UCSR0A & (1<<RXC0)));
	return UDR0;
}

void receiveData() {
	/*int test = USART_receive();//*/while(!(USART_receive() == 127)){};
	closeOnTemp = USART_receive();
	closeOnLight = USART_receive();
	input = USART_receive();
}
//////////////////////////////////////////////////////////////////////////
// Setup
//////////////////////////////////////////////////////////////////////////
void setup() {
	init_ports();
	USART_init();
	init_timer();
	SCH_Init_T1();
	startDashboard();
	unsigned char TASK_temp			= SCH_Add_Task(storeTemp,0, 10);
	unsigned char TASK_tempHour		= SCH_Add_Task(storeTempHour, 0, 600);
	
	unsigned char TASK_light		= SCH_Add_Task(storeLight, 0, 300);
	unsigned char TASK_lightHour	= SCH_Add_Task(storeHourLight, 0, 600);

	unsigned char TASK_edit			= SCH_Add_Task(editStatus, 0, 100);
	unsigned char TASK_status		= SCH_Add_Task(changeStatus, 0, 100);
	unsigned char TASK_sData		= SCH_Add_Task(sendData, 0, 120);
	
	unsigned char TASK_ShowDistance	= SCH_Add_Task(showDistance, 0, 100);
	unsigned char TASK_rData		= SCH_Add_Task(receiveData, 90, 500);
}

//////////////////////////////////////////////////////////////////////////
// Main
//////////////////////////////////////////////////////////////////////////
int main()
{
	setup();
	SCH_Start();
	
	while(1) {
		SCH_Dispatch_Tasks();	
	}
	return 0;
}