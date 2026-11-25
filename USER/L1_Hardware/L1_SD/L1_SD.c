/**
 ******************************************************************************
 * @file           : L1_SD.c
 * @brief          : Micro SD Card Reader
 * @author         : Rex Cheng (bloodmooon1993@gmail.com)
 * @copyright      : Copyright (c) 2023
 ******************************************************************************
 */

/****************************************************************************************************************
 * Includes
 ****************************************************************************************************************/
#include "L1_SD.h"

/****************************************************************************************************************
 * Private typedef
 ****************************************************************************************************************/

typedef enum
{
    SD_TYPE_UNKNOWN = 0,
    SD_TYPE_V1,
    SD_TYPE_V2_SDSC,
    SD_TYPE_V2_SDHC
} SD_CardType_t;

/****************************************************************************************************************
 * Private define
 ****************************************************************************************************************/

/****************************************************************************************************************
 * Private macro
 ****************************************************************************************************************/

#define SD_CS_LOW() (GPIOB->BRR = GPIO_Pin_12)
#define SD_CS_HIGH() (GPIOB->BSRR = GPIO_Pin_12)

/****************************************************************************************************************
 * Private variables
 ****************************************************************************************************************/
static SD_CardType_t g_sdCardType = SD_TYPE_UNKNOWN;

/****************************************************************************************************************
 * Private function prototypes
 ****************************************************************************************************************/

static void SPI2_SetSpeed(uint16_t prescaler);

/****************************************************************************************************************
 * Private functions
 ****************************************************************************************************************/

/*
    Req & Design:
        在 72 MHz 系統下 PCLK1 為 36 MHz。
        SD 卡初始化階段使用 SPI_BaudRatePrescaler_128（36 MHz / 128 = 281.25 kHz，符合 ≤ 400 kHz 規範），
        初始化完成後切換至高速模式（Div2 = 18 MHz；若杜邦線較長或串接 1kΩ 電阻，建議改用 Div4 = 9 MHz）。
*/

