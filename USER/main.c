/**
 ******************************************************************************
 * @file           : main.c
 * @brief          : Main program body
 * @author
 * @copyright
 ******************************************************************************
 */

/****************************************************************************************************************
 * Includes
 ****************************************************************************************************************/
/* Standard includes. */
#include <stdio.h>

/* Scheduler includes. */
#include "FreeRTOS.h"
#include "task.h"

/* Library includes. */
#include "stm32f10x_it.h"

/* User includes */
#include "L1_LED.h"
#include "L1_Bottom.h"
#include "L1_USART.h"
#include "L1_Delay.h"
#include "L1_Timer.h"
#include "L1_Flash.h"
#include "L1_EEPROM.h"
#include "L1_CMOS.h"
#include "L1_TFTLCD.h"
#include "L1_SD.h"
#include "L2_PushBottom.h"

/****************************************************************************************************************
 * Private typedef
 ****************************************************************************************************************/

/****************************************************************************************************************
 * Private define
 ****************************************************************************************************************/

#define START_STK_SIZE 128        // 任务堆栈大小
#define LifeLED2_STK_SIZE 50      // 任务堆栈大小
#define BottomMonitor_STK_SIZE 50 // 任务堆栈大小
#define BottomAction_STK_SIZE 50  // 任务堆栈大小
#define USART_STK_SIZE 50         // 任务堆栈大小
#define FLASH_STK_SIZE 100        // 任务堆栈大小

#define START_TASK_PRIO 1         // 任务优先级
#define BottomMonitor_TASK_PRIO 2 // 任务优先级
#define BottomAction_TASK_PRIO 3  // 任务优先级
#define USART_TASK_PRIO 4         // 任务优先级
#define FLASH_TASK_PRIO 5         // 任务优先级
#define LifeLED2_TASK_PRIO 6      // 任务优先级

/****************************************************************************************************************
 * Private macro
 ****************************************************************************************************************/

/****************************************************************************************************************
 * Private variables
 ****************************************************************************************************************/
TaskHandle_t Start_TaskHandler;         // 任务句柄
TaskHandle_t LifeLED2_TaskHandler;      // 任务句柄
TaskHandle_t BottomMonitor_TaskHandler; // 任务句柄
TaskHandle_t BottomAction_TaskHandler;  // 任务句柄
TaskHandle_t USART_TaskHandler;         // 任务句柄
TaskHandle_t FLASH_TaskHandler;         // 任务句柄

/****************************************************************************************************************
 * Private function prototypes
 ****************************************************************************************************************/
void Init_all(void);
void task_Start(void *pvParameters); // 任务函数

/****************************************************************************************************************
 * Private functions
 ****************************************************************************************************************/

/**
 * @brief  The application entry point.
 * @retval int
 */

int main(void)
{
  /* Init */
  Init_all();

  // Create Start Task
  xTaskCreate((TaskFunction_t)task_Start,          // 任务函数
              (const char *)"task_Start",          // 任务名称
              (uint16_t)START_STK_SIZE,            // 任务堆栈大小
              (void *)NULL,                        // 传递给任务函数的参数
              (UBaseType_t)START_TASK_PRIO,        // 任务优先级
              (TaskHandle_t *)&Start_TaskHandler); // 任务句柄

  vTaskStartScheduler(); // Start Scheduler

  return 0;
}

void Init_all(void)
{
  NVIC_PriorityGroupConfig(NVIC_PriorityGroup_2); // 设置NVIC中断分组2:2位抢占优先级，2位响应优先级
  LED2_Init();
  Bottom_Init();
  Delay_Init();
  USART_all_Init();
  TIM_All_Init();
  SPI_Flash_Init();
  AT24CXX_Init();
}

// Function of start task
void task_Start(void *pvParameters)
{

  taskENTER_CRITICAL(); // 进入临界区

  // Create led2_task
  xTaskCreate((TaskFunction_t)task_LifeLED2,
              (const char *)"task_LifeLED2",
              (uint16_t)LifeLED2_STK_SIZE,
              (void *)NULL,
              (UBaseType_t)LifeLED2_TASK_PRIO,
              (TaskHandle_t *)&LifeLED2_TaskHandler);
  // Create Task of bottom monitor
  xTaskCreate((TaskFunction_t)task_BottomMonitor,
              (const char *)"task_BottomMonitor",
              (uint16_t)BottomMonitor_STK_SIZE,
              (void *)NULL,
              (UBaseType_t)BottomMonitor_TASK_PRIO,
              (TaskHandle_t *)&BottomMonitor_TaskHandler);

  // Create Task of bottom action
  xTaskCreate((TaskFunction_t)task_BottomAction,
              (const char *)"task_BottomAction",
              (uint16_t)BottomAction_STK_SIZE,
              (void *)NULL,
              (UBaseType_t)BottomAction_TASK_PRIO,
              (TaskHandle_t *)&BottomAction_TaskHandler);

  // Create Task of USART communication
  xTaskCreate((TaskFunction_t)task_USART,
              (const char *)"task_USART",
              (uint16_t)USART_STK_SIZE,
              (void *)NULL,
              (UBaseType_t)USART_TASK_PRIO,
              (TaskHandle_t *)&USART_TaskHandler);

  // Create Task of FLASH Test
  xTaskCreate((TaskFunction_t)task_FLASH,
              (const char *)"task_FLASH",
              (uint16_t)FLASH_STK_SIZE,
              (void *)NULL,
              (UBaseType_t)FLASH_TASK_PRIO,
              (TaskHandle_t *)&FLASH_TaskHandler);

  vTaskDelete(Start_TaskHandler); // 删除开始任务

  taskEXIT_CRITICAL(); // 退出临界区
}

/****************************************************************************************************************
 * End of file
 ****************************************************************************************************************/
