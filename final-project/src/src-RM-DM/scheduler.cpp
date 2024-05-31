#include "scheduler.h"


/* Extended Task control block for managing periodic tasks within this library. */
typedef struct xExtended_TCB
{
	TaskFunction_t pvTaskCode; 		/* Function pointer to the code that will be run periodically. */
	const char *pcName; 			/* Name of the task. */
	UBaseType_t uxStackDepth; 			/* Stack size of the task. */
	void *pvParameters; 			/* Parameters to the task function. */
	UBaseType_t uxPriority; 		/* Priority of the task. */
	TaskHandle_t *pxTaskHandle;		/* Task handle for the task. */
	TickType_t xReleaseTime;		/* Release time of the task. */
	TickType_t xRelativeDeadline;	/* Relative deadline of the task. */
	TickType_t xAbsoluteDeadline;	/* Absolute deadline of the task. */
	TickType_t xPeriod;				/* Task period. */
	TickType_t xLastWakeTime; 		/* Last time stamp when the task was running. */
	TickType_t xMaxExecTime;		/* Worst-case execution time of the task. */
	TickType_t xExecTime;			/* Current execution time of the task. */

	BaseType_t xWorkIsDone; 		/* pdFALSE if the job is not finished, pdTRUE if the job is finished. */

	BaseType_t xPriorityIsSet; 	/* pdTRUE if the priority is assigned. */
	BaseType_t xInUse; 			/* pdFALSE if this extended TCB is empty. */
	
	#if( schedUSE_TIMING_ERROR_DETECTION_DEADLINE == 1 )
		BaseType_t xExecutedOnce;	/* pdTRUE if the task has executed once. */
	#endif 

	#if( schedUSE_TIMING_ERROR_DETECTION_EXECUTION_TIME == 1 || schedUSE_TIMING_ERROR_DETECTION_DEADLINE == 1 )
		TickType_t xAbsoluteUnblockTime; /* The task will be unblocked at this time if it is blocked by the scheduler task. */
	#endif 
	#if( schedUSE_TIMING_ERROR_DETECTION_EXECUTION_TIME == 1 )
		BaseType_t xSuspended; 		/* pdTRUE if the task is suspended. */
		BaseType_t xMaxExecTimeExceeded; /* pdTRUE when execTime exceeds maxExecTime. */
	#endif 
	
	/* add if you need anything else */	
	
} SchedTCB_t;


static BaseType_t prvGetTCBIndexFromHandle( TaskHandle_t xTaskHandle );
static void prvInitTCBArray( void );
static BaseType_t prvFindEmptyElementIndexTCB( void );
static void prvDeleteTCBFromArray( BaseType_t xIndex );

static TickType_t xSystemStartTime = 0;

static void prvPeriodicTaskCode( void *pvParameters );
static void prvCreateAllTasks( void );

#if( schedSCHEDULING_POLICY == schedSCHEDULING_POLICY_RMS || schedSCHEDULING_POLICY == schedSCHEDULING_POLICY_DMS )
	static void prvSetFixedPriorities( void );	
#endif /* schedSCHEDULING_POLICY_RMS */

#if( schedUSE_SCHEDULER_TASK == 1 )
	static void prvSchedulerCheckTimingError( TickType_t xTickCount, SchedTCB_t *pxTCB );
	static void prvSchedulerFunction( void );
	static void prvCreateSchedulerTask( void );
	static void prvWakeScheduler( void );

	#if( schedUSE_TIMING_ERROR_DETECTION_DEADLINE == 1 )
		static void prvPeriodicTaskRecreate( SchedTCB_t *pxTCB );
		static void prvDeadlineMissedHook( SchedTCB_t *pxTCB, TickType_t xTickCount );
		static void prvCheckDeadline( SchedTCB_t *pxTCB, TickType_t xTickCount );				
	#endif /* schedUSE_TIMING_ERROR_DETECTION_DEADLINE */

	#if( schedUSE_TIMING_ERROR_DETECTION_EXECUTION_TIME == 1 )
		static void prvExecTimeExceedHook( TickType_t xTickCount, SchedTCB_t *pxCurrentTask );
	#endif /* schedUSE_TIMING_ERROR_DETECTION_EXECUTION_TIME */
	
