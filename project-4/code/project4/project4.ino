#include "project4.h"

#define NO_OF_TASKS 5

static void Task1( void *pvParameters );
static void Task2( void *pvParameters );
static void Task3( void *pvParameters );
static void Task4( void *pvParameters );
static void Task5( void *pvParameters );

static void (*Tasks[NO_OF_TASKS])(void *pvParameters) = {Task1, Task2, Task3, Task4, Task5};

static TaskProperties_t xTaskProperties[NO_OF_TASKS] = 	
{
   /* Handle,   Name,   P,    M,    D,    P */
	{   NULL,   "T1",   0,   92,   92,   92},
	{   NULL,   "T2",   0,   94,   94,   94},
	{   NULL,   "T3",   0,   96,   96,   96},
	{   NULL,   "T4",   0,   98,   98,   98},
	{   NULL,   "T5",   0,  100,  100,  100}
};

static ResourceHandle_t R1;
static ResourceHandle_t R2;

/* Runs the CPU for specified number of ticks*/
inline void RunCPU(TickType_t uxTicks)
{	
	TaskHandle_t xTaskHandle = xTaskGetCurrentTaskHandle();
	PRINTF("TASK : %-2s, PRIORITY : %d\n", pcTaskGetName(xTaskHandle), uxTaskPriorityGet(xTaskHandle));

	while(uxTicks--)
	{
		// Runs the CPU for 15 ms at FCLK = 16Mhz; 15 ms is WatchDog Timer Interrupt
		_delay_loop_2(60000);
	}	
}

static void Task1( void *pvParameters )
{ 
	(void *) pvParameters;	
	vTaskDelay(8);

	RunCPU(2);
	vSchedulerResourceWait(R1);
	PRINTF("%s : CRITICAL SECTION START\n", xTaskProperties[0].xTaskName);
	RunCPU(2);
	PRINTF("%s : CRITICAL SECTION END\n", xTaskProperties[0].xTaskName);
	vSchedulerResourceSignal(R1);
	RunCPU(2);

}

static void Task2( void *pvParameters )
{ 
	(void *) pvParameters;	
	vTaskDelay(9);

	RunCPU(2);
	vSchedulerResourceWait(R2);
	PRINTF("%s : CRITICAL SECTION START\n", xTaskProperties[1].xTaskName);
	RunCPU(2);
	PRINTF("%s : CRITICAL SECTION END\n", xTaskProperties[1].xTaskName);
	vSchedulerResourceSignal(R2);
	RunCPU(2);

}

static void Task3( void *pvParameters )
{ 
	(void *) pvParameters;	
	vTaskDelay(7);

	RunCPU(3);
}

static void Task4( void *pvParameters )
{ 
	(void *) pvParameters;	
	vTaskDelay(5);

	RunCPU(2);
	vSchedulerResourceWait(R1);
	PRINTF("%s : CRITICAL SECTION START\n", xTaskProperties[3].xTaskName);
	RunCPU(3);
	vSchedulerResourceWait(R2);
	PRINTF("%s : CRITICAL SECTION START\n", xTaskProperties[3].xTaskName);
	RunCPU(3);
	PRINTF("%s : CRITICAL SECTION END\n", xTaskProperties[3].xTaskName);
	vSchedulerResourceSignal(R2);
	RunCPU(3);
	PRINTF("%s : CRITICAL SECTION END\n", xTaskProperties[3].xTaskName);
	vSchedulerResourceSignal(R1);
	RunCPU(2);
}

static void Task5( void *pvParameters )
{ 
	(void *) pvParameters;

	RunCPU(4);
	vSchedulerResourceWait(R2);
	PRINTF("%s : CRITICAL SECTION START\n", xTaskProperties[4].xTaskName);
	RunCPU(4);
	PRINTF("%s : CRITICAL SECTION END\n", xTaskProperties[4].xTaskName);
	vSchedulerResourceSignal(R2);
	RunCPU(4);
}


void setup() 
{

}
void loop() 
{

}

int main( void )
{
  	Serial.begin(250000);
  	while (!Serial);

	Serial.print("----- Program Started -----\n");
	
	vSchedulerInit();

	vSchedulerPeriodicTaskCreate(Tasks[4], xTaskProperties[4].xTaskName, configMINIMAL_STACK_SIZE, NULL, tskIDLE_PRIORITY, 
										 &(xTaskProperties[4].xHandle), 
										   xTaskProperties[4].xPhaseTick, 
										   xTaskProperties[4].xPeriodTick, 
										   xTaskProperties[4].xMaxExecTick, 
										   xTaskProperties[4].xDeadlineTick
	); 

	vSchedulerPeriodicTaskCreate(Tasks[3], xTaskProperties[3].xTaskName, configMINIMAL_STACK_SIZE, NULL, tskIDLE_PRIORITY, 
										 &(xTaskProperties[3].xHandle), 
										   xTaskProperties[3].xPhaseTick, 
										   xTaskProperties[3].xPeriodTick, 
										   xTaskProperties[3].xMaxExecTick, 
										   xTaskProperties[3].xDeadlineTick
	); 

	vSchedulerPeriodicTaskCreate(Tasks[2], xTaskProperties[2].xTaskName, configMINIMAL_STACK_SIZE, NULL, tskIDLE_PRIORITY, 
										 &(xTaskProperties[2].xHandle), 
										   xTaskProperties[2].xPhaseTick, 
										   xTaskProperties[2].xPeriodTick, 
										   xTaskProperties[2].xMaxExecTick, 
										   xTaskProperties[2].xDeadlineTick
	); 

	vSchedulerPeriodicTaskCreate(Tasks[1], xTaskProperties[1].xTaskName, configMINIMAL_STACK_SIZE, NULL, tskIDLE_PRIORITY, 
										 &(xTaskProperties[1].xHandle), 
										   xTaskProperties[1].xPhaseTick, 
										   xTaskProperties[1].xPeriodTick, 
										   xTaskProperties[1].xMaxExecTick, 
										   xTaskProperties[1].xDeadlineTick
	); 

	vSchedulerPeriodicTaskCreate(Tasks[0], xTaskProperties[0].xTaskName, configMINIMAL_STACK_SIZE, NULL, tskIDLE_PRIORITY, 
										 &(xTaskProperties[0].xHandle), 
										   xTaskProperties[0].xPhaseTick, 
										   xTaskProperties[0].xPeriodTick, 
										   xTaskProperties[0].xMaxExecTick, 
										   xTaskProperties[0].xDeadlineTick
	); 

	/* Create Resource Handle */
	R1 = xSchedulerCreateResource("R1");
	
	/* Create Resource Handle */
	R2 = xSchedulerCreateResource("R2");

	/* Tell scheduler which tasks use the resource */
	vSchedulerResourceUsedByTask(R1, &(xTaskProperties[0].xHandle));
	vSchedulerResourceUsedByTask(R1, &(xTaskProperties[3].xHandle));

	/* Tell scheduler which tasks use the resource */
	vSchedulerResourceUsedByTask(R2, &(xTaskProperties[1].xHandle));
	vSchedulerResourceUsedByTask(R2, &(xTaskProperties[3].xHandle));
	vSchedulerResourceUsedByTask(R2, &(xTaskProperties[4].xHandle));

	vSchedulerStart();

	Serial.print("----- Program Ended -----\n");
	
	/* If all is well, the scheduler will now be running, and the following line
	will never be reached. */
	for( ;; );
}

