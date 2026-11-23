/**
 ******************************************************************************
 * @file           : L1_Timer.c
 * @brief          : Timer function with TIM of STM32
 * @author         : Rex Cheng (bloodmooon1993@gmail.com)
 * @copyright      : Copyright (c) 2023
 ******************************************************************************
 */

/****************************************************************************************************************
 * Includes
 ****************************************************************************************************************/
#include "L1_Timer.h"

/****************************************************************************************************************
 * Private typedef
 ****************************************************************************************************************/

/****************************************************************************************************************
 * Private define
 ****************************************************************************************************************/

/****************************************************************************************************************
 * Private macro
 ****************************************************************************************************************/

/****************************************************************************************************************
 * Private variables
 ****************************************************************************************************************/
uint8 uLED = 0; /* 1: LED On, 2 LED Off*/

/****************************************************************************************************************
 * Private function prototypes
 ****************************************************************************************************************/
void TIM2_Int_Init(u16 uPeriod, u16 uPreScale);

/****************************************************************************************************************
 * Private functions
 ****************************************************************************************************************/

void TIM_All_Init()
{
#if 0 // Close Interrupt
  //NVIC_PriorityGroupConfig(NVIC_PriorityGroup_2);
  TIM2_Int_Init(9999,7199); //20Khz的计数频率  1秒翻转一次
#endif
}

// 通用定时器2中断初始化
// 这里时钟选择为APB1的2倍，而APB1为36M	即定时器的主频为72M
// arr：自动重装值。
// uPreScale: 时钟预分频数
// 这里使用的是定时器2
void TIM2_Int_Init(u16 uPeriod, u16 uPreScale)
{
  TIM_TimeBaseInitTypeDef TIM_TimeBaseStructure; // 定时器配置结构体定义
  NVIC_InitTypeDef NVIC_InitStructure;           // 中断配置结构体定义

  RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM2, ENABLE); // 时钟使能

  // 定时器TIM3初始化
  TIM_TimeBaseStructure.TIM_Period = uPeriod;                 // 设置在下一个更新事件装入活动的自动重装载寄存器周期的值
  TIM_TimeBaseStructure.TIM_Prescaler = uPreScale;            // 设置用来作为TIMx时钟频率除数的预分频值
  TIM_TimeBaseStructure.TIM_CounterMode = TIM_CounterMode_Up; // TIM向上计数模式
  TIM_TimeBaseStructure.TIM_ClockDivision = TIM_CKD_DIV1;     // 设置时钟分割:TDTS = Tck_tim
  TIM_TimeBaseInit(TIM2, &TIM_TimeBaseStructure);             // 根据指定的参数初始化TIMx的时间基数单位

  TIM_ITConfig(TIM2, TIM_IT_Update, ENABLE); // 使能指定的TIM2中断,允许更新中断	TIM_IT_Update

  // 中断优先级NVIC设置
  NVIC_InitStructure.NVIC_IRQChannel = TIM2_IRQn;           // TIM2中断
  NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 3; // 先占优先级3级
  NVIC_InitStructure.NVIC_IRQChannelSubPriority = 3;        // 从优先级3级
  NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;           // IRQ通道被使能
  NVIC_Init(&NVIC_InitStructure);                           // 初始化NVIC寄存器

  TIM_Cmd(TIM2, ENABLE); // Enable TIM2
}

// 定时器2中断服务程序
void TIM2_IRQHandler(void) // TIM2中断
{
  if (TIM_GetITStatus(TIM2, TIM_IT_Update) != RESET) // 检查TIM2更新中断发生与否
  {
    TIM_ClearITPendingBit(TIM2, TIM_IT_Update); // 清除TIM2更新中断标志
    uLED = !uLED;
    if (uLED == 0)
    {
      LED2_Off();
    }
    else
    {
      LED2_On();
    }
  }
}

/****************************************************************************************************************
 * End of file
 ****************************************************************************************************************/