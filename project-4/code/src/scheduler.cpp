#include "scheduler.h"

#define schedUSE_TCB_ARRAY 1

#define prvGetRCBFromHandle( pxHandle )    ( SchedRCB_t * ) ( pxHandle )

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

	#if( schedUSE_TCB_ARRAY == 1 )
		BaseType_t xPriorityIsSet; 	/* pdTRUE if the priority is assigned. */
		BaseType_t xInUse; 			/* pdFALSE if this extended TCB is empty. */
	#endif

	#if( schedUSE_TIMING_ERROR_DETECTION_DEADLINE == 1 )
		BaseType_t xExecutedOnce;	/* pdTRUE if the task has executed once. */
	#endif /* schedUSE_TIMING_ERROR_DETECTION_DEADLINE */

	#if( schedUSE_TIMING_ERROR_DETECTION_EXECUTION_TIME == 1 || schedUSE_TIMING_ERROR_DETECTION_DEADLINE == 1 )
		TickType_t xAbsoluteUnblockTime; /* The task will be unblocked at this time if it is blocked by the scheduler task. */
	#endif /* schedUSE_TIMING_ERROR_DETECTION_EXECUTION_TIME || schedUSE_TIMING_ERROR_DETECTION_DEADLINE */

	#if( schedUSE_TIMING_ERROR_DETECTION_EXECUTION_TIME == 1 )
		BaseType_t xSuspended; 		/* pdTRUE if the task is suspended. */
		BaseType_t xMaxExecTimeExceeded; /* pdTRUE when execTime exceeds maxExecTime. */
	#endif /* schedUSE_TIMING_ERROR_DETECTION_EXECUTION_TIME */
	
	/* add if you need anything else */	
	UBaseType_t uxActivePriority;
	TaskHandle_t *pxBlockerTaskHandle;
	BaseType_t xIsBlocked;  
	UBaseType_t xHasResource;

} SchedTCB_t;

/************************************** RESOURCE ACCESS PROTOCOL DECLARATIONS ***********************************************/

/* Resource Control Block */
typedef struct xRCB
{
    const char *pcName; 			    /* Name of the resource. */

    SchedTCB_t *pxHolderTCB;            /* Task handle that holds the lock. */

	UBaseType_t uxPriorityCeiling;	    /* Priority ceiling of the resource. */

	BaseType_t xIsLocked; 			    /* pdFALSE, if this resource is not locked. */

	TaskHandle_t *pxUsedByTask[5];		/* Holds all the tasks handles that may use the resource */

	BaseType_t xUsedByTaskCounter;      /* Number of tasks that may hold the resource */

	BaseType_t xInUse = pdFALSE; 	    /* pdFALSE, if this RCB is empty. */

} SchedRCB_t;

/* Initializes the xSCArray */
static void prvInitRCBArray( void );

/* This function assigns priority ceiling to
 * the resources based on tasks' requirement. 
 * */
static void prvSetPriorityCeilingToResources( void );

/* This function updates system priority ceiling 
 * to  the maximum priority ceiling among 
 * all the semaphores currently locked by tasks 
 * other than the one in execution. 
 * */
static void prvUpdateSystemPriorityCeiling( void );

static void prvFreeAllResourcesHeldByTask( SchedTCB_t *pxTCB );



static BaseType_t prvCheckResourcesPriorityCeilingHeldByTask( SchedTCB_t *pxTaskHandle );

static void prvLockResource(SchedRCB_t *pxRCB, SchedTCB_t *pxTCB);

static void prvDenyResource(SchedTCB_t *pxBlockedTCB, SchedTCB_t *pxTCB);

static void prvBlockTask(SchedTCB_t *pxTCB);

static void prvResourceWait(SchedRCB_t *pxRCB, SchedTCB_t *pxTCB);



static void prvFreeResource(SchedRCB_t *pxRCB, SchedTCB_t *pxTCB);

static void prvUpdateTaskPriority(SchedTCB_t *pxTCB);

static void prvUnblockTasks( SchedTCB_t *pxTCB );

static void prvResourceSignal(SchedRCB_t *pxRCB, SchedTCB_t *pxTCB);



/* Array for RCBs. */
static SchedRCB_t xRCBArray[5] = {};

/* Counter for number of resources. */
static BaseType_t xResourceCounter = 0;

/* The current system priority ceiling value */
static UBaseType_t uxSystemPriorityCeiling = 0;

