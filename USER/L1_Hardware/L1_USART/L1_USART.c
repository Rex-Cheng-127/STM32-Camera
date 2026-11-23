/**
 ******************************************************************************
 * @file           : L1_USART.c
 * @brief          : Control USART2
 * @author         : Rex Cheng (bloodmooon1993@gmail.com)
 * @copyright      : Copyright (c) 2023
 ******************************************************************************
 */

/****************************************************************************************************************
 * Includes
 ****************************************************************************************************************/
#include "L1_USART.h"

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
uint8 USART_RX_BUF[USART_REC_LEN]; /* Buffer fort USART Receive Status */
uint16 USART_RX_STA;               /* USART Receive Status */

/****************************************************************************************************************
 * Private function prototypes
 ****************************************************************************************************************/
void USART2_Init(uint32 uBaudrate);

/****************************************************************************************************************
 * Private functions
 ****************************************************************************************************************/

void USART_all_Init(void)
{
    USART2_Init(9600); // 串口初始化为9600
}

//////////////////////////////////////////////////////////////////
// 加入以下代码,支持printf函数,而不需要选择use MicroLIB
#if 1
#pragma import(__use_no_semihosting)
// 标准库需要的支持函数
struct __FILE
{
    int handle;
};

FILE __stdout;
// 定义_sys_exit()以避免使用半主机模式
_sys_exit(int x)
{
    x = x;
}
// 重定义fputc函数
int fputc(int ch, FILE *f)
{
    while (USART_GetFlagStatus(USART2, USART_FLAG_TC) == RESET)
        ;
    USART_SendData(USART2, (uint8_t)ch);
    return ch;
}

/*使用microLib的方法*/
/*
int fputc(int ch, FILE *f)
{
   USART_SendData(USART2, (uint8_t) ch);

   while (USART_GetFlagStatus(USART2, USART_FLAG_TC) == RESET) {}

   return ch;
}
int GetKey (void)  {

   while (!(USART2->SR & USART_FLAG_RXNE));

   return ((int)(USART2->DR & 0x1FF));
}
*/
#endif

#if EN_USART2_RX

/**
 * @brief Init UART
 *
 * @param uBaudrate Baudrate
 */
void USART2_Init(uint32 uBaudrate)
{
    // GPIO端口设置
    GPIO_InitTypeDef GPIO_InitStructure;   // IO口配置结构体
    USART_InitTypeDef USART_InitStructure; // 串口配置结构体
    NVIC_InitTypeDef NVIC_InitStructure;   // 中断配置结构体

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);  // 使能GPIOA时钟
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_USART2, ENABLE); // 使能USART2
    USART_DeInit(USART2);                                  // 复位串口1

    // USART2_TX   PA.2
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_2; // PA.2
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP; // 复用推挽输出	 PA.2选择复用状态才能进入串口模式
    GPIO_Init(GPIOA, &GPIO_InitStructure);          // 初始化PA2

    // USART2_RX	  PA.3
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_3;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING; // 浮空输入
    GPIO_Init(GPIOA, &GPIO_InitStructure);                // 初始化PA3

    // USART 初始化设置
    USART_InitStructure.USART_BaudRate = uBaudrate;                                 // 一般设置为9600;
    USART_InitStructure.USART_WordLength = USART_WordLength_8b;                     // 字长为8位数据格式
    USART_InitStructure.USART_StopBits = USART_StopBits_1;                          // 一个停止位
    USART_InitStructure.USART_Parity = USART_Parity_No;                             // 无奇偶校验位
    USART_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_None; // 无硬件数据流控制
    USART_InitStructure.USART_Mode = USART_Mode_Rx | USART_Mode_Tx;                 // 收发模式

    USART_Init(USART2, &USART_InitStructure); // 初始化串口
