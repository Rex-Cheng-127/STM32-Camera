/**
 ******************************************************************************
 * @file           : L1_OLED_Display.c
 * @brief          : Show Message to OLED Display (SSD1036) Through I2C
 * @author         : Rex Cheng
 * @copyright      : N/A
 ******************************************************************************
 */

/****************************************************************************************************************
 * Includes
 ****************************************************************************************************************/
#include "L1_OLED_Display.h"
#include "L1_OLED_Display_CodeTab.h"

/****************************************************************************************************************
 * Private typedef
 ****************************************************************************************************************/

/****************************************************************************************************************
 * Private define
 ****************************************************************************************************************/

/* 软件/硬件IIC切换宏 0：软件 1：硬件 */
#define IIC_SELECT 1

/*Configuration of I2C */
#define IIC_NUM I2C1
#define IIC_GPIO_CLK_ENABLE RCC_APB2PeriphClockCmd
#define IIC_GPIO_CLK RCC_APB2Periph_GPIOB
#define IIC_CLK_ENABLE RCC_APB1PeriphClockCmd
#define IIC_CLK RCC_APB1Periph_I2C1
#define IIC_GPIO_PORT GPIOB
#define IIC_SDA_GPIO_PIN GPIO_Pin_7
#define IIC_SCL_GPIO_PIN GPIO_Pin_6

/* 定义读写SCL和SDA的宏，已增加代码的可移植性和可阅读性 */
/* 条件编译： 1 选择GPIO的HAL库实现IO读写 */
#if 0
#define IIC_SDA_0 GPIO_WriteBit(IIC_GPIO_PORT, IIC_SDA_GPIO_PIN, Bit_RESET)
#define IIC_SDA_1 GPIO_WriteBit(IIC_GPIO_PORT, IIC_SDA_GPIO_PIN, Bit_SET)
#define IIC_SCL_0 GPIO_WriteBit(IIC_GPIO_PORT, IIC_SCL_GPIO_PIN, Bit_RESET)
#define IIC_SCL_1 GPIO_WriteBit(IIC_GPIO_PORT, IIC_SCL_GPIO_PIN, Bit_SET)

#define IIC_SDA_READ GPIO_ReadInputDataBit(IIC_GPIO_PORT, IIC_SDA_GPIO_PIN)

 /* 这个分支选择直接寄存器操作实现IO读写 
  * 注意：如下写法，在IAR最高级别优化时，会被编译器错误优化
  */
#else
#define IIC_SDA_0 IIC_GPIO_PORT->BSRR = IIC_SDA_GPIO_PIN << 16u /* SDA = 0 */
#define IIC_SDA_1 IIC_GPIO_PORT->BSRR = IIC_SDA_GPIO_PIN        /* SDA = 1 */
#define IIC_SCL_0 IIC_GPIO_PORT->BSRR = IIC_SCL_GPIO_PIN << 16u /* SCL = 0 */
#define IIC_SCL_1 IIC_GPIO_PORT->BSRR = IIC_SCL_GPIO_PIN        /* SCL = 1 */

#define IIC_SDA_READ (IIC_GPIO_PORT->IDR & IIC_SDA_GPIO_PIN) >> 11

#endif

/* Configuration of OLED */
#define OLED_ID 0x78
#define OLED_WR_CMD 0x00
#define OLED_WR_DATA 0x40

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

GPIO_InitTypeDef gpio_initstruct;
I2C_InitTypeDef iic_initstruct;