/* Pointer to the current system priority ceiling RCB */
static SchedRCB_t *pxSystemPriorityCeilingPointer = NULL;

/****************************************************************************************************************************/

#if( schedUSE_TCB_ARRAY == 1 )
	static BaseType_t prvGetTCBIndexFromHandle( TaskHandle_t xTaskHandle );
	static SchedTCB_t *prvGetTCBFromHandle( TaskHandle_t xTaskHandle );
	static void prvInitTCBArray( void );
	/* Find index for an empty entry in xTCBArray. Return -1 if there is no empty entry. */
	static BaseType_t prvFindEmptyElementIndexTCB( void );
	/* Remove a pointer to extended TCB from xTCBArray. */
	static void prvDeleteTCBFromArray( BaseType_t xIndex );
#endif /* schedUSE_TCB_ARRAY */

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



#if( schedUSE_TCB_ARRAY == 1 )
	/* Array for extended TCBs. */
	static SchedTCB_t xTCBArray[ schedMAX_NUMBER_OF_PERIODIC_TASKS ] = { 0 };
	/* Counter for number of periodic tasks. */
	static BaseType_t xTaskCounter = 0;
#endif /* schedUSE_TCB_ARRAY */

#if( schedUSE_SCHEDULER_TASK )
	static TickType_t xSchedulerWakeCounter = 0; /* useful. why? */
	static TaskHandle_t xSchedulerHandle = NULL; /* useful. why? */
#endif /* schedUSE_SCHEDULER_TASK */


#if( schedUSE_TCB_ARRAY == 1 )
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

	static SchedTCB_t *prvGetTCBFromHandle( TaskHandle_t xTaskHandle )
	{
		static BaseType_t xIndex = 0;
		BaseType_t xIterator;

		for( xIterator = 0; xIterator < schedMAX_NUMBER_OF_PERIODIC_TASKS; xIterator++ )
		{
		
			if( pdTRUE == xTCBArray[ xIndex ].xInUse && *xTCBArray[ xIndex ].pxTaskHandle == xTaskHandle )
			{
				return &(xTCBArray[ xIndex ]);
			}
		
			xIndex++;
			if( schedMAX_NUMBER_OF_PERIODIC_TASKS == xIndex )
			{
				xIndex = 0;
			}
		}

		return NULL;
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
	
#endif /* schedUSE_TCB_ARRAY */


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
	#endif /* schedUSE_TIMING_ERROR_DETECTION_DEADLINE */
    
	PRINTF("\nFUNC: %s", __func__);
	PRINTF(" -> TASK: %s, INIT RUN\n", pxThisTask->pcName);

	if( 0 == pxThisTask->xReleaseTime )
	{
		pxThisTask->xLastWakeTime = xSystemStartTime;
	}
	else
	{
		xTaskDelayUntil(&pxThisTask->xLastWakeTime, pxThisTask->xReleaseTime);
	}

	for( ; ; )
	{	
		//PRINTF("TASK: %-2s\n",pxThisTask->pcName);
			
		pxThisTask->xWorkIsDone = pdFALSE;

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
	PRINTF("\nFUNC: %s\n", __func__);

	taskENTER_CRITICAL();
	SchedTCB_t *pxNewTCB;
	
	#if( schedUSE_TCB_ARRAY == 1 )
		BaseType_t xIndex = prvFindEmptyElementIndexTCB();
		configASSERT( xTaskCounter < schedMAX_NUMBER_OF_PERIODIC_TASKS );
		configASSERT( xIndex != -1 );
		pxNewTCB = &xTCBArray[ xIndex ];	
	#endif /* schedUSE_TCB_ARRAY */

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

	#if( schedUSE_TCB_ARRAY == 1 )
		pxNewTCB->xInUse = pdTRUE;
	#endif /* schedUSE_TCB_ARRAY */
	
	#if( schedSCHEDULING_POLICY == schedSCHEDULING_POLICY_RMS || schedSCHEDULING_POLICY == schedSCHEDULING_POLICY_DMS )
        pxNewTCB->xPriorityIsSet = pdFALSE;
	#endif /* schedSCHEDULING_POLICY */
	
	#if( schedUSE_TIMING_ERROR_DETECTION_DEADLINE == 1 )
        pxNewTCB->xExecutedOnce = pdFALSE;
	#endif /* schedUSE_TIMING_ERROR_DETECTION_DEADLINE */
	
	#if( schedUSE_TIMING_ERROR_DETECTION_EXECUTION_TIME == 1 )
        pxNewTCB->xSuspended = pdFALSE;
        pxNewTCB->xMaxExecTimeExceeded = pdFALSE;
	#endif /* schedUSE_TIMING_ERROR_DETECTION_EXECUTION_TIME */	

	#if( schedUSE_TCB_ARRAY == 1 )
		xTaskCounter++;	
	#endif /* schedUSE_TCB_SORTED_LIST */
	
	/* add if you need anything else */
	pxNewTCB->uxActivePriority = uxPriority;
	pxNewTCB->pxBlockerTaskHandle = NULL;
	pxNewTCB->xIsBlocked = pdFALSE;
	pxNewTCB->xHasResource = 0;

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
	PRINTF("\nFUNC: %s\n", __func__);
	
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
	PRINTF("\nFUNC: %s\n", __func__);
	
	SchedTCB_t *pxTCB;

	#if( schedUSE_TCB_ARRAY == 1 )
		BaseType_t xIndex;
		for( xIndex = 0; xIndex < xTaskCounter; xIndex++ )
		{
			configASSERT( pdTRUE == xTCBArray[ xIndex ].xInUse );
			pxTCB = &xTCBArray[ xIndex ];

			BaseType_t xReturnValue = xTaskCreate(prvPeriodicTaskCode, pxTCB->pcName, 
												  pxTCB->uxStackDepth, pxTCB->pvParameters,
												  pxTCB->uxPriority, pxTCB->pxTaskHandle);

		}	
	#endif /* schedUSE_TCB_ARRAY */
}

#if( schedSCHEDULING_POLICY == schedSCHEDULING_POLICY_RMS || schedSCHEDULING_POLICY == schedSCHEDULING_POLICY_DMS )

	/* Initiazes fixed priorities of all periodic tasks with respect to RMS policy. */
static void prvSetFixedPriorities( void )
{
	PRINTF("\nFUNC: %s\n", __func__);

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
		pxShortestTaskPointer->uxActivePriority = xHighestPriority;
		pxShortestTaskPointer->xPriorityIsSet = pdTRUE;

		xPreviousShortest = xShortest;
	
		PRINTF(" Task : %s, Priority : %d, Tick : %d\n", 
		pxShortestTaskPointer->pcName, pxShortestTaskPointer->uxPriority, xShortest);
	
	}
	PRINTF("-------------------------------------\n");
}
#endif /* schedSCHEDULING_POLICY */