#endif /* schedUSE_SCHEDULER_TASK */

/* Array for extended TCBs. */
static SchedTCB_t xTCBArray[ schedMAX_NUMBER_OF_PERIODIC_TASKS ] = { 0 };
/* Counter for number of periodic tasks. */
static BaseType_t xTaskCounter = 0;

#if( schedUSE_SCHEDULER_TASK )
	static TickType_t xSchedulerWakeCounter = 0; /* useful. why? */
	static TaskHandle_t xSchedulerHandle = NULL; /* useful. why? */
#endif /* schedUSE_SCHEDULER_TASK */

/* Returns index position in xTCBArray of TCB with same task handle as parameter. */
static BaseType_t prvGetTCBIndexFromHandle( TaskHandle_t xTaskHandle )
{
	static BaseType_t xIndex = 0;
	BaseType_t xIterator;

	for( xIterator = 0; xIterator < schedMAX_NUMBER_OF_PERIODIC_TASKS; xIterator++ )
	{
	
		if( pdTRUE == xTCBArray[ xIndex ].xInUse && *xTCBArray[ xIndex ].pxTaskHandle == xTaskHandle )
		{
			return xIndex;
		}
	
		xIndex++;
		if( schedMAX_NUMBER_OF_PERIODIC_TASKS == xIndex )
		{
			xIndex = 0;
		}
	}
	return -1;
}

/* Initializes xTCBArray. */
static void prvInitTCBArray( void )
{
	UBaseType_t uxIndex;
	for( uxIndex = 0; uxIndex < schedMAX_NUMBER_OF_PERIODIC_TASKS; uxIndex++)
	{
		xTCBArray[ uxIndex ].xInUse = pdFALSE;
	}
}

/* Find index for an empty entry in xTCBArray. Returns -1 if there is no empty entry. */
static BaseType_t prvFindEmptyElementIndexTCB( void )
{
	/* your implementation goes here */
	BaseType_t xIndex = -1;
	BaseType_t xIterator;

	for (xIterator = 0; xIterator < schedMAX_NUMBER_OF_PERIODIC_TASKS; xIterator++ )
	{
		if (pdFALSE == xTCBArray[xIterator].xInUse)
		{
			xIndex = xIterator;
			break;
		}
	}

	return xIndex;
}

/* Remove a pointer to extended TCB from xTCBArray. */
static void prvDeleteTCBFromArray( BaseType_t xIndex )
{
	configASSERT(0 <= xIndex && xIndex < schedMAX_NUMBER_OF_PERIODIC_TASKS);
	configASSERT(pdTRUE == xTCBArray[xIndex].xInUse);

	if (pdTRUE == xTCBArray[xIndex].xInUse)
	{
		xTCBArray[xIndex].xInUse = pdFALSE;
		xTaskCounter--;
	}
}

/* The whole function code that is executed by every periodic task.
 * This function wraps the task code specified by the user. */
static void prvPeriodicTaskCode( void *pvParameters )
{
	TickType_t xStartTick, xEndTick;

	BaseType_t xIndex;
	SchedTCB_t *pxThisTask;	
	TaskHandle_t xCurrentTaskHandle = xTaskGetCurrentTaskHandle();  
	
	/* Check the handle is not NULL. */
	configASSERT(NULL != xCurrentTaskHandle);
	xIndex = prvGetTCBIndexFromHandle(xCurrentTaskHandle);
	configASSERT(-1 != xIndex);
	pxThisTask = &xTCBArray[xIndex];
    
	#if( schedUSE_TIMING_ERROR_DETECTION_DEADLINE == 1 )
        pxThisTask->xExecutedOnce = pdTRUE;
	#endif
    
	PRINTF("FUNC: %s", __func__);
	PRINTF(" -> TASK: %s, INIT RUN\n", pxThisTask->pcName);

	if( 0 == pxThisTask->xReleaseTime )
	{
		pxThisTask->xLastWakeTime = xSystemStartTime;
	}

	for( ; ; )
	{	
		pxThisTask->xWorkIsDone = pdFALSE;
		xTaskDelayUntil(&pxThisTask->xLastWakeTime, pxThisTask->xReleaseTime);
		
		PRINTF("TASK: %-2s\n",pxThisTask->pcName);
			
		xStartTick = xTaskGetTickCount();		
		pxThisTask->pvTaskCode( pvParameters );
		xEndTick = xTaskGetTickCount();
   
        pxThisTask->xWorkIsDone = pdTRUE;
		pxThisTask->xExecTime = 0;

		PRINTF("STAT: %-2s, ST:%04u, ET:%04u, RT:%02u, DT: %04u\n", pxThisTask->pcName, xStartTick, xEndTick, (xEndTick - xStartTick), pxThisTask->xAbsoluteDeadline);
	
		xTaskDelayUntil(&pxThisTask->xLastWakeTime, pxThisTask->xPeriod );
	}
}