void IIC_GPIO_Config(void)
{
    IIC_GPIO_CLK_ENABLE(IIC_GPIO_CLK, ENABLE);
#if IIC_SELECT
    /* 使能时钟 */
    IIC_CLK_ENABLE(IIC_CLK, ENABLE);

    gpio_initstruct.GPIO_Mode = GPIO_Mode_AF_OD;
    gpio_initstruct.GPIO_Pin = IIC_SDA_GPIO_PIN | IIC_SCL_GPIO_PIN;
    gpio_initstruct.GPIO_Speed = GPIO_Speed_50MHz;

    GPIO_Init(IIC_GPIO_PORT, &gpio_initstruct);

    iic_initstruct.I2C_Mode = I2C_Mode_I2C;
    iic_initstruct.I2C_Ack = I2C_Ack_Enable;
    iic_initstruct.I2C_AcknowledgedAddress = I2C_AcknowledgedAddress_7bit;
    iic_initstruct.I2C_ClockSpeed = 400000;
    iic_initstruct.I2C_DutyCycle = I2C_DutyCycle_2;
    iic_initstruct.I2C_OwnAddress1 = 0;

    I2C_Init(IIC_NUM, &iic_initstruct);

    /* 使能IIC */
    I2C_Cmd(IIC_NUM, ENABLE);
#else
    /* 配置GPIO引脚
     * 此处配置GPIO为开漏模式 强下拉 弱上拉
     * 禁止所有设备输出强上拉的高电平的同时兼顾SDA输入输出
     */
    gpio_initstruct.GPIO_Mode = GPIO_Mode_Out_OD;
    gpio_initstruct.GPIO_Pin = IIC_SDA_GPIO_PIN | IIC_SCL_GPIO_PIN;
    gpio_initstruct.GPIO_Speed = GPIO_Speed_50MHz;

    GPIO_Init(IIC_GPIO_PORT, &gpio_initstruct);
#endif
}

static void IIC_Delay(void)
{
    uint8_t i;

    /*　
        下面的时间是通过逻辑分析仪测试得到的。
        工作条件：CPU主频72MHz ，MDK编译环境，1级优化

        循环次数为10时，SCL频率 = 205KHz
        循环次数为7时，SCL频率 = 347KHz， SCL高电平时间1.5us，SCL低电平时间2.87us
        循环次数为5时，SCL频率 = 421KHz， SCL高电平时间1.25us，SCL低电平时间2.375us
    */
    for (i = 0; i < 10; i++)
        ;
}

/* IIC 通讯起始信号 */
void IIC_Start(void)
{
    /* SCL和SDA拉高 */
    IIC_SDA_1;
    IIC_SCL_1;

    IIC_Delay();
    /* SCL高电平期间SDA拉低 */
    IIC_SDA_0;

    /* 从机接收开始信号 延时 SCL拉低 准备传输数据 */
    IIC_Delay();
    IIC_SCL_0;
}

void IIC_Stop(void)
{
    /* SDA和SCL拉低 */
    IIC_SDA_0;
    IIC_SCL_0;

    /* SCL拉高 */
    IIC_SCL_1;

    /* 延时 释放SDA */
    IIC_Delay();
    IIC_SDA_1;
}

void IIC_SendByte(uint8_t byte)
{
    for (uint8_t i = 0; i < 8; i++)
    {
        /* 从最高位按位取出byte */
        if (byte >> (7 - i) & 0x01)
            IIC_SDA_1;
        else
            IIC_SDA_0;
        /* 时钟线拉高 从机接收数据 */
        IIC_SCL_1;
        /* 延时 等待从机接收数据 */
        IIC_Delay();
        /* SCL拉低 主机发送下一位数据 */
        IIC_SCL_0;
        IIC_Delay();
    }
    /* 主机释放SDA */
    IIC_SDA_1;
    /* 延时 等待从机接收完成 */
    IIC_Delay();
}

uint8_t IIC_ReciveByte(void)
{
    uint8_t temp;

    for (uint8_t i = 0; i < 8; i++)
    {
        /* 时钟线拉高 主机接收数据 */
        IIC_SCL_1;
        IIC_Delay();
        if (IIC_SDA_READ)
            temp = IIC_SDA_READ << (7 - i);
        /* 时钟线拉低 从机继续发送数据 */
        IIC_SCL_0;
        /* 等待从机发送下一位数据 */
        IIC_Delay();
    }

    return temp;
}

/* 发送应答和不应答 */
void IIC_ACK(uint8_t ack)
{
    if (ack)
        IIC_SDA_1;
    else
        IIC_SDA_0;
    /* SCL拉高 */
    IIC_SCL_1;
    IIC_Delay();
    /* SCL拉低 */
    IIC_SCL_0;
    IIC_Delay();
    /* 主机释放总线 */
    IIC_SDA_1;
}

/* 等待应答和非应答 0:应答 1:非应答 */
uint8_t IIC_Wait_ACK(void)
{
    uint8_t ack;

    /* 主机释放SDA */
    IIC_SDA_1;
    IIC_Delay();
    /* SCL拉高 */
    IIC_SCL_1;
    IIC_Delay();
    if (IIC_SDA_READ)
        ack = 1;
    else
        ack = 0;
    /* SCL拉低 */
    IIC_SCL_0;
    IIC_Delay();

    return ack;
}