#if( schedUSE_TIMING_ERROR_DETECTION_DEADLINE == 1 )

	/* Recreates a deleted task that still has its information left in the task array (or list). */
	static void prvPeriodicTaskRecreate( SchedTCB_t *pxTCB )
	{
		BaseType_t xReturnValue = xTaskCreate(prvPeriodicTaskCode, pxTCB->pcName, 
											  pxTCB->uxStackDepth, pxTCB->pvParameters,
		 								      pxTCB->uxPriority, pxTCB->pxTaskHandle);
				                      		
		if( pdPASS == xReturnValue )
		{
			pxTCB->xExecutedOnce = pdFALSE;		
			pxTCB->xSuspended = pdFALSE;
			pxTCB->xMaxExecTimeExceeded = pdFALSE;

			pxTCB->uxActivePriority = pxTCB->uxPriority;
			pxTCB->pxBlockerTaskHandle = NULL;
			pxTCB->xIsBlocked = pdFALSE;
			pxTCB->xHasResource = 0;
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
		PRINTF("\nFUNC: %s", __func__);
		PRINTF(" -> TASK: %s @ T : %d\n", pxTCB->pcName, xTickCount);

		/* Free up all resources held by task */
		prvFreeAllResourcesHeldByTask(pxTCB);

		/* Delete the pxTask and recreate it. */
		vTaskDelete(*(pxTCB->pxTaskHandle));
		pxTCB->xExecTime = 0;
		prvPeriodicTaskRecreate(pxTCB);	
		
		pxTCB->xReleaseTime = pxTCB->xLastWakeTime + pxTCB->xPeriod;
		pxTCB->xAbsoluteDeadline = pxTCB->xLastWakeTime + pxTCB->xPeriod + pxTCB->xRelativeDeadline;
	}

	/* Checks whether given task has missed deadline or not. */
	static void prvCheckDeadline( SchedTCB_t *pxTCB, TickType_t xTickCount )
	{   
		pxTCB->xAbsoluteDeadline = pxTCB->xLastWakeTime + pxTCB->xRelativeDeadline;

		if (( signed ) ( pxTCB->xAbsoluteDeadline - xTickCount ) < 0)
		{
			prvDeadlineMissedHook( pxTCB, xTickCount );
		}	
	}	
#endif /* schedUSE_TIMING_ERROR_DETECTION_DEADLINE */


#if( schedUSE_TIMING_ERROR_DETECTION_EXECUTION_TIME == 1 )

	/* Called if a periodic task has exceeded its worst-case execution time.
	 * The periodic task is blocked until next period. A context switch to
	 * the scheduler task occur to block the periodic task. */
	static void prvExecTimeExceedHook( TickType_t xTickCount, SchedTCB_t *pxCurrentTask )
	{
		PRINTF("\nFUNC: %s", __func__);
		PRINTF(" -> TASK: %s @ T : %d\n", pxCurrentTask->pcName, xTickCount);

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
			/* Free up all resources held by task */
			prvFreeAllResourcesHeldByTask(pxTCB);

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
		PRINTF("\nFUNC: %s\n", __func__);
		PRINTF("---- Scheduler Details ----\n");
		PRINTF("Period Tick  : %d\n", schedSCHEDULER_TASK_PERIOD);
		PRINTF("Priority     : %d\n", schedSCHEDULER_PRIORITY);
		PRINTF("Overhead     : %d\n", schedOVERHEAD);
		PRINTF("---------------------------\n");

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
	PRINTF("\nFUNC: %s\n", __func__);

	#if( schedUSE_TCB_ARRAY == 1 )
		prvInitTCBArray();
	#endif /* schedUSE_TCB_ARRAY */

	prvInitRCBArray();
}


/* Starts scheduling tasks. All periodic tasks (including polling server) must
 * have been created with API function before calling this function. */
void vSchedulerStart( void )
{
	PRINTF("\nFUNC: %s\n", __func__);

	#if( schedSCHEDULING_POLICY == schedSCHEDULING_POLICY_RMS || schedSCHEDULING_POLICY == schedSCHEDULING_POLICY_DMS )
		prvSetFixedPriorities();	
	#endif /* schedSCHEDULING_POLICY */

	#if( schedUSE_SCHEDULER_TASK == 1 )
		prvCreateSchedulerTask();
	#endif /* schedUSE_SCHEDULER_TASK */

	prvCreateAllTasks();

	prvSetPriorityCeilingToResources();

	xSystemStartTime = xTaskGetTickCount();
	
	vTaskStartScheduler();
}

/************************************** RESOURCE ACCESS PROTOCOL DEFINITIONS ***********************************************/

static void prvInitRCBArray( void )
{
 	UBaseType_t uxResourceIndex;
	for( uxResourceIndex = 0; uxResourceIndex < schedMAX_NUMBER_OF_RESOURCES; uxResourceIndex++)
	{
		xRCBArray[ uxResourceIndex ].xInUse = pdFALSE;
	}   
}

static void prvSetPriorityCeilingToResources( void )
{
	#if ( schedRESOURCE_ACCESS_PROTOCOL == schedRESOURCE_ACCESS_PROTOCOL_OCPP )
		PRINTF("----Using OCPP----\n");
	#elif ( schedRESOURCE_ACCESS_PROTOCOL == schedRESOURCE_ACCESS_PROTOCOL_ICPP ) 
		PRINTF("----Using ICPP----\n");
	#endif

	PRINTF("\nFUNC: %s\n", __func__);

	UBaseType_t uxResourceIndex;
	UBaseType_t uxTaskIndex;
	UBaseType_t uxPriorityCeiling;

	SchedRCB_t *pxRCB;

	for( uxResourceIndex = 0; uxResourceIndex < schedMAX_NUMBER_OF_RESOURCES; uxResourceIndex++)
	{
		/* Check if resource is in use */
		if ( pdTRUE == xRCBArray[ uxResourceIndex ].xInUse )
		{ 
			/* Start with lowest priority */
			uxPriorityCeiling = tskIDLE_PRIORITY;

			pxRCB = &xRCBArray[ uxResourceIndex ];

			/* Get task with highest priority from xUsedByTask Array */
			for( uxTaskIndex = 0; uxTaskIndex < pxRCB->xUsedByTaskCounter ; uxTaskIndex++)
			{
				/* Check if priority is greater than current priority ceiling */
				if ( uxPriorityCeiling < uxTaskPriorityGet(*(pxRCB->pxUsedByTask[ uxTaskIndex ])) )
				{
					uxPriorityCeiling = uxTaskPriorityGet(*(pxRCB->pxUsedByTask[ uxTaskIndex ]));
				}
				else
				{
					mtCOVERAGE_TEST_MARKER();
				}
			}

			/* Assign highest priority to the resource */
			pxRCB->uxPriorityCeiling = uxPriorityCeiling;

			#if DEBUG
				PRINTF("---- Resource Details ----------\n");
				PRINTF("Name                           : %s\n", pxRCB->pcName);
				PRINTF("Priority Ceiling               : %d\n", pxRCB->uxPriorityCeiling);
				PRINTF("Number Of Tasks Using Resource : %d\n", pxRCB->xUsedByTaskCounter);
				PRINTF("Tasks Utilizing Resource       : [ ");
				for( uxTaskIndex = 0; uxTaskIndex < pxRCB->xUsedByTaskCounter ; uxTaskIndex++)
				{
					PRINTF("%s ", pcTaskGetName(*(pxRCB->pxUsedByTask[ uxTaskIndex ])));
				}
				PRINTF("]\n");
				PRINTF("--------------------------------\n\n");
			#endif
		}
		else
		{
			mtCOVERAGE_TEST_MARKER();
		}
	}
}

static void prvUpdateSystemPriorityCeiling( void )
{
	UBaseType_t uxResourceIndex;

	SchedRCB_t *pxRCB;

	UBaseType_t uxMaxPriorityCeiling = tskIDLE_PRIORITY;

	SchedRCB_t *uxMaxPriorityCeilingRCB = NULL;

	for( uxResourceIndex = 0; uxResourceIndex < schedMAX_NUMBER_OF_RESOURCES; uxResourceIndex++)
	{
		/* Check if resource is in use */
		if ( pdTRUE == xRCBArray[ uxResourceIndex ].xInUse )
		{ 
			pxRCB = &xRCBArray[ uxResourceIndex ];

			/* Check if resource is locked */
			if ( pdTRUE == pxRCB->xIsLocked )
			{
				/* Check for priority ceiling */
				if ( uxMaxPriorityCeiling < pxRCB->uxPriorityCeiling )
				{
					uxMaxPriorityCeiling = pxRCB->uxPriorityCeiling;
					uxMaxPriorityCeilingRCB = pxRCB;

				}			
				else
				{
					mtCOVERAGE_TEST_MARKER()
				}
			}
			else
			{
				mtCOVERAGE_TEST_MARKER()
			}
		}
		else
		{
			mtCOVERAGE_TEST_MARKER()
		}
	}

	uxSystemPriorityCeiling        = uxMaxPriorityCeiling;
	pxSystemPriorityCeilingPointer = uxMaxPriorityCeilingRCB;

}

static void prvFreeAllResourcesHeldByTask( SchedTCB_t *pxTCB )
{
	UBaseType_t uxResourceIndex;

	if(pxTCB->xHasResource)
	{
		vTaskSuspendAll();

		for( uxResourceIndex = 0; uxResourceIndex < schedMAX_NUMBER_OF_RESOURCES; uxResourceIndex++)
		{
			/* Check if resource is in use */
			if ( pdTRUE == xRCBArray[ uxResourceIndex ].xInUse )
			{
				/* Check if resource is locked */
				if ( pdTRUE == xRCBArray[ uxResourceIndex ].xIsLocked )
				{
					/* Check if given task has locked the resource */
					if ( pxTCB == xRCBArray[ uxResourceIndex ].pxHolderTCB )
					{
						configASSERT( *(pxRCB->pxHolderTCB->pxTaskHandle) == *(pxTCB->pxTaskHandle) )

						PRINTF("%s Freed By %s\n", xRCBArray[ uxResourceIndex ].pcName, pxTCB->pcName);

						prvFreeResource(&(xRCBArray[ uxResourceIndex ]), pxTCB);
						prvUnblockTasks(pxTCB);

					}
					else
					{
						mtCOVERAGE_TEST_MARKER()
					}
				}
				else
				{
					mtCOVERAGE_TEST_MARKER()
				}
			}
			else
			{
				mtCOVERAGE_TEST_MARKER()
			}
		}

		xTaskResumeAll();	

		prvUpdateSystemPriorityCeiling();
	}
}



static BaseType_t prvCheckResourcesPriorityCeilingHeldByTask( SchedTCB_t *pxTCB )
{
	UBaseType_t uxResourceIndex;
	BaseType_t  xFlag = pdFALSE;

	for( uxResourceIndex = 0; uxResourceIndex < schedMAX_NUMBER_OF_RESOURCES; uxResourceIndex++)
	{
		/* Check if resource is in use */
		if ( pdTRUE == xRCBArray[ uxResourceIndex ].xInUse )
		{
			/* Check if resource is locked */
			if ( pdTRUE == xRCBArray[ uxResourceIndex ].xIsLocked )
			{
				/* Check if given task has locked the resource */
				if ( pxTCB == xRCBArray[ uxResourceIndex ].pxHolderTCB )
				{
					/* Check if priority ceiling is equal to system ceiling priority */
					if ( uxSystemPriorityCeiling == xRCBArray[ uxResourceIndex ].uxPriorityCeiling )
					{
						xFlag = pdTRUE;
						break;
					}
					else
					{
						mtCOVERAGE_TEST_MARKER()
					}
				}
				else
				{
					mtCOVERAGE_TEST_MARKER()
				}
			}
			else
			{
				mtCOVERAGE_TEST_MARKER()
			}
		}
		else
		{
			mtCOVERAGE_TEST_MARKER()
		}
	}

	return xFlag;
}

static void prvLockResource(SchedRCB_t *pxRCB, SchedTCB_t *pxTCB)
{
	taskENTER_CRITICAL();

	pxRCB->xIsLocked    = pdTRUE;
	pxRCB->pxHolderTCB  = pxTCB;

	pxTCB->xHasResource++;

	taskEXIT_CRITICAL();
}

static void prvDenyResource(SchedTCB_t *pxBlockedTCB, SchedTCB_t *pxTCB)
{
	PRINTF(" %s Blocked %s @ T : %d\n", pxBlockedTCB->pcName, pxTCB->pcName, xTaskGetTickCount());

	taskENTER_CRITICAL();

	pxBlockedTCB->xIsBlocked          = pdTRUE;
	pxBlockedTCB->pxBlockerTaskHandle = pxTCB->pxTaskHandle;

	#if( schedRESOURCE_ACCESS_PROTOCOL == schedRESOURCE_ACCESS_PROTOCOL_OCPP)
		pxTCB->uxActivePriority = pxBlockedTCB->uxPriority;
		vTaskPrioritySet(*(pxTCB->pxTaskHandle), pxTCB->uxActivePriority);
	#endif

	taskEXIT_CRITICAL();
}

static void prvBlockTask(SchedTCB_t *pxTCB)
{
	do 
	{
		taskYIELD();
	} while ( pxTCB->xIsBlocked );
}

static void prvResourceWait(SchedRCB_t *pxRCB, SchedTCB_t *pxTCB)
{
	#if( schedRESOURCE_ACCESS_PROTOCOL == schedRESOURCE_ACCESS_PROTOCOL_OCPP)
		for ( ; ; )
		{
			PRINTF("\nFUNC: %s\n", __func__);

			/* Check if a resource is free */
			if (pdFALSE == pxRCB->xIsLocked)
			{
				/* Check if task priority is strictly greater than system priority ceiling */
				if ( pxTCB->uxActivePriority > uxSystemPriorityCeiling )
				{
					prvLockResource(pxRCB, pxTCB);
					prvUpdateSystemPriorityCeiling();

					PRINTF("%s Locked By %s |", pxRCB->pcName, pxTCB->pcName);
					PRINTF(" SPC : %d @ T : %d\n", uxSystemPriorityCeiling, xTaskGetTickCount());

					return;
				}
				else
				{
					/* Check if task is holding any resource with priority equal to system priority ceiling */
					if ( prvCheckResourcesPriorityCeilingHeldByTask(pxTCB) )
					{

						prvLockResource(pxRCB, pxTCB);
						prvUpdateSystemPriorityCeiling();

						PRINTF("%s Locked By %s |", pxRCB->pcName, pxTCB->pcName);
						PRINTF(" SPC : %d @ T : %d\n", uxSystemPriorityCeiling, xTaskGetTickCount());

						return;
					}
					else
					{
						PRINTF("%s Denied To %s |", pxRCB->pcName, pxTCB->pcName);

						prvDenyResource(pxTCB, pxSystemPriorityCeilingPointer->pxHolderTCB);
						prvBlockTask(pxTCB);
					}
				}
			}
			else
			{
				PRINTF("%s Denied To %s |", pxRCB->pcName, pxTCB->pcName);
				
				prvDenyResource(pxTCB, pxRCB->pxHolderTCB);
				prvBlockTask(pxTCB);
			}
		}
	#elif( schedRESOURCE_ACCESS_PROTOCOL == schedRESOURCE_ACCESS_PROTOCOL_ICPP)
		for ( ; ; )
		{
			PRINTF("\nFUNC: %s\n", __func__);

			/* Check if a resource is free */
			if (pdFALSE == pxRCB->xIsLocked)
			{
				/* Check if task priority is strictly greater than system priority ceiling */
				if ( pxTCB->uxActivePriority > pxRCB->uxPriorityCeiling )
				{
					prvLockResource(pxRCB, pxTCB);

					PRINTF("%s Locked By %s @ T : %d\n", pxRCB->pcName, pxTCB->pcName, xTaskGetTickCount());

					return;
				}
				else
				{
					prvLockResource(pxRCB, pxTCB);

					taskENTER_CRITICAL();

					pxTCB->uxActivePriority = pxRCB->uxPriorityCeiling;
					vTaskPrioritySet(*(pxTCB->pxTaskHandle), pxTCB->uxActivePriority);
					
					taskEXIT_CRITICAL();

					PRINTF("%s Locked By %s\n", pxRCB->pcName, pxTCB->pcName);

					return;

				}
			}
			else
			{
				PRINTF("%s Denied To %s |", pxRCB->pcName, pxTCB->pcName);
				
				prvDenyResource(pxTCB, pxRCB->pxHolderTCB);
				prvBlockTask(pxTCB);
			}
		}		
	#endif

}

void vSchedulerResourceWait( ResourceHandle_t xResourceHandle)
{
	SchedRCB_t *pxRCB; 
	TaskHandle_t xTaskHandle;
	SchedTCB_t *pxTCB;

	/* Check if task handle is empty*/
	configASSERT(NULL != xResourceHandle);
	pxRCB = prvGetRCBFromHandle(xResourceHandle);

	xTaskHandle = xTaskGetCurrentTaskHandle();
	pxTCB       = prvGetTCBFromHandle(xTaskHandle);

	prvResourceWait(pxRCB, pxTCB);
}



static void prvFreeResource(SchedRCB_t *pxRCB, SchedTCB_t *pxTCB)
{
	taskENTER_CRITICAL();

	/* Unlock the resource*/
	pxRCB->xIsLocked   = pdFALSE;
	pxRCB->pxHolderTCB = NULL;

	pxTCB->xHasResource--;

	taskEXIT_CRITICAL();
}

static void prvUpdateTaskPriority(SchedTCB_t *pxTCB)
{
	UBaseType_t uxResourceIndex;
	UBaseType_t uxResourcePriorityCeiling = tskIDLE_PRIORITY;

	taskENTER_CRITICAL();

	for( uxResourceIndex = 0; uxResourceIndex < schedMAX_NUMBER_OF_RESOURCES; uxResourceIndex++)
	{
		/* Check if resource is in use */
		if ( pdTRUE == xRCBArray[ uxResourceIndex ].xInUse )
		{
			/* Check if resource is locked */
			if ( pdTRUE == xRCBArray[ uxResourceIndex ].xIsLocked )
			{
				/* Check if task has locked the resource */
				if ( pxTCB == xRCBArray[ uxResourceIndex ].pxHolderTCB )
				{
					/* Check if priority ceiling is equal to system ceiling priority */
					if ( uxResourcePriorityCeiling < xRCBArray[ uxResourceIndex ].uxPriorityCeiling )
					{
						uxResourcePriorityCeiling = xRCBArray[ uxResourceIndex ].uxPriorityCeiling;
					}
					else
					{
						mtCOVERAGE_TEST_MARKER()
					}
				}
				else
				{
					mtCOVERAGE_TEST_MARKER()
				}
			}
			else
			{
				mtCOVERAGE_TEST_MARKER()
			}
		}
		else
		{
			mtCOVERAGE_TEST_MARKER()
		}
	}

	taskEXIT_CRITICAL();

	if (tskIDLE_PRIORITY != uxResourcePriorityCeiling)
	{
		pxTCB->uxActivePriority = uxResourcePriorityCeiling;
	}
	else
	{
		pxTCB->uxActivePriority = pxTCB->uxPriority;
	}

	vTaskPrioritySet(*(pxTCB->pxTaskHandle), pxTCB->uxActivePriority);
}

static void prvUnblockTasks( SchedTCB_t *pxTCB )
{
	UBaseType_t uxIndex;
	SchedTCB_t *pxTempTCB;

	taskENTER_CRITICAL();
	
	for (uxIndex = 0; uxIndex < schedMAX_NUMBER_OF_PERIODIC_TASKS; uxIndex++)
	{
		pxTempTCB = &xTCBArray[uxIndex];

		/* Only check tasks which are in use */
		if (pdTRUE == pxTempTCB->xInUse)
		{
			/* Check if task is blocked */
			if ( pdTRUE == pxTempTCB->xIsBlocked )
			{
				/* Check if task is by blocked by given task */
				if ( *(pxTCB->pxTaskHandle) == *(pxTempTCB->pxBlockerTaskHandle) )
				{
					PRINTF("%s Unblocked %s @ T : %d\n", pxTempTCB->pcName, pxTCB->pcName, xTaskGetTickCount());

					pxTempTCB->xIsBlocked          = pdFALSE;
					pxTempTCB->pxBlockerTaskHandle = NULL;

				}
				else
				{
					mtCOVERAGE_TEST_MARKER()
				}
			}
			else
			{
				mtCOVERAGE_TEST_MARKER()
			}
		}
		else
		{
			mtCOVERAGE_TEST_MARKER()
		}
	}

	taskEXIT_CRITICAL();
}

static void prvResourceSignal(SchedRCB_t *pxRCB, SchedTCB_t *pxTCB)
{
	PRINTF("\nFUNC: %s\n", __func__);

	configASSERT( *(pxRCB->pxHolderTCB->pxTaskHandle) == *(pxTCB->pxTaskHandle) )

	PRINTF("%s Freed By %s @ T : %d\n", pxRCB->pcName, pxTCB->pcName, xTaskGetTickCount());

	vTaskSuspendAll();

	prvFreeResource(pxRCB, pxTCB);

	#if( schedRESOURCE_ACCESS_PROTOCOL == schedRESOURCE_ACCESS_PROTOCOL_OCPP)
		prvUpdateSystemPriorityCeiling();
	#endif

	prvUpdateTaskPriority(pxTCB);

	prvUnblockTasks(pxTCB);

	xTaskResumeAll();
}

void vSchedulerResourceSignal( ResourceHandle_t xResourceHandle)
{
	SchedRCB_t *pxRCB; 

	TaskHandle_t xTaskHandle;
	SchedTCB_t *pxTCB;

	/* Check if task handle is empty*/
	configASSERT(NULL != xResourceHandle);

	pxRCB = prvGetRCBFromHandle(xResourceHandle);

	xTaskHandle = xTaskGetCurrentTaskHandle();
	pxTCB = prvGetTCBFromHandle(xTaskHandle);

	prvResourceSignal(pxRCB, pxTCB);
}



ResourceHandle_t xSchedulerCreateResource( const char *pcName )
{
	PRINTF("\nFUNC: %s\n", __func__);

    SchedRCB_t *pxNewRCB;
      
	configASSERT( xResourceCounter < schedMAX_NUMBER_OF_RESOURCES );

	pxNewRCB = &xRCBArray[ xResourceCounter ];

	pxNewRCB->pcName = pcName;

	pxNewRCB->pxHolderTCB = NULL;
	pxNewRCB->uxPriorityCeiling = tskIDLE_PRIORITY;
	pxNewRCB->xIsLocked = pdFALSE;

	pxNewRCB->xUsedByTaskCounter = 0;

	pxNewRCB->xInUse = pdTRUE;

	xResourceCounter++;

	return (ResourceHandle_t) pxNewRCB;
}

void vSchedulerResourceUsedByTask( ResourceHandle_t xResourceHandle, TaskHandle_t *pxTaskHandle )
{
	PRINTF("\nFUNC: %s\n", __func__);

	SchedRCB_t *pxRCB; 

	/* Check if task handle is empty*/
	configASSERT(NULL != xResourceHandle);

	/* Check if Resource handle is empty */
	configASSERT(NULL != xTaskHandle);
	
	/* Check if too many tasks are added */
	configASSERT(pxRCB->xUsedByTaskCounter < schedMAX_NUMBER_OF_PERIODIC_TASKS);

	pxRCB = prvGetRCBFromHandle(xResourceHandle);

	pxRCB->pxUsedByTask[pxRCB->xUsedByTaskCounter] = pxTaskHandle;
	
	pxRCB->xUsedByTaskCounter++;	
}

/************************************** RESOURCE ACCESS PROTOCOL DEFINITIONS ***********************************************/
