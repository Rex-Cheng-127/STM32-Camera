/**
 ******************************************************************************
 * @file           : L1_TFTLCD.c
 * @brief          : TFT LCD Module
 * @author         : Rex Cheng (bloodmooon1993@gmail.com)
 * @copyright      : Copyright (c) 2023
 ******************************************************************************
 */

/****************************************************************************************************************
 * Includes
 ****************************************************************************************************************/
#include "L1_TFTLCD.h"

/****************************************************************************************************************
 * Private typedef
 ****************************************************************************************************************/

/****************************************************************************************************************
 * Private define
 ****************************************************************************************************************/

#define LCD_RST_LOW() (GPIOB->BRR = GPIO_Pin_8)
#define LCD_RST_HIGH() (GPIOB->BSRR = GPIO_Pin_8)
#define LCD_CS_LOW() (GPIOB->BRR = GPIO_Pin_9)
#define LCD_CS_HIGH() (GPIOB->BSRR = GPIO_Pin_9)
#define LCD_RS_CMD() (GPIOD->BRR = GPIO_Pin_2)
#define LCD_RS_DATA() (GPIOD->BSRR = GPIO_Pin_2)
#define LCD_WR_LOW() (GPIOC->BRR = GPIO_Pin_10)
#define LCD_WR_HIGH() (GPIOC->BSRR = GPIO_Pin_10)

/****************************************************************************************************************
 * Private macro
 ****************************************************************************************************************/

/****************************************************************************************************************
 * Private variables
 ****************************************************************************************************************/

/****************************************************************************************************************
 * Private function prototypes
 ****************************************************************************************************************/

/****************************************************************************************************************
 * Private functions
 ****************************************************************************************************************/

/*
    Req & Design:
        依據規劃腳位（D0~D7: PB0~PB7、RST: PB8、CS: PB9、RS: PD2、WR: PC10、RD: 3.3V）實作。
        在 72 MHz 下單週期僅 13.89 ns，因此在 LCD_WriteBus8() 的 WR 低電位期間補上 __NOP() 以確保滿足 ILI9341 的寫入脈寬規範，
        並透過 GPIOB->BSRR 僅操作低 8 位元，避免干擾 PB12~PB15 的 SPI2 SD 卡腳位。

    Note:
        ILI9341 的 0x3A（COLMOD）、0xB1（FRMCTR1，預設 70 Hz 對應 0x00, 0x1B）與 0x2C 連續記憶體寫入機制均遵循原廠資料手冊定義
        ref: ILI9341_DS_V1.09_20110315.pdf
*/