/* Creates a periodic task. */
void vSchedulerPeriodicTaskCreate( TaskFunction_t pvTaskCode, const char *pcName, UBaseType_t uxStackDepth, void *pvParameters, UBaseType_t uxPriority,
		TaskHandle_t *pxCreatedTask, TickType_t xPhaseTick, TickType_t xPeriodTick, TickType_t xMaxExecTimeTick, TickType_t xDeadlineTick )
{
	PRINTF("FUNC: %s\n", __func__);

	taskENTER_CRITICAL();
	SchedTCB_t *pxNewTCB;
	
	BaseType_t xIndex = prvFindEmptyElementIndexTCB();
	configASSERT( xTaskCounter < schedMAX_NUMBER_OF_PERIODIC_TASKS );
	configASSERT( xIndex != -1 );
	pxNewTCB = &xTCBArray[ xIndex ];	

	/* Intialize item. */
		
	pxNewTCB->pvTaskCode = pvTaskCode;
	pxNewTCB->pcName = pcName;
	pxNewTCB->uxStackDepth = uxStackDepth;
	pxNewTCB->pvParameters = pvParameters;
	pxNewTCB->uxPriority = uxPriority;
	pxNewTCB->pxTaskHandle = pxCreatedTask;
	pxNewTCB->xReleaseTime = xPhaseTick;
	pxNewTCB->xPeriod = xPeriodTick;
	
    /* Populate the rest */
  	pxNewTCB->xMaxExecTime = xMaxExecTimeTick;
  	pxNewTCB->xRelativeDeadline = xDeadlineTick;
	pxNewTCB->xAbsoluteDeadline = pxNewTCB->xRelativeDeadline + pxNewTCB->xReleaseTime + xSystemStartTime;
  	pxNewTCB->xWorkIsDone = pdTRUE;
  	pxNewTCB->xExecTime = 0;

	pxNewTCB->xInUse = pdTRUE;

	#if( schedSCHEDULING_POLICY == schedSCHEDULING_POLICY_RMS || schedSCHEDULING_POLICY == schedSCHEDULING_POLICY_DMS )
	    pxNewTCB->xPriorityIsSet = pdFALSE;

	#endif
	
	#if( schedUSE_TIMING_ERROR_DETECTION_DEADLINE == 1 )
        pxNewTCB->xExecutedOnce = pdFALSE;
	#endif
	
	#if( schedUSE_TIMING_ERROR_DETECTION_EXECUTION_TIME == 1 )
        pxNewTCB->xSuspended = pdFALSE;
        pxNewTCB->xMaxExecTimeExceeded = pdFALSE;
	#endif	

	xTaskCounter++;	

	PRINTF("---- Task Details ----\n");
	PRINTF("Name                : %s\n", pxNewTCB->pcName);
	PRINTF("Phase Tick          : %u\n", pxNewTCB->xReleaseTime);
	PRINTF("Max. Execution Tick : %u\n", pxNewTCB->xMaxExecTime);
	PRINTF("Rel. Deadline Tick  : %u\n", pxNewTCB->xRelativeDeadline);
	PRINTF("Period Tick         : %u\n", pxNewTCB->xPeriod);
	PRINTF("Priority            : %d\n", pxNewTCB->uxPriority);
	PRINTF("----------------------\n\n");

	taskEXIT_CRITICAL();
}

