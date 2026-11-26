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
#include "semphr.h"
#include "queue.h"

/* Library includes. */
#include "stm32f10x_it.h"
#include "stm32f10x.h"

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

typedef enum
{
  SD_CMD_SAVE_LOG = 1,
  SD_CMD_SNAPSHOT = 2
} SD_Cmd_t;

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

/* FreeRTOS 同步物件 */
SemaphoreHandle_t xSemVSync = NULL;
SemaphoreHandle_t xMutexDisplay = NULL;
QueueHandle_t xQueueSDCmd = NULL;

/****************************************************************************************************************
 * Private function prototypes
 ****************************************************************************************************************/
void Init_all(void);
void task_Start(void *pvParameters); // 任务函数

extern ErrorStatus SystemClock_HSE_72MHz_Init(void);
extern void MCO_XCLK_Init(void);
extern void DWT_Delay_Init(void);
extern void TFTLCD_Init(void);
extern uint8_t OV7670_Init(void);
extern uint8_t SD_Init(void);
extern void OV7670_CaptureFrameToLCD(void);
extern volatile uint32_t g_frameCount;
static void USART1_Debug_Init(uint32_t baudrate);
void vTaskCameraDisplay(void *pvParameters);
void vTaskSDStorage(void *pvParameters);
void vTaskSystemMonitor(void *pvParameters);

/****************************************************************************************************************
 * Private functions
 ****************************************************************************************************************/

/* Task 分配表

任務配置表
| Task 名稱            | 優先權                          |  Stack 大小           |               職責與同步機制
| vTaskCameraDisplay   | 最高  (tskIDLE_PRIORITY + 4)   | 256 words (1 KB)      |  等待 xSemVSync 信號量，擷取影格直傳 TFT；完成後透過 vTaskDelay(2) 釋出 CPU 時間片，避免低優先權任務饑餓
| vTaskSDStorage       | 中 (tskIDLE_PRIORITY + 2)      | 512 words (2 KB)      |   處理 SD 初始化與 xQueueSDCmd 佇列命令（寫入 Log 或截圖存檔），預留較大 Stack 供 FatFs 使用
| vTaskSystemMonitor   | 低 (tskIDLE_PRIORITY + 1)      | 256 words (1 KB)      | 每秒經由 USART1（PA9/PA10, 115200）輸出目前 FPS 與剩餘 FreeRTOS Heap 大小

*/

/**
 * @brief  The application entry point.
 * @retval int
 */

int main(void)
{
  /* 1. 系統時鐘 (HSE 72MHz)、DWT 延時與 NVIC 中斷分組 (FreeRTOS 必須為 Group 4) */
  if (SystemClock_HSE_72MHz_Init() != SUCCESS)
  {
    while (1)
      ; // 外部晶振啟動異常，停機除錯
  }
  DWT_Delay_Init();
  NVIC_PriorityGroupConfig(NVIC_PriorityGroup_4);

  /* 2. 開啟 Debug UART1 與 MCO 8MHz XCLK (OV7670 初始化前必須先送出 XCLK) */
  USART1_Debug_Init(115200);
  MCO_XCLK_Init();

  /* 3. 初始化 TFT LCD 與 OV7670 */
  TFTLCD_Init();
  /* For LCD Test */
  LCD_FillColor(0xF800); // 整片刷成純紅色 (RGB565)
  // while (1);                      // 停在這裡先不進 OV7670 與 FreeRTOS
  /* Test End */
  uint8_t camErr = OV7670_Init();
  printf("[CAM] OV7670 Init %s (code=%d)\r\n", (camErr == 0) ? "OK" : "FAIL", camErr);

  /* 4. 建立 FreeRTOS 同步物件 */
  xSemVSync = xSemaphoreCreateBinary();
  xMutexDisplay = xSemaphoreCreateMutex();
  xQueueSDCmd = xQueueCreate(4, sizeof(SD_Cmd_t));

  /* 5. 建立 Tasks */

  xTaskCreate(vTaskCameraDisplay, "CamDisp", 256, NULL, tskIDLE_PRIORITY + 4, NULL);
  xTaskCreate(vTaskSDStorage, "SDStore", 512, NULL, tskIDLE_PRIORITY + 2, NULL);
  xTaskCreate(vTaskSystemMonitor, "SysMon", 256, NULL, tskIDLE_PRIORITY + 1, NULL);

  /* 6. 啟動排程器 */
  vTaskStartScheduler();

  while (1)
    ;
}

