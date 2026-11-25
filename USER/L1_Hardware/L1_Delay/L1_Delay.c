/**
 ******************************************************************************
 * @file           : L1_Delay.c
 * @brief          : Template of .c file
 * @author         : Rex Cheng (bloodmooon1993@gmail.com)
 * @copyright      : Copyright (c) 2023
 ******************************************************************************
 */

/****************************************************************************************************************
 * Includes
 ****************************************************************************************************************/
#include "L1_Delay.h"
#include "FreeRTOS.h"
#include "task.h"

/****************************************************************************************************************
 * Private typedef
 ****************************************************************************************************************/

/****************************************************************************************************************
 * Private define
 ****************************************************************************************************************/

/****************************************************************************************************************
 * Private macro
 ****************************************************************************************************************/

/* 直接定義 Cortex-M3 標準 DWT 與 CoreDebug 暫存器位址 (適用舊版 SPL CMSIS) */
#define DEM_CR (*(volatile uint32_t *)0xE000EDFCU)
#define DWT_CTRL_REG (*(volatile uint32_t *)0xE0001000U)
#define DWT_CYCCNT_REG (*(volatile uint32_t *)0xE0001004U)

#define DEM_CR_TRCENA (1UL << 24)
#define DWT_CTRL_CYCCNTENA (1UL << 0)

/****************************************************************************************************************
 * Private variables
 ****************************************************************************************************************/

/****************************************************************************************************************
 * Private function prototypes
 ****************************************************************************************************************/

/****************************************************************************************************************
 * Private functions
 ****************************************************************************************************************/

/* 初始化 HSE 72MHz (假設 PD0/PD1 外接 8MHz 晶振；若為 12MHz 請將 PLLMUL 改為 RCC_PLLMul_6) */
ErrorStatus SystemClock_HSE_72MHz_Init(void)
{
	RCC_DeInit();

	/* 1. 開啟外部高速晶振 HSE (PD0/PD1 預設即為 OSC_IN/OSC_OUT，切勿啟用 PD01 Remap) */
	RCC_HSEConfig(RCC_HSE_ON);
	if (RCC_WaitForHSEStartUp() != SUCCESS)
	{
		return ERROR; // HSE 起振失敗 (檢查晶振與負載電容)
	}

	/* 2. 開啟 Flash 預取緩衝並設定 2 Wait States (48MHz < SYSCLK <= 72MHz 必須設為 2 WS) */
	FLASH_PrefetchBufferCmd(FLASH_PrefetchBuffer_Enable);
	FLASH_SetLatency(FLASH_Latency_2);

	/* 3. 設定匯流排分頻: HCLK=72MHz, PCLK2=72MHz, PCLK1=36MHz (APB1 上限 36MHz) */
	RCC_HCLKConfig(RCC_SYSCLK_Div1);
	RCC_PCLK2Config(RCC_HCLK_Div1);
	RCC_PCLK1Config(RCC_HCLK_Div2);
	RCC_ADCCLKConfig(RCC_PCLK2_Div6); // 12MHz (ADC 上限 14MHz)

	/* 4. 設定 PLL: HSE 不分頻 (8MHz) * 9 = 72MHz */
	RCC_PLLConfig(RCC_PLLSource_HSE_Div1, RCC_PLLMul_9);
	RCC_PLLCmd(ENABLE);
	while (RCC_GetFlagStatus(RCC_FLAG_PLLRDY) == RESET)
		;

	/* 5. 切換系統主時鐘至 PLL */
	RCC_SYSCLKConfig(RCC_SYSCLKSource_PLLCLK);
	while (RCC_GetSYSCLKSource() != 0x08)
		;

	SystemCoreClockUpdate();
	return SUCCESS;
}

/* 配置 PA8 (MCO) 輸出 8MHz HSE 石英時脈給 OV7670 XCLK */
void MCO_XCLK_Init(void)
{
	GPIO_InitTypeDef GPIO_InitStructure;

	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA | RCC_APB2Periph_AFIO, ENABLE);

	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_8;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(GPIOA, &GPIO_InitStructure);

	RCC_MCOConfig(RCC_MCO_HSE); // 輸出 8MHz 方波 (石英晶振精度高於 HSI)
}

/* 裸機精準延時 (72MHz 下使用) */
void DWT_Delay_Init(void)
{
	DEM_CR |= DEM_CR_TRCENA;			// 開啟 Trace/DWT 模組
	DWT_CYCCNT_REG = 0;					// 清空週期計數器
	DWT_CTRL_REG |= DWT_CTRL_CYCCNTENA; // 啟動 CYCCNT 計數
}

void Delay_us(uint32_t us)
{
	uint32_t start = DWT_CYCCNT_REG;
	uint32_t ticks = us * (SystemCoreClock / 1000000U);
	while ((uint32_t)(DWT_CYCCNT_REG - start) < ticks)
		;
}

void Delay_ms(uint32_t ms)
{
	while (ms--)
	{
		Delay_us(1000);
	}
}
/* Old Codes*/

#if 0 

static u8 fac_us = 0;  // us延时倍乘数
static u16 fac_ms = 0; // ms延时倍乘数

void Delay_Init(void)
{
	// Init System Clock
	SysTick_CLKSourceConfig(SysTick_CLKSource_HCLK_Div8); // 选择外部时钟  HCLK/8

	// Define factor of us/ms
	fac_us = SystemCoreClock / 8000000; // 为系统时钟的1/8
	fac_ms = (u16)fac_us * 1000;		// 代表每个ms需要的systick时钟数
}

void Delay_ms(uint16 uMS)
{
	u32 temp;
	SysTick->LOAD = (u32)uMS * fac_ms;		  // 时间加载(SysTick->LOAD为24bit)
	SysTick->VAL = 0x00;					  // 清空计数器
	SysTick->CTRL |= SysTick_CTRL_ENABLE_Msk; // 开始倒数
	do
	{
		temp = SysTick->CTRL;
	} while (temp & 0x01 && !(temp & (1 << 16))); // 等待时间到达
	SysTick->CTRL &= ~SysTick_CTRL_ENABLE_Msk; // 关闭计数器
	SysTick->VAL = 0X00;					   // 清空计数器
}

void Delay_us(uint32 uUS)
{
	u32 temp;
	SysTick->LOAD = uUS * fac_us;			  // 时间加载
	SysTick->VAL = 0x00;					  // 清空计数器
	SysTick->CTRL |= SysTick_CTRL_ENABLE_Msk; // 开始倒数
	do
	{
		temp = SysTick->CTRL;
	} while (temp & 0x01 && !(temp & (1 << 16))); // 等待时间到达   	   temp&0x01  0x01表示是否使能 防止无意关闭时出现死循环
	SysTick->CTRL &= ~SysTick_CTRL_ENABLE_Msk; // 关闭计数器
	SysTick->VAL = 0X00;					   // 清空计数器
}

// 此函數是 FreeRTOS中的vTaskDelay, 會導致任務切換
void xDelay_ms(uint32 uMS)
{
	if (xTaskGetSchedulerState() != taskSCHEDULER_NOT_STARTED) // OS is Running
	{
		if (uMS >= fac_ms)
		{
			vTaskDelay(uMS * fac_ms); // Delay FreeRTOS Task
		}
	}

	// Normal Delay;
	Delay_ms(uMS);
}

#endif

/****************************************************************************************************************
 * End of file
 ****************************************************************************************************************/