/* Deletes a periodic task. */
void vSchedulerPeriodicTaskDelete( TaskHandle_t xTaskHandle )
{
	PRINTF("FUNC: %s\n", __func__);
	
	BaseType_t xIndex;

	if (NULL != xTaskHandle)
	{
		xIndex = prvGetTCBIndexFromHandle(xTaskHandle);
	}
	else
	{
		xTaskHandle = xTaskGetCurrentTaskHandle();
		xIndex = prvGetTCBIndexFromHandle(xTaskHandle);
	}
	
	configASSERT(-1 != xIndex);

	prvDeleteTCBFromArray(xIndex);
	vTaskDelete(xTaskHandle);
}

/* Creates all periodic tasks stored in TCB array, or TCB list. */
static void prvCreateAllTasks( void )
{
	PRINTF("FUNC: %s\n", __func__);
	
	SchedTCB_t *pxTCB;

	BaseType_t xIndex;
	for( xIndex = 0; xIndex < xTaskCounter; xIndex++ )
	{
		configASSERT( pdTRUE == xTCBArray[ xIndex ].xInUse );
		pxTCB = &xTCBArray[ xIndex ];

		BaseType_t xReturnValue = xTaskCreate(prvPeriodicTaskCode, pxTCB->pcName, 
												pxTCB->uxStackDepth, pxTCB->pvParameters,
												pxTCB->uxPriority, pxTCB->pxTaskHandle);

	}
}

#if( schedSCHEDULING_POLICY == schedSCHEDULING_POLICY_RMS || schedSCHEDULING_POLICY == schedSCHEDULING_POLICY_DMS )

	/* Initiazes fixed priorities of all periodic tasks with respect to RMS policy. */
	static void prvSetFixedPriorities( void )
	{
		PRINTF("FUNC: %s\n", __func__);

		#if( schedSCHEDULING_POLICY == schedSCHEDULING_POLICY_RMS )
			PRINTF("----Using RM Scheduling Algorithm----\n");
		#elif( schedSCHEDULING_POLICY == schedSCHEDULING_POLICY_DMS ) 
			PRINTF("----Using DM Scheduling Algorithm----\n");
		#endif

		BaseType_t xIter, xIndex;
		TickType_t xShortest, xPreviousShortest=0;
		SchedTCB_t *pxShortestTaskPointer, *pxTCB;

		#if( schedUSE_SCHEDULER_TASK == 1 )
			BaseType_t xHighestPriority = schedSCHEDULER_PRIORITY; 
		#else
			BaseType_t xHighestPriority = configMAX_PRIORITIES;
		#endif /* schedUSE_SCHEDULER_TASK */

		for( xIter = 0; xIter < xTaskCounter; xIter++ )
		{
			xShortest = portMAX_DELAY;

			/* search for shortest period */
			for( xIndex = 0; xIndex < xTaskCounter; xIndex++ )
			{
				pxTCB = &xTCBArray[xIndex];
				configASSERT(pdTRUE == pxTCB->xInUse);

				if (pdFALSE == pxTCB->xPriorityIsSet)
				{
					#if( schedSCHEDULING_POLICY == schedSCHEDULING_POLICY_RMS )
						if (pxTCB->xPeriod <= xShortest)
						{
							xShortest = pxTCB->xPeriod;
							pxShortestTaskPointer = pxTCB;
						}
					#elif( schedSCHEDULING_POLICY == schedSCHEDULING_POLICY_DMS )
						if (pxTCB->xRelativeDeadline <= xShortest)
						{
							xShortest = pxTCB->xRelativeDeadline;
							pxShortestTaskPointer = pxTCB;
						}
					#endif /* schedSCHEDULING_POLICY */
				}
			}
			
			/* set highest priority to task with xShortest period (the highest priority is configMAX_PRIORITIES-1) */		
			if (xShortest != xPreviousShortest)
			{
				xHighestPriority--;
			}

			configASSERT(tskIDLE_PRIORITY < xHighestPriority);

			pxShortestTaskPointer->uxPriority = xHighestPriority;
			pxShortestTaskPointer->xPriorityIsSet = pdTRUE;

			xPreviousShortest = xShortest;
		
			PRINTF(" Task : %s, Priority : %d, Tick : %d\n", 
			pxShortestTaskPointer->pcName, pxShortestTaskPointer->uxPriority, xShortest);
		
		}
		PRINTF("-------------------------------------\n\n");
	}

