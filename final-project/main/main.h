#ifndef FINAL_H_
#define FINAL_H_

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

	struct
	{
		TickType_t xRunTick;
	} xParameters;

} TaskProperties_t;

#endif