#include "scheduler.h"

#if( schedSCHEDULING_POLICY == schedSCHEDULING_POLICY_EDF )
	#include "list.h"
#endif /* schedSCHEDULING_POLICY_EDF */

#define schedTHREAD_LOCAL_STORAGE_POINTER_INDEX 0

#define prvGetTCBFromHandle(x) 		( SchedTCB_t * ) pvTaskGetThreadLocalStoragePointer( x, schedTHREAD_LOCAL_STORAGE_POINTER_INDEX );
#define prvSetTCBForHandle(x, y) 	vTaskSetThreadLocalStoragePointer(x, schedTHREAD_LOCAL_STORAGE_POINTER_INDEX, ( SchedTCB_t * ) y );

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

	ListItem_t xTCBListItem; 	/* Used to reference TCB from the TCB list. */
	
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

} SchedTCB_t;

static void prvInitTCBList( void );
static void prvAddTCBToList( SchedTCB_t *pxTCB );
static void prvDeleteTCBFromList(  SchedTCB_t *pxTCB );

static TickType_t xSystemStartTime = 0;

static void prvPeriodicTaskCode( void *pvParameters );
static void prvCreateAllTasks( void );

#if( schedSCHEDULING_POLICY == schedSCHEDULING_POLICY_EDF )

	static void prvInit( void );
	static void prvSetPriorites( void );
	static void prvUpdatePrioritiesEDF( void );
	static void prvSwapList( List_t **ppxList1, List_t **ppxList2 );
	
#endif /* schedSCHEDULING_POLICY_EDF */

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

static List_t xTCBList;						/* Sorted linked list for all periodic tasks. */
static List_t xTCBTempList;					/* A temporary list used for switching lists. */
static List_t xTCBOverflowedList; 			/* Sorted linked list for periodic tasks that have overflowed deadline. */
static List_t *pxTCBList = NULL;  			/* Pointer to xTCBList. */
static List_t *pxTCBTempList = NULL;		/* Pointer to xTCBTempList. */
static List_t *pxTCBOverflowedList = NULL;	/* Pointer to xTCBOverflowedList. */


#if( schedUSE_SCHEDULER_TASK )

	static TickType_t xSchedulerWakeCounter = 0;
	static TaskHandle_t xSchedulerHandle = NULL;

#endif /* schedUSE_SCHEDULER_TASK */

static void prvInitTCBList( void )
{
	vListInitialise( &xTCBList );
	vListInitialise( &xTCBTempList );
	vListInitialise( &xTCBOverflowedList );
	pxTCBList = &xTCBList;
	pxTCBTempList = &xTCBTempList;
	pxTCBOverflowedList = &xTCBOverflowedList;
}

/* Add an extended TCB to sorted linked list. */
static void prvAddTCBToList( SchedTCB_t *pxTCB )
{
	/* Initialise TCB list item. */
	vListInitialiseItem( &pxTCB->xTCBListItem );

	/* Set owner of list item to the TCB. */
	listSET_LIST_ITEM_OWNER( &pxTCB->xTCBListItem, pxTCB );

	/* List is sorted by absolute deadline value. */
	listSET_LIST_ITEM_VALUE( &pxTCB->xTCBListItem, pxTCB->xAbsoluteDeadline );
	
	/* Insert TCB into list. */
	vListInsert( pxTCBList, &pxTCB->xTCBListItem );

}

/* Delete an extended TCB from sorted linked list. */
static void prvDeleteTCBFromList(  SchedTCB_t *pxTCB )
{
	uxListRemove( &pxTCB->xTCBListItem );
	vPortFree( pxTCB );
}


