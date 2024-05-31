#ifndef PROJECT4_H_
#define PROJECT4_H_

#include <util/delay_basic.h>
#include "scheduler.h"

/* Struct */
typedef struct TaskProperties
{
	TaskHandle_t xHandle;

	const char xTaskName[8];
	const TickType_t xPhaseTick;
	const TickType_t xMaxExecTick;
	const TickType_t xDeadlineTick;
	const TickType_t xPeriodTick;

} TaskProperties_t;

#endif