/* Debug 串口 USART1 (PA9 TX / PA10 RX, APB2 = 72MHz) */
static void USART1_Debug_Init(uint32_t baudrate)
{
  GPIO_InitTypeDef GPIO_InitStructure;
  USART_InitTypeDef USART_InitStructure;

  RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA | RCC_APB2Periph_USART1, ENABLE);

  GPIO_InitStructure.GPIO_Pin = GPIO_Pin_9;
  GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
  GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
  GPIO_Init(GPIOA, &GPIO_InitStructure);

  GPIO_InitStructure.GPIO_Pin = GPIO_Pin_10;
  GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;
  GPIO_Init(GPIOA, &GPIO_InitStructure);

  USART_InitStructure.USART_BaudRate = baudrate;
  USART_InitStructure.USART_WordLength = USART_WordLength_8b;
  USART_InitStructure.USART_StopBits = USART_StopBits_1;
  USART_InitStructure.USART_Parity = USART_Parity_No;
  USART_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
  USART_InitStructure.USART_Mode = USART_Mode_Rx | USART_Mode_Tx;
  USART_Init(USART1, &USART_InitStructure);
  USART_Cmd(USART1, ENABLE);
}

/* Task 1: Camera 取像直傳 TFT LCD (最高優先權) */
void vTaskCameraDisplay(void *pvParameters)
{
  (void)pvParameters;
  xSemaphoreTake(xSemVSync, 0);

  for (;;)
  {
    if (xSemaphoreTake(xSemVSync, pdMS_TO_TICKS(500)) == pdTRUE)
    {
      if (xSemaphoreTake(xMutexDisplay, pdMS_TO_TICKS(10)) == pdTRUE)
      {
        /* 擷取期間暫時遮蔽 EXTI2，避免重複中斷 */
        EXTI->IMR &= ~EXTI_Line2;

        OV7670_CaptureFrameToLCD();

        EXTI_ClearITPendingBit(EXTI_Line2);
        EXTI->IMR |= EXTI_Line2;

        xSemaphoreGive(xMutexDisplay);
      }
      /* 主動讓出 2ms 給中低優先權任務執行 */
      vTaskDelay(pdMS_TO_TICKS(2));
    }
  }
}

/* Task 2: SD 卡讀寫任務 (中優先權) */
void vTaskSDStorage(void *pvParameters)
{
  (void)pvParameters;
  SD_Cmd_t cmd;

  uint8_t sdStatus = SD_Init();
  printf("[SD] Init %s (code=%d)\r\n", (sdStatus == 0) ? "OK" : "FAIL", sdStatus);

  for (;;)
  {
    if (xQueueReceive(xQueueSDCmd, &cmd, portMAX_DELAY) == pdTRUE)
    {
      if (sdStatus != 0)
        continue;

      if (cmd == SD_CMD_SAVE_LOG)
      {
        printf("[SD] Writing log...\r\n");
      }
      else if (cmd == SD_CMD_SNAPSHOT)
      {
        printf("[SD] Snapshot requested\r\n");
      }
    }
  }
}

/* Task 3: 系統狀態監控 (低優先權) */
void vTaskSystemMonitor(void *pvParameters)
{
  (void)pvParameters;
  uint32_t lastFrames = 0;

  for (;;)
  {
    vTaskDelay(pdMS_TO_TICKS(1000));

    uint32_t curFrames = g_frameCount;
    uint32_t fps = curFrames - lastFrames;
    lastFrames = curFrames;

    printf("[SYS] 72MHz HSE | FPS: %lu | FreeHeap: %u bytes\r\n",
           (unsigned long)fps,
           (unsigned int)xPortGetFreeHeapSize());
  }
}

/* Old Codes */
#if 0

void Init_all(void)
{
  NVIC_PriorityGroupConfig(NVIC_PriorityGroup_2); // 设置NVIC中断分组2:2位抢占优先级，2位响应优先级
  Delay_Init();
  LED2_Init();
  TIM_All_Init();
  SD_Init();
  TFTLCD_Init();
  OV7670_Init();

#if 0  
  USART_all_Init();
  Bottom_Init();
  SPI_Flash_Init();
  AT24CXX_Init();
#endif
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

  xTaskCreate(vTaskCameraDisplay, "CamDisp", 256, NULL, tskIDLE_PRIORITY + 4, NULL);
  xTaskCreate(vTaskSDStorage, "SDStore", 512, NULL, tskIDLE_PRIORITY + 2, NULL);
  xTaskCreate(vTaskSystemMonitor, "SysMon", 256, NULL, tskIDLE_PRIORITY + 1, NULL);

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

#endif

/****************************************************************************************************************
 * End of file
 ****************************************************************************************************************/