#if( schedSCHEDULING_POLICY == schedSCHEDULING_POLICY_EDF )
	
	/* Swap content of two lists. */
	static void prvSwapList( List_t **ppxList1, List_t **ppxList2 )
	{
		List_t *pxTemp;
		pxTemp = *ppxList1;
		*ppxList1 = *ppxList2;
		*ppxList2 = pxTemp;
	}

	static void prvSetPriorites( void )
	{
		SchedTCB_t *pxTCB;

		#if( schedUSE_SCHEDULER_TASK == 1 )
			UBaseType_t uxHighestPriority = schedSCHEDULER_PRIORITY - 1;
		#else
			UBaseType_t uxHighestPriority = configMAX_PRIORITIES - 1;
		#endif /* schedUSE_SCHEDULER_TASK */

		const ListItem_t *pxTCBListEndMarker = listGET_END_MARKER( pxTCBList );
		ListItem_t *pxTCBListItem = listGET_HEAD_ENTRY( pxTCBList );

		while( pxTCBListItem != pxTCBListEndMarker )
		{
			pxTCB = listGET_LIST_ITEM_OWNER( pxTCBListItem );

			configASSERT( -1 <= uxHighestPriority );

			pxTCB->uxPriority = uxHighestPriority;
			if (NULL != *pxTCB->pxTaskHandle)
			{
				vTaskPrioritySet( *pxTCB->pxTaskHandle, pxTCB->uxPriority );
			}

			uxHighestPriority--;
			pxTCBListItem = listGET_NEXT( pxTCBListItem );
		}
	}

	/* Update priorities of all periodic tasks with respect to EDF policy. */
	static void prvUpdatePrioritiesEDF( void )
	{
		SchedTCB_t *pxTCB;

		ListItem_t *pxTCBListItem;
		ListItem_t *pxTCBListItemTemp;
	
		if( listLIST_IS_EMPTY( pxTCBList ) && !listLIST_IS_EMPTY( pxTCBOverflowedList ) )
		{
			prvSwapList( &pxTCBList, &pxTCBOverflowedList );
		}

		const ListItem_t *pxTCBListEndMarker = listGET_END_MARKER( pxTCBList );
		pxTCBListItem = listGET_HEAD_ENTRY( pxTCBList );

		while( pxTCBListItem != pxTCBListEndMarker )
		{
			pxTCB = listGET_LIST_ITEM_OWNER( pxTCBListItem );

			/* Update priority in the SchedTCB list. */
			listSET_LIST_ITEM_VALUE( pxTCBListItem, pxTCB->xAbsoluteDeadline );

			pxTCBListItemTemp = pxTCBListItem;
			pxTCBListItem = listGET_NEXT( pxTCBListItem );
			uxListRemove( pxTCBListItem->pxPrevious );

			/* If absolute deadline overflowed, insert TCB to overflowed list. */
			if( pxTCB->xAbsoluteDeadline < pxTCB->xLastWakeTime )
			{
				vListInsert( pxTCBOverflowedList, pxTCBListItemTemp );
			}
			else /* Insert TCB into temp list in usual case. */
			{
				vListInsert( pxTCBTempList, pxTCBListItemTemp );
			}
		}

		/* Swap list with temp list. */
		prvSwapList( &pxTCBList, &pxTCBTempList );

		prvSetPriorites();
	
	}

#endif /* schedSCHEDULING_POLICY_EDF */

/* The whole function code that is executed by every periodic task.
 * This function wraps the task code specified by the user. */