#if IIC_SELECT
void IIC_Write_Byte(uint8_t addr, uint8_t data)
{
    while (I2C_GetFlagStatus(IIC_NUM, I2C_FLAG_BUSY))
        ;

    I2C_GenerateSTART(IIC_NUM, ENABLE); /* IIC_Start信号 */
    while (!I2C_CheckEvent(IIC_NUM, I2C_EVENT_MASTER_MODE_SELECT))
        ; /*EV5,主模式*/

    I2C_Send7bitAddress(IIC_NUM, OLED_ID, I2C_Direction_Transmitter); /* 呼叫从机 */
    while (!I2C_CheckEvent(IIC_NUM, I2C_EVENT_MASTER_TRANSMITTER_MODE_SELECTED))
        ;
    I2C_SendData(IIC_NUM, addr); /* 配置数据写入地址 */
    while (!I2C_CheckEvent(IIC_NUM, I2C_EVENT_MASTER_BYTE_TRANSMITTED))
        ;

    I2C_SendData(IIC_NUM, data); /* 发送数据 */
    while (!I2C_CheckEvent(IIC_NUM, I2C_EVENT_MASTER_BYTE_TRANSMITTED))
        ;

    I2C_GenerateSTOP(IIC_NUM, ENABLE); /* IIC_Stop信号 */
}

#endif

/* oled写数据 */
void Oled_Write_Data(uint8_t data)
{
#if IIC_SELECT
    IIC_Write_Byte(OLED_WR_DATA, data);
#else
    IIC_Start();
    IIC_SendByte(OLED_ID);
    /* 等待应答 */
    while (IIC_Wait_ACK())
        ;
    IIC_SendByte(OLED_WR_DATA);
    /* 等待应答 */
    while (IIC_Wait_ACK())
        ;
    IIC_SendByte(data);
    /* 等待应答 */
    while (IIC_Wait_ACK())
        ;
    IIC_Stop();
#endif
}

/* oled写命令 */
void Oled_Write_Cmd(uint8_t cmd)
{
#if IIC_SELECT
    IIC_Write_Byte(OLED_WR_CMD, cmd);
#else
    IIC_Start();
    IIC_SendByte(OLED_ID);
    /* 等待应答 */
    while (IIC_Wait_ACK())
        ;
    IIC_SendByte(OLED_WR_CMD);
    /* 等待应答 */
    while (IIC_Wait_ACK())
        ;
    IIC_SendByte(cmd);
    /* 等待应答 */
    while (IIC_Wait_ACK())
        ;
    IIC_Stop();
#endif
}

