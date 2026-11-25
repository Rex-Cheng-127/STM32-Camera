/**
 ******************************************************************************
 * @file           : L1_CMOS.c
 * @brief          : CMOS Camera module
 * @author         : Rex Cheng (bloodmooon1993@gmail.com)
 * @copyright      : Copyright (c) 2023
 ******************************************************************************
 */

/****************************************************************************************************************
 * Includes
 ****************************************************************************************************************/
#include "L1_CMOS.h"

/****************************************************************************************************************
 * Private typedef
 ****************************************************************************************************************/

/****************************************************************************************************************
 * Private define
 ****************************************************************************************************************/

#define OV7670_ADDR_WRITE 0x42
#define OV7670_ADDR_READ 0x43

#define SCCB_SCL_H() (GPIOC->BSRR = GPIO_Pin_12)
#define SCCB_SCL_L() (GPIOC->BRR = GPIO_Pin_12)
#define SCCB_SDA_H() (GPIOC->BSRR = GPIO_Pin_11)
#define SCCB_SDA_L() (GPIOC->BRR = GPIO_Pin_11)
#define SCCB_SDA_READ() ((GPIOC->IDR & GPIO_Pin_11) != 0)

/****************************************************************************************************************
 * Private macro
 ****************************************************************************************************************/

/****************************************************************************************************************
 * Private variables
 ****************************************************************************************************************/

/* 無 FIFO 專用的降頻 QVGA (320x240) RGB565 暫存器設定表 */
static const uint8_t ov7670_qvga_rgb565_regs[][2] = {
    {0x12, 0x14}, // COM7: QVGA (320x240) + RGB 輸出
    {0x11, 0x03}, // CLKRC: 輸入時脈 4 分頻 (降速關鍵，72MHz CPU 採樣裕度極佳)
    {0x6B, 0x0A}, // DBLV: Bypass PLL
    {0x3E, 0x19}, // COM14: 啟用 PCLK 手動分頻 /2
    {0x40, 0xD0}, // COM15: 全範圍輸出 + RGB565 格式
    {0x8C, 0x00}, // RGB444: 關閉
    {0x3A, 0x04}, // TSLB: 正常影像輸出順序
    {0x0C, 0x04}, // COM3: 啟用 DCW 縮放
    {0x70, 0x3A}, // SCALING_XSC
    {0x71, 0x35}, // SCALING_YSC (除錯測試條可改 0xB5)
    {0x72, 0x11}, // SCALING_DCWCTR: 水平/垂直 2 倍下採樣
    {0x73, 0xF1}, // SCALING_PCLK_DIV: DSP PCLK /2
    {0xA2, 0x02}, // SCALING_PCLK_DELAY
    {0x15, 0x20}, // COM10: 水平空白期間 PCLK 不翻轉

    /* QVGA 視窗裁切參數 */
    {0x17, 0x16}, // HSTART
    {0x18, 0x04}, // HSTOP
    {0x32, 0x80}, // HREF
    {0x19, 0x02}, // VSTRT
    {0x1A, 0x7A}, // VSTOP
    {0x03, 0x0A}, // VREF

    /* 色彩矩陣與自動曝光/增益/白平衡 */
    {0x4F, 0xB3},
    {0x50, 0xB3},
    {0x51, 0x00},
    {0x52, 0x3D},
    {0x53, 0xA7},
    {0x54, 0xE4},
    {0x58, 0x9E}, // MTXS
    {0x13, 0xE7}, // COM8: 啟用快速 AGC/AEC/AWB
    {0x1E, 0x01}, // MVFP: 視鏡頭安裝方向調整翻轉
};

/****************************************************************************************************************
 * Private function prototypes
 ****************************************************************************************************************/

static void SCCB_Start(void);
static void SCCB_Stop(void);
static uint8_t SCCB_WriteByte(uint8_t data);
static uint8_t SCCB_ReadByte(void);
uint8_t OV7670_WriteReg(uint8_t reg, uint8_t val);
uint8_t OV7670_ReadReg(uint8_t reg);

/****************************************************************************************************************
 * Private functions
 ****************************************************************************************************************/

/*
    * Req and Design:
        上述降頻參數（0x12=0x14、0x11=0x03、0x3E=0x19、0x40=0xD0）完全承襲原規劃，且支援 RGB565 直接輸出至 ILI9341。
*/

static void SCCB_Start(void)
{
    SCCB_SDA_H();
    SCCB_SCL_H();
    Delay_us(5);
    SCCB_SDA_L();
    Delay_us(5);
    SCCB_SCL_L();
}

static void SCCB_Stop(void)
{
    SCCB_SDA_L();
    Delay_us(5);
    SCCB_SCL_H();
    Delay_us(5);
    SCCB_SDA_H();
    Delay_us(5);
}