static void prvPeriodicTaskCode( void *pvParameters )
{
	TaskHandle_t xTaskHandle;
	SchedTCB_t *pxThisTask;

	xTaskHandle = xTaskGetCurrentTaskHandle();
	pxThisTask  = prvGetTCBFromHandle(xTaskHandle);

	TickType_t xStartTick, xEndTick;
	
	configASSERT( NULL != pxThisTask );

	#if( schedUSE_TIMING_ERROR_DETECTION_DEADLINE == 1 )
		pxThisTask->xExecutedOnce = pdTRUE;
	#endif /* schedUSE_TIMING_ERROR_DETECTION_DEADLINE */
	
	PRINTF("FUNC: %s", __func__);
	PRINTF(" -> TASK: %s, INIT RUN\n", pxThisTask->pcName);

	if( 0 == pxThisTask->xReleaseTime )
	{
		pxThisTask->xLastWakeTime = xSystemStartTime;
	}

	for( ; ; )
	{
		prvWakeScheduler();
	
		pxThisTask->xWorkIsDone = pdFALSE;
		xTaskDelayUntil(&pxThisTask->xLastWakeTime, pxThisTask->xReleaseTime);

		PRINTF("TASK: %-2s\n",pxThisTask->pcName);

		xStartTick = xTaskGetTickCount();		
		pxThisTask->pvTaskCode( pvParameters );
		xEndTick = xTaskGetTickCount();

		pxThisTask->xWorkIsDone = pdTRUE;

		pxThisTask->xExecTime = 0;
		
		PRINTF("STAT: %-2s, ST:%04u, ET:%04u, RT:%02u, DT: %04u\n", pxThisTask->pcName, xStartTick, xEndTick, (xEndTick - xStartTick), pxThisTask->xAbsoluteDeadline);
	
		pxThisTask->xAbsoluteDeadline = pxThisTask->xLastWakeTime + pxThisTask->xPeriod + pxThisTask->xRelativeDeadline;
		prvWakeScheduler();

		xTaskDelayUntil( &pxThisTask->xLastWakeTime, pxThisTask->xPeriod );
	}
}

/* Creates a periodic task. */
void vSchedulerPeriodicTaskCreate( TaskFunction_t pvTaskCode, const char *pcName, UBaseType_t uxStackDepth, void *pvParameters, UBaseType_t uxPriority,
		TaskHandle_t *pxCreatedTask, TickType_t xPhaseTick, TickType_t xPeriodTick, TickType_t xMaxExecTimeTick, TickType_t xDeadlineTick )
{
	PRINTF("FUNC: %s\n", __func__);

	taskENTER_CRITICAL();
	SchedTCB_t *pxNewTCB;

	pxNewTCB = pvPortMalloc( sizeof( SchedTCB_t ) );

	/* Intialize item. */
	pxNewTCB->pvTaskCode 	= pvTaskCode;
	pxNewTCB->pcName 		= pcName;
	pxNewTCB->uxStackDepth 	= uxStackDepth;
	pxNewTCB->pvParameters 	= pvParameters;
	pxNewTCB->uxPriority 	= uxPriority;
	pxNewTCB->pxTaskHandle	= pxCreatedTask;
	pxNewTCB->xReleaseTime 	= xPhaseTick;
	pxNewTCB->xPeriod 		= xPeriodTick;
	
    /* Populate the rest */
  	pxNewTCB->xMaxExecTime = xMaxExecTimeTick;
  	pxNewTCB->xRelativeDeadline = xDeadlineTick;
	pxNewTCB->xAbsoluteDeadline = pxNewTCB->xRelativeDeadline + pxNewTCB->xReleaseTime + xSystemStartTime;
  	pxNewTCB->xWorkIsDone = pdTRUE;
  	pxNewTCB->xExecTime = 0;

	#if( schedSCHEDULING_POLICY == schedSCHEDULING_POLICY_EDF )
		pxNewTCB->xAbsoluteDeadline = pxNewTCB->xRelativeDeadline + pxNewTCB->xReleaseTime + xSystemStartTime;
		pxNewTCB->uxPriority = 0;
	#endif /* schedSCHEDULING_POLICY */
	
	#if( schedUSE_TIMING_ERROR_DETECTION_DEADLINE == 1 )
		pxNewTCB->xExecutedOnce = pdFALSE;
	#endif /* schedUSE_TIMING_ERROR_DETECTION_DEADLINE */
	
	#if( schedUSE_TIMING_ERROR_DETECTION_EXECUTION_TIME == 1 )
		pxNewTCB->xSuspended = pdFALSE;
		pxNewTCB->xMaxExecTimeExceeded = pdFALSE;
	#endif /* schedUSE_TIMING_ERROR_DETECTION_EXECUTION_TIME */

	prvAddTCBToList( pxNewTCB );

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
	
	SchedTCB_t *pxTCB;

	if( xTaskHandle == NULL )
	{
		xTaskHandle = xTaskGetCurrentTaskHandle();
	}

	pxTCB = prvGetTCBFromHandle(xTaskHandle)

	prvDeleteTCBFromList(pxTCB);
	vTaskDelete( xTaskHandle );
}