void OLED_Init(void)
{
    /* 设置显示打开/关闭
     * AE--->显示打开
     * AF--->显示关闭(休眠模式)
     */
    Oled_Write_Cmd(0xAE);

    /* ================== 基本命令表 ===================*/
    /* 设置对比度
     * 0~255：数值越大 亮度越亮
     */
    Oled_Write_Cmd(0x81);
    Oled_Write_Cmd(0xFF);

    /* 使能全屏显示
     * A4--->恢复到RAM内容显示
     * A5--->忽略RAM内容显示
     */
    Oled_Write_Cmd(0xA4);

    /* 设置显示模式
     * A6--->正常显示：0灭1亮
     * A7--->逆显示：1灭0亮
     */
    Oled_Write_Cmd(0xA6);

    /* ================== 滚动命令表 ===================*/
    /* 滚动使能/失能
     * 2E--->失能
     * 2F--->使能
     */
    Oled_Write_Cmd(0x2E);

    /* 七字节命令： 连续水平滚动设置 */

    /* 左/右水平滚动设置
     * 26--->右水平滚动
     * 27--->左水平滚动
     */
    Oled_Write_Cmd(0x26);
    /* 虚拟字节 */
    Oled_Write_Cmd(0x00);
    /* 设置滚动起始页地址 */
    Oled_Write_Cmd(0x00);
    /* 设置滚动间隔 */
    Oled_Write_Cmd(0x03);
    /* 设置滚动结束地址 */
    Oled_Write_Cmd(0x07);
    /* 虚拟字节 */
    Oled_Write_Cmd(0x00);
    Oled_Write_Cmd(0xFF);

    /* =============== 寻址设置命令表 ==================*/

    /* 双字节命令:寄存器寻址模式 */
    Oled_Write_Cmd(0x20);

    /* 10:页寻址模式
     * 01:垂直寻址模式
     * 00:水平寻址模式
     */
    Oled_Write_Cmd(0x10);
    /* 单字节命令:设置页寻址的起始页地址 */
    Oled_Write_Cmd(0xB0);
    /* 单字节命令:设置页寻址的起始列地址低位 */
    Oled_Write_Cmd(0x00);
    /* 单字节命令:设置页寻址的起始列地址高位 */
    Oled_Write_Cmd(0x10);

    /*=============== 硬件配置命令表 ==================*/

    /* 设置显示开始线
     * 0x40~0x7F对应0~63
     */
    Oled_Write_Cmd(0x40);

    /* 设置列重映射
     * A0:addressX--->segX
     * A1:addressX--->seg(127-X)
     */
    Oled_Write_Cmd(0xA1);

    /* 设置多路复用比 */
    Oled_Write_Cmd(0xA8);
    Oled_Write_Cmd(0x3F);

    /* 设置COM输出扫描方向
     * C0：COM0--->COM63(从上往下扫描)
     * C8：COM63--->COM0(从下往上扫描)
     */
    Oled_Write_Cmd(0xC8);

    /* 双字节命令：设置COM显示偏移量 */
    Oled_Write_Cmd(0xD3);
    Oled_Write_Cmd(0x00); /* COM不偏移 */

    /* 双字节命令：配置COM重映射 */
    Oled_Write_Cmd(0xDA);
    Oled_Write_Cmd(0x12);

    /* 双字节命令：设置预充期 */
    Oled_Write_Cmd(0xD9);
    Oled_Write_Cmd(0x22); /* 阶段一2个无效DCLK时钟/阶段二2个无效DCLK时钟 */

    /* 设置VCOMH取消选择电平
     * 00:0.65xVcc
     * 20:0.77xVcc
     * 30:0.83xVcc
     */
    Oled_Write_Cmd(0xDB);
    Oled_Write_Cmd(0x20);

    /* 双字节命令：设置电荷泵 */
    Oled_Write_Cmd(0x8d);
    Oled_Write_Cmd(0x14);

    Oled_Write_Cmd(0xAF);
}

/**
 * @brief  设置光标
 * @param  x,光标x位置
 *		   y,光标y位置
 * @retval 无
 */
void OLED_SetPos(unsigned char x, unsigned char y) // 设置起始点坐标
{
    Oled_Write_Cmd(0xb0 + y);
    Oled_Write_Cmd(((x & 0xf0) >> 4) | 0x10);
    Oled_Write_Cmd((x & 0x0f) | 0x01);
}

/**
 * @brief  填充整个屏幕
 * @param  fill_data:要填充的数据
 * @retval 无
 */
void OLED_Fill(unsigned char fill_data) // 全屏填充
{
    unsigned char m, n;
    for (m = 0; m < 8; m++)
    {
        Oled_Write_Cmd(0xb0 + m); // page0-page1
        Oled_Write_Cmd(0x00);     // low column start address
        Oled_Write_Cmd(0x10);     // high column start address
        for (n = 0; n < 128; n++)
        {
            Oled_Write_Data(fill_data);
        }
    }
}

/**
 * @brief  清屏
 * @param  无
 * @retval 无
 */
void OLED_CLS(void) // 清屏
{
    OLED_Fill(0x00);
}

/**
 * @brief  将OLED从休眠中唤醒
 * @param  无
 * @retval 无
 */
void OLED_ON(void)
{
    Oled_Write_Cmd(0X8D); // 设置电荷泵
    Oled_Write_Cmd(0X14); // 开启电荷泵
    Oled_Write_Cmd(0XAF); // OLED唤醒
}

/**
 * @brief  让OLED休眠 -- 休眠模式下,OLED功耗不到10uA
 * @param  无
 * @retval 无
 */
void OLED_OFF(void)
{
    Oled_Write_Cmd(0X8D); // 设置电荷泵
    Oled_Write_Cmd(0X10); // 关闭电荷泵
    Oled_Write_Cmd(0XAE); // OLED休眠
}

