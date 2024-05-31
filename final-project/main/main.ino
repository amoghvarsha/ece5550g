#include "project3.h"

#define TASK_SET 	2
#define NO_OF_TASKS 4

static void Task1( void *pvParameters );
static void Task2( void *pvParameters );
static void Task3( void *pvParameters );
static void Task4( void *pvParameters );

static void (*Tasks[NO_OF_TASKS])(void *pvParameters) = {Task1, Task2, Task3, Task4};

#if (TASK_SET == 1)

	/* Task Set 1 of Project-3 Part-3 a*/
	static TaskProperties_t xTaskProperties[] = 	
	{
		{NULL, "T1", pdMS_TO_TICKS(0), pdMS_TO_TICKS(100), pdMS_TO_TICKS( 400), pdMS_TO_TICKS( 400), {(pdMS_TO_TICKS(100)-1)}},
		{NULL, "T2", pdMS_TO_TICKS(0), pdMS_TO_TICKS(200), pdMS_TO_TICKS( 690), pdMS_TO_TICKS( 775), {(pdMS_TO_TICKS(200)-1)}},
		{NULL, "T3", pdMS_TO_TICKS(0), pdMS_TO_TICKS(150), pdMS_TO_TICKS(1018), pdMS_TO_TICKS(1018), {(pdMS_TO_TICKS(150)-1)}},
		{NULL, "T4", pdMS_TO_TICKS(0), pdMS_TO_TICKS(300), pdMS_TO_TICKS(4992), pdMS_TO_TICKS(4992), {(pdMS_TO_TICKS(300)-1)}}
	};

#elif (TASK_SET == 2)	

	/* Task Set 2 of Project-3 Part-3 b*/
	static TaskProperties_t xTaskProperties[] = 	
	{
		{NULL, "T1", pdMS_TO_TICKS(0), pdMS_TO_TICKS(100), pdMS_TO_TICKS( 400), pdMS_TO_TICKS( 400), {(pdMS_TO_TICKS(100)-1)}},
		{NULL, "T2", pdMS_TO_TICKS(0), pdMS_TO_TICKS(150), pdMS_TO_TICKS( 200), pdMS_TO_TICKS( 495), {(pdMS_TO_TICKS(150)-1)}},
		{NULL, "T3", pdMS_TO_TICKS(0), pdMS_TO_TICKS(200), pdMS_TO_TICKS( 690), pdMS_TO_TICKS( 775), {(pdMS_TO_TICKS(200)-1)}},
		{NULL, "T4", pdMS_TO_TICKS(0), pdMS_TO_TICKS(150), pdMS_TO_TICKS(1018), pdMS_TO_TICKS(1018), {(pdMS_TO_TICKS(150)-1)}}
	};

#else
	#error "Error : No TASK_SET Defined"
#endif

void setup() {}
void loop()  {}


inline void RunCPU(TickType_t uxTicks)
{	
	/* Runs the CPU for specified number of ticks*/
	while(uxTicks--)
	{
		// Runs the CPU for 15 ms at FCLK = 16Mhz; 15 ms is WatchDog Timer Interrupt
		_delay_loop_2(60000);
	}	
}


static void Task1( void *pvParameters )
{ 
	(void) pvParameters;
	RunCPU(xTaskProperties[0].xParameters.xRunTick);
}

static void Task2( void *pvParameters )
{ 
	(void) pvParameters;
	RunCPU(xTaskProperties[1].xParameters.xRunTick);
}

static void Task3( void *pvParameters )
{ 
	(void) pvParameters;
	RunCPU(xTaskProperties[2].xParameters.xRunTick);
}

static void Task4( void *pvParameters )
{ 
	(void) pvParameters;
	RunCPU(xTaskProperties[3].xParameters.xRunTick);
}


int main( void )
{
  	Serial.begin(250000);
  	while (!Serial);

	Serial.print("----- Program Started -----\n");
	
	vSchedulerInit();
	

	vSchedulerPeriodicTaskCreate(Tasks[0], xTaskProperties[0].xTaskName, configMINIMAL_STACK_SIZE, NULL, tskIDLE_PRIORITY, &(xTaskProperties[0].xHandle), 
										xTaskProperties[0].xPhaseTick, 
										xTaskProperties[0].xPeriodTick, 
										xTaskProperties[0].xMaxExecTick, 
										xTaskProperties[0].xDeadlineTick
	); 

	vSchedulerPeriodicTaskCreate(Tasks[1], xTaskProperties[1].xTaskName, configMINIMAL_STACK_SIZE, NULL, tskIDLE_PRIORITY, &(xTaskProperties[1].xHandle), 
										xTaskProperties[1].xPhaseTick, 
										xTaskProperties[1].xPeriodTick, 
										xTaskProperties[1].xMaxExecTick, 
										xTaskProperties[1].xDeadlineTick
	); 

	vSchedulerPeriodicTaskCreate(Tasks[2], xTaskProperties[2].xTaskName, configMINIMAL_STACK_SIZE, NULL, tskIDLE_PRIORITY, &(xTaskProperties[2].xHandle), 
										xTaskProperties[2].xPhaseTick, 
										xTaskProperties[2].xPeriodTick, 
										xTaskProperties[2].xMaxExecTick, 
										xTaskProperties[2].xDeadlineTick
	); 

	vSchedulerPeriodicTaskCreate(Tasks[3], xTaskProperties[3].xTaskName, configMINIMAL_STACK_SIZE, NULL, tskIDLE_PRIORITY, &(xTaskProperties[3].xHandle), 
										xTaskProperties[3].xPhaseTick, 
										xTaskProperties[3].xPeriodTick, 
										xTaskProperties[3].xMaxExecTick, 
										xTaskProperties[3].xDeadlineTick
	); 
	
	vSchedulerStart();

	Serial.print("----- Program Ended -----\n");
	
	/* If all is well, the scheduler will now be running, and the following line
	will never be reached. */
	for( ;; );
}

