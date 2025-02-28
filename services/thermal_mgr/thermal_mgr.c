#include "thermal_mgr.h"
#include "errors.h"
#include "lm75bd.h"
#include "console.h"

#include <FreeRTOS.h>
#include <os_task.h>
#include <os_queue.h>

#include <string.h>

#define THERMAL_MGR_STACK_SIZE 256U

static TaskHandle_t thermalMgrTaskHandle;
static StaticTask_t thermalMgrTaskBuffer;
static StackType_t thermalMgrTaskStack[THERMAL_MGR_STACK_SIZE];

#define THERMAL_MGR_QUEUE_LENGTH 10U
#define THERMAL_MGR_QUEUE_ITEM_SIZE sizeof(thermal_mgr_event_t)

static QueueHandle_t thermalMgrQueueHandle;
static StaticQueue_t thermalMgrQueueBuffer;
static uint8_t thermalMgrQueueStorageArea[THERMAL_MGR_QUEUE_LENGTH * THERMAL_MGR_QUEUE_ITEM_SIZE];

static void thermalMgr(void *pvParameters);

void initThermalSystemManager(lm75bd_config_t *config) {
  memset(&thermalMgrTaskBuffer, 0, sizeof(thermalMgrTaskBuffer));
  memset(thermalMgrTaskStack, 0, sizeof(thermalMgrTaskStack));
  
  thermalMgrTaskHandle = xTaskCreateStatic(
    thermalMgr, "thermalMgr", THERMAL_MGR_STACK_SIZE,
    config, 1, thermalMgrTaskStack, &thermalMgrTaskBuffer);

  memset(&thermalMgrQueueBuffer, 0, sizeof(thermalMgrQueueBuffer));
  memset(thermalMgrQueueStorageArea, 0, sizeof(thermalMgrQueueStorageArea));

  thermalMgrQueueHandle = xQueueCreateStatic(
    THERMAL_MGR_QUEUE_LENGTH, THERMAL_MGR_QUEUE_ITEM_SIZE,
    thermalMgrQueueStorageArea, &thermalMgrQueueBuffer);

}

error_code_t thermalMgrSendEvent(thermal_mgr_event_t *event) {
  /* Send an event to the thermal manager queue */
  if (thermalMgrQueueHandle == NULL)
    return ERR_CODE_INVALID_STATE;

  if (event == NULL)
    return ERR_CODE_INVALID_ARG;

  if( xQueueSend(thermalMgrQueueHandle,
                ( void * ) event,
                ( TickType_t ) 0) == errQUEUE_FULL)
  {
    return ERR_CODE_QUEUE_FULL;
  }
  return ERR_CODE_SUCCESS;
}

void osHandlerLM75BD(void) {
  /* Implement this function */
  thermal_mgr_event_t eventCall;
  eventCall.type = THERMAL_MGR_EVENT_OS_INTERRUPT;
  
  error_code_t errEvent = thermalMgrSendEvent(&eventCall);
}

static void thermalMgr(void *pvParameters) {
  /* Implement this task */
  thermal_mgr_event_t eventIn;

  while (1) {
    if(xQueueReceive(thermalMgrQueueHandle,
                     &eventIn,
                     (TickType_t) 10000) == pdTRUE)
    {
      if (eventIn.type == THERMAL_MGR_EVENT_MEASURE_TEMP_CMD)
      {
        float tempC = 0;
        error_code_t errTemp = readTempLM75BD(LM75BD_OBC_I2C_ADDR, &tempC);
        if (errTemp != ERR_CODE_SUCCESS)
          printConsole("Error reading Temp in Thermal Manager");
        
        addTemperatureTelemetry(tempC);
      }

      else if (eventIn.type == THERMAL_MGR_EVENT_OS_INTERRUPT)
      {
        float tempC = 0;
        error_code_t errTemp = readTempLM75BD(LM75BD_OBC_I2C_ADDR, &tempC);

        if (errTemp != ERR_CODE_SUCCESS)
          printConsole("Error reading Temp while handling OS interrupt");

        if (tempC > LM75BD_DEFAULT_HYST_THRESH)
          overTemperatureDetected();        
        else
          safeOperatingConditions();

      }
    }
  }
}

void addTemperatureTelemetry(float tempC) {
  printConsole("Temperature telemetry: %f deg C\n", tempC);
}

void overTemperatureDetected(void) {
  printConsole("Over temperature detected!\n");
}

void safeOperatingConditions(void) { 
  printConsole("Returned to safe operating conditions!\n");
}