/**
 * @brief 显示codetab.h中的ASCII字符,有6*8和8*16可选择
 * @param  x,y : 起始点坐标(x:0~127, y:0~7);
 *					ch[] :- 要显示的字符串;
 *					textsize : 字符大小(1:6*8 ; 2:8*16)
 * @retval 无
 */
void OLED_ShowStr(unsigned char x, unsigned char y, unsigned char ch[], unsigned char textsize)
{
    unsigned char c = 0, i = 0, j = 0;
    switch (textsize)
    {
    case 1:
    {
        while (ch[j] != '\0')
        {
            c = ch[j] - 32;
            if (x > 126)
            {
                x = 0;
                y++;
            }
            OLED_SetPos(x, y);
            for (i = 0; i < 6; i++)
                Oled_Write_Data(F6x8[c][i]);
            x += 6;
            j++;
        }
    }
    break;
    case 2:
    {
        while (ch[j] != '\0')
        {
            c = ch[j] - 32;
            if (x > 120)
            {
                x = 0;
                y++;
            }
            OLED_SetPos(x, y);
            for (i = 0; i < 8; i++)
                Oled_Write_Data(F8X16[c * 16 + i]);
            OLED_SetPos(x, y + 1);
            for (i = 0; i < 8; i++)
                Oled_Write_Data(F8X16[c * 16 + i + 8]);
            x += 8;
            j++;
        }
    }
    break;
    }
}

/**
 * @brief  OLED_ShowCN，显示codetab.h中的汉字,16*16点阵
 * @param  x,y: 起始点坐标(x:0~127, y:0~7); N:汉字在codetab.h中的索引
 *
 * @retval 无
 */
void OLED_ShowCN(unsigned char x, unsigned char y, unsigned char n)
{
    unsigned char wm = 0;
    unsigned int adder = 32 * n;
    OLED_SetPos(x, y);
    for (wm = 0; wm < 16; wm++)
    {
        Oled_Write_Data(F16x16[adder]);
        adder += 1;
    }
    OLED_SetPos(x, y + 1);
    for (wm = 0; wm < 16; wm++)
    {
        Oled_Write_Data(F16x16[adder]);
        adder += 1;
    }
}

/**
 * @brief  OLED_DrawBMP，显示BMP位图
 * @param  x0,y0 :起始点坐标(x0:0~127, y0:0~7);x1,y1 : 起点对角线(结束点)的坐标(x1:1~128,y1:1~8)
 * @retval 无
 */
void OLED_DrawBMP(unsigned char x0, unsigned char y0, unsigned char x1, unsigned char y1, unsigned char BMP[])
{
    unsigned int j = 0;
    unsigned char x, y;

    if (y1 % 8 == 0)
    {
        y = y1 / 8;
    }
    else
    {
        y = y1 / 8 + 1;
    }
    for (y = y0; y < y1; y++)
    {
        OLED_SetPos(x0, y);
        for (x = x0; x < x1; x++)
        {
            Oled_Write_Data(BMP[j++]);
        }
    }
}

void OLED_Display_Init(void)
{
    IIC_GPIO_Config();
    OLED_Init();
}

void task_OLED_Display(void *pvParameters)
{
    while (1)
    {
        OLED_Fill(0xFF);  // 全屏点亮
        vTaskDelay(2000); // 2s
        OLED_Fill(0x00);  // 全屏灭
        vTaskDelay(2000); // 2s
        for (uint8_t i = 0; i < 4; i++)
        {
            OLED_ShowCN(22 + i * 16, 0, i); // 测试显示中文
        }
        vTaskDelay(2000);                                         // 2s
        OLED_ShowStr(0, 3, (unsigned char *)"Wildfire Tech", 1);  // 测试6*8字符
        OLED_ShowStr(0, 4, (unsigned char *)"Hello wildfire", 2); // 测试8*16字符
        vTaskDelay(2000);                                         // 2s
        OLED_CLS();                                               // 清屏
        OLED_OFF();                                               // 测试OLED休眠
        vTaskDelay(2000);                                         // 2s
        OLED_ON();                                                // 测试OLED休眠后唤醒
        OLED_DrawBMP(0, 0, 128, 8, (unsigned char *)BMP1);        // 测试BMP位图显示
        vTaskDelay(2000);
    }
}

/****************************************************************************************************************
 * End of file
 ****************************************************************************************************************/