void TFTLCD_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB | RCC_APB2Periph_GPIOC |
                               RCC_APB2Periph_GPIOD | RCC_APB2Periph_AFIO,
                           ENABLE);

    /* 關閉 JTAG 釋放 PB3/PB4 給 LCD_D3/D4，保留 SWD (PA13/PA14) 除錯 */
    GPIO_PinRemapConfig(GPIO_Remap_SWJ_JTAGDisable, ENABLE);

    /* PB0~PB7 (D0~D7), PB8 (RST), PB9 (CS) */
    GPIO_InitStructure.GPIO_Pin = 0x03FF;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOB, &GPIO_InitStructure);

    /* PC10 (LCD_WR) */
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_10;
    GPIO_Init(GPIOC, &GPIO_InitStructure);

    /* PD2 (LCD_RS) */
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_2;
    GPIO_Init(GPIOD, &GPIO_InitStructure);

    LCD_CS_HIGH();
    LCD_WR_HIGH();

    /* 硬體重置 */
    LCD_RST_HIGH();
    Delay_ms(10);
    LCD_RST_LOW();
    Delay_ms(20);
    LCD_RST_HIGH();
    Delay_ms(120);

    LCD_CS_LOW();

    LCD_WriteCmd(0x01); // Software Reset
    Delay_ms(50);

    /* 電源與驅動暫存器配置 */
    LCD_WriteCmd(0xCF);
    LCD_WriteData(0x00);
    LCD_WriteData(0xC1);
    LCD_WriteData(0x30);
    LCD_WriteCmd(0xED);
    LCD_WriteData(0x64);
    LCD_WriteData(0x03);
    LCD_WriteData(0x12);
    LCD_WriteData(0x81);
    LCD_WriteCmd(0xE8);
    LCD_WriteData(0x85);
    LCD_WriteData(0x10);
    LCD_WriteData(0x7A);
    LCD_WriteCmd(0xCB);
    LCD_WriteData(0x39);
    LCD_WriteData(0x2C);
    LCD_WriteData(0x00);
    LCD_WriteData(0x34);
    LCD_WriteData(0x02);
    LCD_WriteCmd(0xF7);
    LCD_WriteData(0x20);
    LCD_WriteCmd(0xEA);
    LCD_WriteData(0x00);
    LCD_WriteData(0x00);

    LCD_WriteCmd(0xC0);
    LCD_WriteData(0x1B);
    LCD_WriteCmd(0xC1);
    LCD_WriteData(0x01);
    LCD_WriteCmd(0xC5);
    LCD_WriteData(0x30);
    LCD_WriteData(0x30);
    LCD_WriteCmd(0xC7);
    LCD_WriteData(0xB7);

    /* 0x28: 橫向 320x240 + BGR 模式 (若紅藍反色請改 0x20) */
    LCD_WriteCmd(0x36);
    LCD_WriteData(0x28);

    /* COLMOD: 16-bit RGB565，直接對應 OV7670 輸出格式 */
    LCD_WriteCmd(0x3A);
    LCD_WriteData(0x55);

    LCD_WriteCmd(0xB1);
    LCD_WriteData(0x00);
    LCD_WriteData(0x1B); // Frame Rate 70Hz (預設)
    LCD_WriteCmd(0xB6);
    LCD_WriteData(0x0A);
    LCD_WriteData(0xA2);

    LCD_WriteCmd(0x11); // Sleep Out
    Delay_ms(120);
    LCD_WriteCmd(0x29); // Display ON

    LCD_CS_HIGH();
}

/* 原子操作寫入 PB0~PB7，不影響 PB8~PB15 (CS/RST/SPI2) */
static inline void LCD_WriteBus8(uint8_t data)
{
    GPIOB->BSRR = ((uint32_t)(~data & 0x00FF) << 16) | (data & 0x00FF);
    LCD_WR_LOW();
    __NOP(); // 72MHz 下確保 WR 低電位脈寬 >= 15ns
    LCD_WR_HIGH();
}

void LCD_WriteCmd(uint8_t cmd)
{
    LCD_RS_CMD();
    LCD_WriteBus8(cmd);
}

void LCD_WriteData(uint8_t data)
{
    LCD_RS_DATA();
    LCD_WriteBus8(data);
}

/* 設定 320x240 繪圖視窗並維持 CS Low + DATA 模式等待像素直傳 */
void LCD_SetWindow(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1)
{
    LCD_CS_LOW();

    LCD_WriteCmd(0x2A); // Column Address Set (CASET)
    LCD_WriteData(x0 >> 8);
    LCD_WriteData(x0 & 0xFF);
    LCD_WriteData(x1 >> 8);
    LCD_WriteData(x1 & 0xFF);

    LCD_WriteCmd(0x2B); // Page Address Set (PASET)
    LCD_WriteData(y0 >> 8);
    LCD_WriteData(y0 & 0xFF);
    LCD_WriteData(y1 >> 8);
    LCD_WriteData(y1 & 0xFF);

    LCD_WriteCmd(0x2C); // Memory Write (RAMWR)
    LCD_RS_DATA();
}

/* Fill color at Monitor For Test */
void LCD_FillColor(uint16_t color)
{
    uint32_t total = 320UL * 240UL;
    uint8_t hi = (uint8_t)(color >> 8);
    uint8_t lo = (uint8_t)(color & 0xFF);

    LCD_SetWindow(0, 0, 319, 239);
    while (total--)
    {
        LCD_WriteData(hi);
        LCD_WriteData(lo);
    }
    LCD_CS_HIGH();
}

/****************************************************************************************************************
 * End of file
 ****************************************************************************************************************/