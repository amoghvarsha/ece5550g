#include <Arduino_FreeRTOS.h>

// define two tasks for Blink & AnalogRead
void TaskBlink( void *pvParameters );
void TaskAnalogRead( void *pvParameters );

// the setup function runs once when you press reset or power the board
void setup() {
  
  // initialize serial communication at 9600 bits per second:
  Serial.begin(230400);
  
  while (!Serial) {
    ; // wait for serial port to connect. Needed for native USB, on LEONARDO, MICRO, YUN, and other 32u4 based boards.
  }

  // Now set up two tasks to run independently.
  xTaskCreate(
    TaskBlink
    ,  "Blink"   // A name just for humans
    ,  128  // This stack size can be checked & adjusted by reading the Stack Highwater
    ,  NULL
    ,  2  // Priority, with 3 (configMAX_PRIORITIES - 1) being the highest, and 0 being the lowest.
    ,  NULL );

  xTaskCreate(
    TaskAnalogRead
    ,  "AnalogRead"
    ,  128  // Stack size
    ,  NULL
    ,  1  // Priority
    ,  NULL );
       
  // Now the task scheduler, which takes over control of scheduling individual tasks, is automatically started.
}

void loop()
{
  // Empty. Things are done in Tasks.
}

/*--------------------------------------------------*/
/*---------------------- Tasks ---------------------*/
/*--------------------------------------------------*/

void TaskBlink(void *pvParameters)  // This is a task.
{
  (void) pvParameters;

  bool led_state = LOW;
  
  const TaskHandle_t task_handle = xTaskGetCurrentTaskHandle(); 
  const char *task_name = pcTaskGetName(task_handle);

  // initialize digital LED_BUILTIN on pin 13 as an output.
  pinMode(LED_BUILTIN, OUTPUT);

  for (;;) // A Task shall never return or exit.
  {
    // Print the task name
    Serial.println(task_name);

    // Toggle the LED
    digitalWrite(LED_BUILTIN, led_state);
    led_state = !led_state;
    
    // Wait for 500 ms
    vTaskDelay(pdMS_TO_TICKS(500) ); // wait for one second
  }
}

void TaskAnalogRead(void *pvParameters)  // This is a task.
{
  (void) pvParameters;

  const TaskHandle_t task_handle = xTaskGetCurrentTaskHandle(); 
  const char *task_name = pcTaskGetName(task_handle);

  for (;;)
  {
    // Print the task name
    Serial.print(task_name);
    Serial.print(" : ");

    // read the input on analog pin 0:
    int sensorValue = analogRead(A0);
    
    // print out the value you read:
    Serial.println(sensorValue);
    
    // Wait for 100 ms
    vTaskDelay(pdMS_TO_TICKS(100));  
  }
}