#endif /* schedSCHEDULING_POLICY */


#if( schedUSE_TIMING_ERROR_DETECTION_DEADLINE == 1 )

	/* Recreates a deleted task that still has its information left in the task array (or list). */
	static void prvPeriodicTaskRecreate( SchedTCB_t *pxTCB )
	{
		BaseType_t xReturnValue = xTaskCreate(prvPeriodicTaskCode, pxTCB->pcName, 
											  pxTCB->uxStackDepth, pxTCB->pvParameters,
		 								      pxTCB->uxPriority, pxTCB->pxTaskHandle );
				                      		
		if( pdPASS == xReturnValue )
		{
			pxTCB->xExecutedOnce = pdFALSE;	

			#if( schedUSE_TIMING_ERROR_DETECTION_EXECUTION_TIME == 1 )	

				pxTCB->xSuspended = pdFALSE;
				pxTCB->xMaxExecTimeExceeded = pdFALSE;

			#endif /* schedUSE_TIMING_ERROR_DETECTION_EXECUTION_TIME */
		}
		else
		{
			configASSERT(pdFAIL);
		}
	}

	/* Called when a deadline of a periodic task is missed.
	 * Deletes the periodic task that has missed it's deadline and recreate it.
	 * The periodic task is released during next period. */
	static void prvDeadlineMissedHook( SchedTCB_t *pxTCB, TickType_t xTickCount )
	{
		PRINTF("FUNC: %s", __func__);
		PRINTF(" -> TASK: %s, T : %d\n", pxTCB->pcName, xTickCount);

		/* Delete the pxTask and recreate it. */
		vTaskDelete(*(pxTCB->pxTaskHandle));
		pxTCB->xExecTime = 0;
		prvPeriodicTaskRecreate(pxTCB);	
		
		pxTCB->xReleaseTime = pxTCB->xLastWakeTime + pxTCB->xPeriod;
		pxTCB->xAbsoluteDeadline = pxTCB->xLastWakeTime + pxTCB->xPeriod + pxTCB->xRelativeDeadline;
		pxTCB->xLastWakeTime = 0;
	}

	/* Checks whether given task has missed deadline or not. */
	static void prvCheckDeadline( SchedTCB_t *pxTCB, TickType_t xTickCount )
	{  
		if(  NULL != pxTCB )
		{ 
			pxTCB->xAbsoluteDeadline = pxTCB->xLastWakeTime + pxTCB->xRelativeDeadline;

			if (( signed ) ( pxTCB->xAbsoluteDeadline - xTickCount ) < 0)
			{
				prvDeadlineMissedHook( pxTCB, xTickCount );
			}	
		}
	}	

#endif /* schedUSE_TIMING_ERROR_DETECTION_DEADLINE */


#if( schedUSE_TIMING_ERROR_DETECTION_EXECUTION_TIME == 1 )

	/* Called if a periodic task has exceeded its worst-case execution time.
	 * The periodic task is blocked until next period. A context switch to
	 * the scheduler task occur to block the periodic task. */
	static void prvExecTimeExceedHook( TickType_t xTickCount, SchedTCB_t *pxCurrentTask )
	{
		PRINTF("FUNC: %s", __func__);
		PRINTF(" -> TASK: %s, T : %d\n", pxCurrentTask->pcName, xTickCount);

        pxCurrentTask->xMaxExecTimeExceeded = pdTRUE;

        /* Is not suspended yet, but will be suspended by the scheduler later. */
        pxCurrentTask->xSuspended = pdTRUE;
        pxCurrentTask->xAbsoluteUnblockTime = pxCurrentTask->xLastWakeTime + pxCurrentTask->xPeriod;
        pxCurrentTask->xExecTime = 0;
        
        BaseType_t xHigherPriorityTaskWoken;
        vTaskNotifyGiveFromISR( xSchedulerHandle, &xHigherPriorityTaskWoken );
        xTaskResumeFromISR(xSchedulerHandle);
	}