/* Creates all periodic tasks stored in TCB array, or TCB list. */
static void prvCreateAllTasks( void )
{
	PRINTF("FUNC: %s\n", __func__);
	
	SchedTCB_t *pxTCB;

	const ListItem_t *pxTCBListEndMarker = listGET_END_MARKER( pxTCBList );
	ListItem_t *pxTCBListItem = listGET_HEAD_ENTRY( pxTCBList );

	while( pxTCBListItem != pxTCBListEndMarker )
	{
		pxTCB = listGET_LIST_ITEM_OWNER( pxTCBListItem );
		configASSERT( NULL != pxTCB );

		BaseType_t xReturnValue = xTaskCreate( prvPeriodicTaskCode, pxTCB->pcName, pxTCB->uxStackDepth, pxTCB->pvParameters, pxTCB->uxPriority, pxTCB->pxTaskHandle );

		prvSetTCBForHandle(*pxTCB->pxTaskHandle, pxTCB );
		pxTCBListItem = listGET_NEXT( pxTCBListItem );
	}
}

#if( schedSCHEDULING_POLICY == schedSCHEDULING_POLICY_EDF )

	/* Initializes priorities of all periodic tasks with respect to EDF policy. */
	static void prvInit( void )
	{
		PRINTF("FUNC: %s\n", __func__);

		prvSetPriorites();
	}

#endif /* schedSCHEDULING_POLICY */