#if EN_USART2_RX                              // 如果使能了接收
    // Usart2 NVIC 配置
    NVIC_InitStructure.NVIC_IRQChannel = USART2_IRQn;
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 3; // 抢占优先级3
    NVIC_InitStructure.NVIC_IRQChannelSubPriority = 3;        // 子优先级3
    NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;           // IRQ通道使能
    NVIC_Init(&NVIC_InitStructure);                           // 根据指定的参数初始化VIC寄存器

    USART_ITConfig(USART2, USART_IT_RXNE, ENABLE); // 开启中断
#endif
    USART_Cmd(USART2, ENABLE); // 使能串口
}

// 这里 定义了一个16位数据USART_RX_STA 来表示 采集的数据长度  数据状态等 相当于一个寄存器
// USART_RX_STA     15		    14	         13-0
//				 接收完成	接收到0x0d	  接收的数据长度  没接收加1 表示多了一个字节
// USART_RX_STA=0 则为接收数据做准备

// 串口进入中断的前提是 数据的最后以回车为准  即  0x0d 0x0a
void USART2_IRQHandler(void) // 串口2中断响应程序		 其名字不能随便定义
{
    u8 Res;                                                // 当串口接收到数据  RXNE将被置1
    if (USART_GetITStatus(USART2, USART_IT_RXNE) != RESET) // 接收中断(接收到的数据必须是0x0d 0x0a结尾)
    {
        Res = USART_ReceiveData(USART2); //(USART2->DR);	//读取接收到的数据

        if ((USART_RX_STA & 0x8000) == 0) // 接收未完成
        {
            if (USART_RX_STA & 0x4000) // 接收到了0x0d
            {
                if (Res != 0x0a)
                    USART_RX_STA = 0; // 接收错误,重新开始
                else
                    USART_RX_STA |= 0x8000; // 接收完成了 			  //接收到回车的后字节  置位状态寄存器
            }
            else // 还没收到0X0D
            {
                if (Res == 0x0d)
                    USART_RX_STA |= 0x4000; // 接收到回车的前一字节  置位状态寄存器
                else
                {
                    USART_RX_BUF[USART_RX_STA & 0X3FFF] = Res; // 将接收的数据 存入数组中
                    USART_RX_STA++;                            // 长度+1 为下一次做准备
                    if (USART_RX_STA > (USART_REC_LEN - 1))
                        USART_RX_STA = 0; // 接收数据错误,重新开始接收
                }
            }
        }
    }
}

/**
 * @brief USART2 Communicate
 *
 */
void USART2_Com(void)
{
    u8 t;
    u8 len;
    u16 times = 0;

    while (1)
    {
        if (USART_RX_STA & 0x8000) // 判断是否接收完数据
        {
            len = USART_RX_STA & 0x3f; // 得到此次接收到的数据长度   前两位为状态位 所以要与去
            printf("\r\n Your Message:\r\n\r\n");
            for (t = 0; t < len; t++) // 通过长度进行循环
            {
                USART_SendData(USART2, USART_RX_BUF[t]); // 将数组采集好的数据 一次向串口1发送
                while (USART_GetFlagStatus(USART2, USART_FLAG_TC) != SET)
                    ; // 等待发送结束
            }
            printf("\r\n\r\n"); // 插入换行
            USART_RX_STA = 0;   // 清零 虚拟寄存器		  为一下次采集做准备
        }
        else
        {
            times++;
            if (times % 2000 == 0)
            {
                printf("\r\n STM32F103 USART2 Test! \r\n\r\n");
            }
            if (times % 200 == 0)
            {
                printf("Please Key Message, and Key Enter\n");
                // GPIO_SetBits(GPIOA, GPIO_Pin_1); // 给PA1置1 LED2
            }
            if (times % 400 == 0)
            {
                // GPIO_ResetBits(GPIOA, GPIO_Pin_1); // 给PA1置0
            }
            Delay_ms(6);
        }
    }
}

void task_USART(void *pvParameters)
{
    while (1)
    {
        printf("Executing Task: task_USART\n");
        // USART2_Com();
        vTaskDelay(1000);
    }
}

#endif
/****************************************************************************************************************
 * End of file
 ****************************************************************************************************************/