#endif /* schedUSE_TIMING_ERROR_DETECTION_EXECUTION_TIME */


#if( schedUSE_SCHEDULER_TASK == 1 )
	
	/* Called by the scheduler task. Checks all tasks for any enabled
	 * Timing Error Detection feature. */
	static void prvSchedulerCheckTimingError( TickType_t xTickCount, SchedTCB_t *pxTCB )
	{
		#if( schedUSE_TIMING_ERROR_DETECTION_DEADLINE == 1 )						
			
			if ((pdTRUE == pxTCB->xWorkIsDone) && (( signed ) ( xTickCount - pxTCB->xLastWakeTime ) > 0))
			{
				pxTCB->xWorkIsDone = pdFALSE;
			}

			/* check if task missed deadline */
			if ((pdTRUE == pxTCB->xExecutedOnce) && (pdFALSE == pxTCB->xWorkIsDone ))
			{
				prvCheckDeadline(pxTCB, xTickCount);
			}

		#endif /* schedUSE_TIMING_ERROR_DETECTION_DEADLINE */
		

		#if( schedUSE_TIMING_ERROR_DETECTION_EXECUTION_TIME == 1 )

        if( pdTRUE == pxTCB->xMaxExecTimeExceeded )
        {
            pxTCB->xMaxExecTimeExceeded = pdFALSE;
            vTaskSuspend( *pxTCB->pxTaskHandle );
        }
        if( pdTRUE == pxTCB->xSuspended )
        {
            if( ( signed ) ( pxTCB->xAbsoluteUnblockTime - xTickCount ) <= 0 )
            {
                pxTCB->xSuspended = pdFALSE;
                pxTCB->xLastWakeTime = xTickCount;
                vTaskResume( *pxTCB->pxTaskHandle );
            }
        }

		#endif /* schedUSE_TIMING_ERROR_DETECTION_EXECUTION_TIME */

		return;
	}

	/* Function code for the scheduler task. */
	static void prvSchedulerFunction( void *pvParameters )
	{
		for( ; ; )
		{ 
     		#if( schedUSE_TIMING_ERROR_DETECTION_DEADLINE == 1 || schedUSE_TIMING_ERROR_DETECTION_EXECUTION_TIME == 1 )
				
				TickType_t xTickCount = xTaskGetTickCount();
				UBaseType_t xIndex;
        		SchedTCB_t *pxTCB;
        		
				for (xIndex = 0; xIndex < schedMAX_NUMBER_OF_PERIODIC_TASKS; xIndex++)
				{
					pxTCB = &xTCBArray[xIndex];

					/* Only check tasks which are in use */
					if (pdTRUE == pxTCB->xInUse)
					{
						prvSchedulerCheckTimingError(xTickCount, pxTCB);
					}
				}
			
			#endif /* schedUSE_TIMING_ERROR_DETECTION_DEADLINE || schedUSE_TIMING_ERROR_DETECTION_EXECUTION_TIME */

			#if (schedOVERHEAD == 1)

				TickType_t xTicks = schedOVERHEAD_TICKS;
				while(xTicks--)
				{
					// Runs the CPU for 15 ms at FCLK = 16Mhz; 15 ms is WatchDog Timer Interrupt
					_delay_loop_2(60000);
				}

			#endif

			ulTaskNotifyTake( pdTRUE, portMAX_DELAY );
		}
	}

	/* Creates the scheduler task. */
	static void prvCreateSchedulerTask( void )
	{
		PRINTF("FUNC: %s\n", __func__);
		PRINTF("---- Scheduler Details ----\n");
		PRINTF("Period Tick  : %d\n", schedSCHEDULER_TASK_PERIOD);
		PRINTF("Priority     : %d\n", schedSCHEDULER_PRIORITY);
		PRINTF("Overhead     : %d\n", schedOVERHEAD);
		PRINTF("---------------------------\n\n");

		xTaskCreate( (TaskFunction_t) prvSchedulerFunction, "Scheduler", schedSCHEDULER_TASK_STACK_SIZE, NULL, schedSCHEDULER_PRIORITY, &xSchedulerHandle );                             
	}

