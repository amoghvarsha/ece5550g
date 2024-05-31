#include "project3.h"

#define TASK_SET 1

#if (TASK_SET == 1)
#define NO_OF_TASKS 4

/* Task Set 1 of Project-3 Part-3 a*/
static TaskProperties_t xTaskProperties[NO_OF_TASKS] = 	
{
	{NULL, "T1", pdMS_TO_TICKS(0), pdMS_TO_TICKS(100), pdMS_TO_TICKS( 400), pdMS_TO_TICKS( 400), {(pdMS_TO_TICKS(100)-1)}},
	{NULL, "T2", pdMS_TO_TICKS(0), pdMS_TO_TICKS(200), pdMS_TO_TICKS( 690), pdMS_TO_TICKS( 775), {(pdMS_TO_TICKS(200)-1)}},
	{NULL, "T3", pdMS_TO_TICKS(0), pdMS_TO_TICKS(150), pdMS_TO_TICKS(1018), pdMS_TO_TICKS(1018), {(pdMS_TO_TICKS(150)-1)}},
	{NULL, "T4", pdMS_TO_TICKS(0), pdMS_TO_TICKS(300), pdMS_TO_TICKS(4992), pdMS_TO_TICKS(4992), {(pdMS_TO_TICKS(300)-1)}}
};

#elif (TASK_SET == 2)	
#define NO_OF_TASKS 4

/* Task Set 2 of Project-3 Part-3 b*/
static TaskProperties_t xTaskProperties[NO_OF_TASKS] = 	
{
	{NULL, "T1", pdMS_TO_TICKS(0), pdMS_TO_TICKS(100), pdMS_TO_TICKS( 400), pdMS_TO_TICKS( 400), {(pdMS_TO_TICKS(100)-1)}},
	{NULL, "T2", pdMS_TO_TICKS(0), pdMS_TO_TICKS(150), pdMS_TO_TICKS( 200), pdMS_TO_TICKS( 495), {(pdMS_TO_TICKS(150)-1)}},
	{NULL, "T3", pdMS_TO_TICKS(0), pdMS_TO_TICKS(200), pdMS_TO_TICKS( 690), pdMS_TO_TICKS( 775), {(pdMS_TO_TICKS(200)-1)}},
	{NULL, "T4", pdMS_TO_TICKS(0), pdMS_TO_TICKS(150), pdMS_TO_TICKS(1018), pdMS_TO_TICKS(1018), {(pdMS_TO_TICKS(150)-1)}}
};

#else
#error "No TASK_SET Defined"
#endif

void setup() {}
void loop()  {}

/* Runs the CPU for specified number of ticks*/
inline void RunCPU(TickType_t uxTicks)
{	
	while(uxTicks--)
	{
		// Runs the CPU for 15 ms at FCLK = 16Mhz; 15 ms is WatchDog Timer Interrupt
		_delay_loop_2(60000);
	}	
}


static void Task( void *pvParameters )
{ 
	TaskProperties_t *xProperties = (TaskProperties_t *) pvParameters;	
	RunCPU(xProperties->xParameters.xRunTick);
}


int main( void )
{
  	Serial.begin(250000);
  	while (!Serial);

	Serial.print("----- Program Started -----\n");
	
	vSchedulerInit();
	
	for (int i = 0; i < NO_OF_TASKS; i++)
	{
		TaskProperties_t *xProperties = &(xTaskProperties[i]);

		vSchedulerPeriodicTaskCreate(Task, xProperties->xTaskName, configMINIMAL_STACK_SIZE, xProperties, tskIDLE_PRIORITY, 
									 &(xProperties->xHandle), 
									 xProperties->xPhaseTick, 
									 xProperties->xPeriodTick, 
									 xProperties->xMaxExecTick, 
									 xProperties->xDeadlineTick
		); 
	}

	vSchedulerStart();

	Serial.print("----- Program Ended -----\n");
	
	/* If all is well, the scheduler will now be running, and the following line
	will never be reached. */
	for( ;; );
}

