#include <Arduino_FreeRTOS.h>
#include <task.h>

// Wait times of the tasks
#define TASK1_DELAY     2000
#define TASK2_DELAY     3000
#define TASK3_DELAY     5000

// Priorities of the tasks
#define TASK1_PRIORITY  2
#define TASK2_PRIORITY  3
#define TASK3_PRIORITY  1

// Stack Size of the task
#define STACK_SIZE      128

// Global Variables
static const int A = 20;
static const int B = 10;

// Function Declarations
void Task1 ( void *pvParameters );
void Task2 ( void *pvParameters );
void Task3 ( void *pvParameters );

void setup() {
  
  // initialize serial communication at 115200 bits per second:
  Serial.begin(230400);
  
  // Wait for serial port to connect.
  while (!Serial); 
  
  Serial.println("----- SETUP SECTION -----");

  // Creating Tasks
  xTaskCreate(Task1, "Task1",  STACK_SIZE, NULL,  TASK1_PRIORITY,  NULL);
  xTaskCreate(Task2, "Task2",  STACK_SIZE, NULL,  TASK2_PRIORITY,  NULL);
  xTaskCreate(Task3, "Task3",  STACK_SIZE, NULL,  TASK3_PRIORITY,  NULL);

  Serial.println("----- END OF SECTION ----\n");
   
}

void loop()
{
  // Empty. Things are done in Tasks.
}

/*--------------------------------------------------*/
/*---------------------- Tasks ---------------------*/
/*--------------------------------------------------*/

void Task1(void *pvParameters)
{
  (void) pvParameters;
  
  // Getting the task handle of the task
  const TaskHandle_t task_handle = xTaskGetCurrentTaskHandle();

  // Getting the task name
  const char *task_name = pcTaskGetName(task_handle);

  int j = 0;

  while(1)
  {
    Serial.print("Running  : ");
    Serial.println(task_name);
    Serial.print("Blocking : ");
    Serial.println(task_name); 
    vTaskDelay(pdMS_TO_TICKS(TASK1_DELAY));
  }
     
}

void Task2(void *pvParameters)
{
  (void) pvParameters;
  
  // Getting the task handle of the task
  const TaskHandle_t task_handle = xTaskGetCurrentTaskHandle();

  // Getting the task name
  const char *task_name = pcTaskGetName(task_handle);

  while(1)
  {
    Serial.print("Running  : ");
    Serial.println(task_name);
    Serial.print("Blocking : ");
    Serial.println(task_name); 
    vTaskDelay(pdMS_TO_TICKS(TASK2_DELAY));
  }
     
}

void Task3(void *pvParameters)
{
  (void) pvParameters;

  // Getting the task handle of the task
  const TaskHandle_t task_handle = xTaskGetCurrentTaskHandle();

  // Getting the task_name
  const char *task_name = pcTaskGetName(task_handle);

  while(1)
  {
    Serial.print("Running  : ");
    Serial.println(task_name);
    Serial.print("Blocking : ");
    Serial.println(task_name); 
    vTaskDelay(pdMS_TO_TICKS(TASK3_DELAY));
  }
     
}