#endif /* schedUSE_SCHEDULER_TASK */


#if( schedUSE_SCHEDULER_TASK == 1 )
	
	/* Wakes up (context switches to) the scheduler task. */
	static void prvWakeScheduler( void )
	{
		BaseType_t xHigherPriorityTaskWoken;
		vTaskNotifyGiveFromISR( xSchedulerHandle, &xHigherPriorityTaskWoken );
		xTaskResumeFromISR(xSchedulerHandle);  
	}

	/* Called every software tick. */
	// In FreeRTOSConfig.h,
	// Enable configUSE_TICK_HOOK
	// Enable INCLUDE_xTaskGetIdleTaskHandle
	// Enable INCLUDE_xTaskGetCurrentTaskHandle
	void vApplicationTickHook( void )
	{    
		TickType_t xStartTick, xEndTick;

		SchedTCB_t *pxCurrentTask;		
		TaskHandle_t xCurrentTaskHandle = xTaskGetCurrentTaskHandle();		
        UBaseType_t flag = 0;
        BaseType_t xIndex;
		BaseType_t prioCurrentTask = uxTaskPriorityGet(xCurrentTaskHandle);

		for(xIndex = 0; xIndex < xTaskCounter ; xIndex++){
			pxCurrentTask = &xTCBArray[xIndex];
			if(pxCurrentTask -> uxPriority == prioCurrentTask){
				flag = 1;
				break;
			}
		}
		
		if( xCurrentTaskHandle != xSchedulerHandle && xCurrentTaskHandle != xTaskGetIdleTaskHandle() && flag == 1)
		{
			pxCurrentTask->xExecTime++;     
     
			#if( schedUSE_TIMING_ERROR_DETECTION_EXECUTION_TIME == 1 )
            if( pxCurrentTask->xMaxExecTime <= pxCurrentTask->xExecTime )
            {
                if( pdFALSE == pxCurrentTask->xMaxExecTimeExceeded )
                {
                    if( pdFALSE == pxCurrentTask->xSuspended )
                    {
                        prvExecTimeExceedHook( xTaskGetTickCountFromISR(), pxCurrentTask );
                    }
                }
            }
			#endif /* schedUSE_TIMING_ERROR_DETECTION_EXECUTION_TIME */
		}

		#if( schedUSE_TIMING_ERROR_DETECTION_DEADLINE == 1 )    
			
			xSchedulerWakeCounter++;      
			if( xSchedulerWakeCounter == schedSCHEDULER_TASK_PERIOD )
			{
				xSchedulerWakeCounter = 0;        
				prvWakeScheduler();
			}

		#endif /* schedUSE_TIMING_ERROR_DETECTION_DEADLINE */
	}

#endif /* schedUSE_SCHEDULER_TASK */

/* This function must be called before any other function call from this module. */
void vSchedulerInit( void )
{
	PRINTF("FUNC: %s\n", __func__);

	prvInitTCBArray();

}

/* Starts scheduling tasks. All periodic tasks (including polling server) must
 * have been created with API function before calling this function. */
void vSchedulerStart( void )
{
	PRINTF("FUNC: %s\n", __func__);

	#if( schedSCHEDULING_POLICY == schedSCHEDULING_POLICY_RMS || schedSCHEDULING_POLICY == schedSCHEDULING_POLICY_DMS )
		prvSetFixedPriorities();	
	#endif /* schedSCHEDULING_POLICY */

	#if( schedUSE_SCHEDULER_TASK == 1 )
		prvCreateSchedulerTask();
	#endif /* schedUSE_SCHEDULER_TASK */

	prvCreateAllTasks();
	  
	xSystemStartTime = xTaskGetTickCount();
	
	vTaskStartScheduler();
}