#if( schedUSE_TIMING_ERROR_DETECTION_DEADLINE == 1 )

	/* Recreates a deleted task that still has its information left in the task array (or list). */
	static void prvPeriodicTaskRecreate( SchedTCB_t *pxTCB )
	{
		BaseType_t xReturnValue = xTaskCreate( prvPeriodicTaskCode, pxTCB->pcName, pxTCB->uxStackDepth, pxTCB->pvParameters, pxTCB->uxPriority, pxTCB->pxTaskHandle );
		if( pdPASS == xReturnValue )
		{
			
			prvSetTCBForHandle(*pxTCB->pxTaskHandle, pxTCB);

			/* This must be set to false so that the task does not miss the deadline immediately when it is created. */
			pxTCB->xExecutedOnce = pdFALSE;
			#if( schedUSE_TIMING_ERROR_DETECTION_EXECUTION_TIME == 1 )
				pxTCB->xSuspended = pdFALSE;
				pxTCB->xMaxExecTimeExceeded = pdFALSE;
			#endif /* schedUSE_TIMING_ERROR_DETECTION_EXECUTION_TIME */

		}
		else
		{
			/* if task creation failed */
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
		vTaskDelete( *pxTCB->pxTaskHandle );
		pxTCB->xExecTime = 0;
		prvPeriodicTaskRecreate( pxTCB );

		pxTCB->xReleaseTime = pxTCB->xLastWakeTime + pxTCB->xPeriod;
		/* Need to reset lastWakeTime for correct release. */
		pxTCB->xLastWakeTime = 0;
		pxTCB->xAbsoluteDeadline = pxTCB->xRelativeDeadline + pxTCB->xReleaseTime;
	}

	/* Checks whether given task has missed deadline or not. */
	static void prvCheckDeadline( SchedTCB_t *pxTCB, TickType_t xTickCount )
	{
		if( ( NULL != pxTCB ) && ( pdFALSE == pxTCB->xWorkIsDone ) && ( pdTRUE == pxTCB->xExecutedOnce ) )
		{
			/* Need to update absolute deadline if the scheduling policy is not EDF. */
			#if( schedSCHEDULING_POLICY != schedSCHEDULING_POLICY_EDF )
				pxTCB->xAbsoluteDeadline = pxTCB->xLastWakeTime + pxTCB->xRelativeDeadline;
			#endif /* schedSCHEDULING_POLICY */

			/* Using ICTOH method proposed by Carlini and Buttazzo, to check whether deadline is missed. */
			if( ( signed ) ( pxTCB->xAbsoluteDeadline - xTickCount ) < 0 )
			{
				/* Deadline is missed. */
				prvDeadlineMissedHook( pxTCB, xTickCount );
			}
		}
	}

#endif /* schedUSE_TIMING_ERROR_DETECTION_DEADLINE */

#if( schedUSE_TIMING_ERROR_DETECTION_EXECUTION_TIME == 1 )

	/* Called if a periodic task has exceeded it's worst-case execution time.
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
			
			/* Since lastWakeTime is updated to next wake time when the task is delayed, tickCount > lastWakeTime implies that
			* the task has not finished it's job this period. */

			/* Using ICTOH method proposed by Carlini and Buttazzo, to check the condition unaffected by counter overflows. */
			if( ( signed ) ( xTickCount - pxTCB->xLastWakeTime ) > 0 )
			{
				pxTCB->xWorkIsDone = pdFALSE;
			}

			prvCheckDeadline( pxTCB, xTickCount );

		#endif /* schedUSE_TIMING_ERROR_DETECTION_DEADLINE */
		
		#if( schedUSE_TIMING_ERROR_DETECTION_EXECUTION_TIME == 1 )

			if( pdTRUE == pxTCB->xMaxExecTimeExceeded )
			{
				pxTCB->xMaxExecTimeExceeded = pdFALSE;
				vTaskSuspend( *pxTCB->pxTaskHandle );
			}
			if( pdTRUE == pxTCB->xSuspended )
			{
				/* Using ICTOH method proposed by Carlini and Buttazzo, to check whether absolute unblock time is reached. */
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
	static void prvSchedulerFunction( void )
	{
		for( ; ; )
		{
			#if( schedSCHEDULING_POLICY == schedSCHEDULING_POLICY_EDF )

				prvUpdatePrioritiesEDF();

			#endif /* schedSCHEDULING_POLICY_EDF */

			#if( schedUSE_TIMING_ERROR_DETECTION_DEADLINE == 1 || schedUSE_TIMING_ERROR_DETECTION_EXECUTION_TIME == 1 )

				TickType_t xTickCount = xTaskGetTickCount();
				SchedTCB_t *pxTCB;

				const ListItem_t *pxTCBListEndMarker = listGET_END_MARKER( pxTCBList );
				ListItem_t *pxTCBListItem = listGET_HEAD_ENTRY( pxTCBList );

				while( pxTCBListItem != pxTCBListEndMarker )
				{
					pxTCB = listGET_LIST_ITEM_OWNER( pxTCBListItem);

					prvSchedulerCheckTimingError( xTickCount, pxTCB );

					pxTCBListItem = listGET_NEXT( pxTCBListItem );
				}
				
			#endif /* schedUSE_TIMING_ERROR_DETECTION_DEADLINE || schedUSE_TIMING_ERROR_DETECTION_EXECUTION_TIME */

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
	void vApplicationTickHook( void )
	{
		SchedTCB_t *pxCurrentTask;
		TaskHandle_t xCurrentTaskHandle = xTaskGetCurrentTaskHandle();

		pxCurrentTask = prvGetTCBFromHandle(xCurrentTaskHandle);

		if( NULL != pxCurrentTask && xCurrentTaskHandle != xSchedulerHandle && xCurrentTaskHandle != xTaskGetIdleTaskHandle() )
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

	prvInitTCBList();

}

/* Starts scheduling tasks. All periodic tasks (including polling server) must
 * have been created with API function before calling this function. */
void vSchedulerStart( void )
{
	PRINTF("FUNC: %s\n", __func__);

	#if( schedSCHEDULING_POLICY == schedSCHEDULING_POLICY_EDF )
		prvInit();
	#endif /* schedSCHEDULING_POLICY */

	#if( schedUSE_SCHEDULER_TASK == 1 )
		prvCreateSchedulerTask();
	#endif /* schedUSE_SCHEDULER_TASK */

	prvCreateAllTasks();

	xSystemStartTime = xTaskGetTickCount();
	vTaskStartScheduler();
}