/* 返回 0 表示 SD 卡初始化成功 */
uint8_t SD_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;
    SPI_InitTypeDef SPI_InitStructure;
    uint8_t r1, buf[4];
    uint16_t retry;

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_SPI2, ENABLE);

    /* PB13 (SCK), PB15 (MOSI) 復用推挽輸出 */
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_13 | GPIO_Pin_15;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOB, &GPIO_InitStructure);

    /* PB14 (MISO) 上拉輸入 */
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_14;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPU;
    GPIO_Init(GPIOB, &GPIO_InitStructure);

    /* PB12 (CS) 推挽輸出 */
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_12;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
    GPIO_Init(GPIOB, &GPIO_InitStructure);
    SD_CS_HIGH();

    /* SPI2 初始化：識別階段設為 36MHz / 128 = 281.25kHz (<= 400kHz) */
    SPI_InitStructure.SPI_Direction = SPI_Direction_2Lines_FullDuplex;
    SPI_InitStructure.SPI_Mode = SPI_Mode_Master;
    SPI_InitStructure.SPI_DataSize = SPI_DataSize_8b;
    SPI_InitStructure.SPI_CPOL = SPI_CPOL_Low; // Mode 0
    SPI_InitStructure.SPI_CPHA = SPI_CPHA_1Edge;
    SPI_InitStructure.SPI_NSS = SPI_NSS_Soft;
    SPI_InitStructure.SPI_BaudRatePrescaler = SPI_BaudRatePrescaler_128;
    SPI_InitStructure.SPI_FirstBit = SPI_FirstBit_MSB;
    SPI_InitStructure.SPI_CRCPolynomial = 7;
    SPI_Init(SPI2, &SPI_InitStructure);
    SPI_Cmd(SPI2, ENABLE);

    /* 1. CS 拉高，送出 80 個 Dummy Clocks (>= 74 clocks) */
    SD_CS_HIGH();
    for (uint8_t i = 0; i < 10; i++)
        SPI2_ReadWriteByte(0xFF);

    /* 2. CMD0 進入 SPI Idle 模式 */
    retry = 20;
    do
    {
        r1 = SD_SendCmd(0, 0, 0x95);
    } while ((r1 != 0x01) && --retry);
    if (r1 != 0x01)
    {
        SD_CS_HIGH();
        return 1;
    }

    /* 3. CMD8 檢查 SDv2.0 與工作電壓 */
    if (SD_SendCmd(8, 0x000001AA, 0x87) == 0x01)
    {
        for (uint8_t i = 0; i < 4; i++)
            buf[i] = SPI2_ReadWriteByte(0xFF);
        if (buf[2] == 0x01 && buf[3] == 0xAA)
        {
            retry = 0xFFF;
            do
            {
                SD_SendCmd(55, 0, 0x01);
                r1 = SD_SendCmd(41, 0x40000000, 0x01);
            } while (r1 != 0x00 && --retry);

            if (r1 == 0x00 && SD_SendCmd(58, 0, 0x01) == 0x00)
            {
                for (uint8_t i = 0; i < 4; i++)
                    buf[i] = SPI2_ReadWriteByte(0xFF);
                g_sdCardType = (buf[0] & 0x40) ? SD_TYPE_V2_SDHC : SD_TYPE_V2_SDSC;
            }
        }
    }
    else
    {
        /* SDv1.0 初始化流程 */
        retry = 0xFFF;
        do
        {
            SD_SendCmd(55, 0, 0x01);
            r1 = SD_SendCmd(41, 0, 0x01);
        } while (r1 != 0x00 && --retry);
        if (r1 == 0x00)
            g_sdCardType = SD_TYPE_V1;
    }

    SD_CS_HIGH();
    SPI2_ReadWriteByte(0xFF);

    if (g_sdCardType == SD_TYPE_UNKNOWN)
        return 2;

    /* 4. 切換高速傳輸：36MHz / 2 = 18MHz (長導線建議改 SPI_BaudRatePrescaler_4 = 9MHz) */
    SPI2_SetSpeed(SPI_BaudRatePrescaler_2);
    return 0;
}

static void SPI2_SetSpeed(uint16_t prescaler)
{
    SPI_Cmd(SPI2, DISABLE);
    SPI2->CR1 = (SPI2->CR1 & ~SPI_BaudRatePrescaler_256) | prescaler;
    SPI_Cmd(SPI2, ENABLE);
}

uint8_t SPI2_ReadWriteByte(uint8_t txData)
{
    uint32_t timeout = 0xFFFF;
    while (SPI_I2S_GetFlagStatus(SPI2, SPI_I2S_FLAG_TXE) == RESET && --timeout)
        ;
    SPI_I2S_SendData(SPI2, txData);

    timeout = 0xFFFF;
    while (SPI_I2S_GetFlagStatus(SPI2, SPI_I2S_FLAG_RXNE) == RESET && --timeout)
        ;
    return (uint8_t)SPI_I2S_ReceiveData(SPI2);
}

static uint8_t SD_SendCmd(uint8_t cmd, uint32_t arg, uint8_t crc)
{
    uint8_t r1, retry = 0x1F;

    SD_CS_HIGH();
    SPI2_ReadWriteByte(0xFF);
    SD_CS_LOW();

    SPI2_ReadWriteByte(cmd | 0x40);
    SPI2_ReadWriteByte((uint8_t)(arg >> 24));
    SPI2_ReadWriteByte((uint8_t)(arg >> 16));
    SPI2_ReadWriteByte((uint8_t)(arg >> 8));
    SPI2_ReadWriteByte((uint8_t)arg);
    SPI2_ReadWriteByte(crc);

    do
    {
        r1 = SPI2_ReadWriteByte(0xFF);
    } while ((r1 & 0x80) && --retry);

    return r1;
}

/****************************************************************************************************************
 * End of file
 ****************************************************************************************************************/