static uint8_t SCCB_WriteByte(uint8_t data)
{
    uint8_t ack;
    for (uint8_t i = 0; i < 8; i++)
    {
        if (data & 0x80)
            SCCB_SDA_H();
        else
            SCCB_SDA_L();
        data <<= 1;
        Delay_us(5);
        SCCB_SCL_H();
        Delay_us(5);
        SCCB_SCL_L();
    }
    SCCB_SDA_H();
    Delay_us(5);
    SCCB_SCL_H();
    Delay_us(5);
    ack = SCCB_SDA_READ();
    SCCB_SCL_L();
    return ack; // 0 表示 ACK 成功
}

static uint8_t SCCB_ReadByte(void)
{
    uint8_t data = 0;
    SCCB_SDA_H(); // 釋放 SDA 線由 OV7670 驅動
    for (uint8_t i = 0; i < 8; i++)
    {
        data <<= 1;
        SCCB_SCL_H();
        Delay_us(5);
        if (SCCB_SDA_READ())
            data |= 0x01;
        SCCB_SCL_L();
        Delay_us(5);
    }
    /* NACK */
    SCCB_SDA_H();
    Delay_us(5);
    SCCB_SCL_H();
    Delay_us(5);
    SCCB_SCL_L();
    return data;
}

uint8_t OV7670_WriteReg(uint8_t reg, uint8_t val)
{
    uint8_t err = 0;
    SCCB_Start();
    if (SCCB_WriteByte(OV7670_ADDR_WRITE))
        err = 1;
    if (SCCB_WriteByte(reg))
        err = 1;
    if (SCCB_WriteByte(val))
        err = 1;
    SCCB_Stop();
    return err;
}

uint8_t OV7670_ReadReg(uint8_t reg)
{
    uint8_t val;
    SCCB_Start();
    SCCB_WriteByte(OV7670_ADDR_WRITE);
    SCCB_WriteByte(reg);
    SCCB_Stop();
    Delay_us(10);

    SCCB_Start();
    SCCB_WriteByte(OV7670_ADDR_READ);
    val = SCCB_ReadByte();
    SCCB_Stop();
    return val;
}

/* 返回 0 表示成功 */
uint8_t OV7670_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;
    EXTI_InitTypeDef EXTI_InitStructure;
    NVIC_InitTypeDef NVIC_InitStructure;

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA | RCC_APB2Periph_GPIOC |
                               RCC_APB2Periph_AFIO,
                           ENABLE);

    /* 1. PA0 (PCLK), PA1 (HREF), PA2 (VSYNC) 浮空輸入 */
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_0 | GPIO_Pin_1 | GPIO_Pin_2;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOA, &GPIO_InitStructure);

    /* 2. PA3 (RESET) 硬體重置 */
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_3;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
    GPIO_Init(GPIOA, &GPIO_InitStructure);
    GPIO_ResetBits(GPIOA, GPIO_Pin_3);
    Delay_ms(10);
    GPIO_SetBits(GPIOA, GPIO_Pin_3);
    Delay_ms(20);

    /* 3. PC0~PC7 (D0~D7) 浮空輸入 */
    GPIO_InitStructure.GPIO_Pin = 0x00FF;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;
    GPIO_Init(GPIOC, &GPIO_InitStructure);

    /* 4. PC11 (SIOD), PC12 (SIOC) 開漏輸出 */
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_11 | GPIO_Pin_12;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_OD;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOC, &GPIO_InitStructure);
    SCCB_SCL_H();
    SCCB_SDA_H();

    /* 5. 驗證 OV7670 PID (0x76) 與 VER (0x73) */
    if (OV7670_ReadReg(0x0A) != 0x76 || OV7670_ReadReg(0x0B) != 0x73)
    {
        return 1;
    }

    /* 6. 軟體重置並等待 50ms */
    OV7670_WriteReg(0x12, 0x80);
    Delay_ms(50);

    /* 7. 寫入降頻 QVGA RGB565 暫存器組 */
    uint32_t count = sizeof(ov7670_qvga_rgb565_regs) / 2;
    for (uint32_t i = 0; i < count; i++)
    {
        if (OV7670_WriteReg(ov7670_qvga_rgb565_regs[i][0],
                            ov7670_qvga_rgb565_regs[i][1]) != 0)
        {
            return 2;
        }
    }

    /* 8. 配置 PA2 (VSYNC) 觸發 EXTI2 下降沿中斷 */
    GPIO_EXTILineConfig(GPIO_PortSourceGPIOA, GPIO_PinSource2);

    EXTI_InitStructure.EXTI_Line = EXTI_Line2;
    EXTI_InitStructure.EXTI_Mode = EXTI_Mode_Interrupt;
    EXTI_InitStructure.EXTI_Trigger = EXTI_Trigger_Falling;
    EXTI_InitStructure.EXTI_LineCmd = ENABLE;
    EXTI_Init(&EXTI_InitStructure);

    /* NVIC 優先權設為 6 (必須 >= configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY = 5) */
    NVIC_InitStructure.NVIC_IRQChannel = EXTI2_IRQn;
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 6;
    NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0;
    NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&NVIC_InitStructure);

    return 0;
}

/****************************************************************************************************************
 * End of file
 ****************************************************************************************************************/