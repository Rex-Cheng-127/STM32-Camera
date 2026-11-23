/**
  ******************************************************************************
  * @file           : L1_EERPOM.c
  * @brief          : Read/Write EERPM (24C02) Through I2C
  * @author         : Rex Cheng (bloodmooon1993@gmail.com)
  * @copyright      : Copyright (c) 2023
  ******************************************************************************
*/  


/****************************************************************************************************************
 * Includes  
 ****************************************************************************************************************/
#include "L1_EEPROM.h"

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


/****************************************************************************************************************
 * Private function prototypes
 ****************************************************************************************************************/


/****************************************************************************************************************
 * Private functions
 ****************************************************************************************************************/

void EEPROM_Test(void)
{
  u8 pd;
  
  //Write Test
  printf("Write EEPROM......"); 
  pd='A';  
  AT24C_Sendone(0, pd);
	printf("Write EEPROM Finish!"); 

  //Read Test 
  pd = AT24C_Rcvone(0);
	printf("Read EEPROM: %c \n", pd);

}

void IIC_Init(void)
{					     
	GPIO_InitTypeDef GPIO_InitStructure;
	RCC_APB2PeriphClockCmd(	RCC_APB2Periph_GPIOC, ENABLE );	
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_11|GPIO_Pin_12;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP ;   //推挽输出
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(GPIOC, &GPIO_InitStructure);
	GPIO_SetBits(GPIOC,GPIO_Pin_11|GPIO_Pin_12); 	//PC11,PC12 输出高
}


//起动总线函数               
//Start_I2c();  
//功能:     启动I2C总线,即发送I2C起始条件.  
void Start_I2c(void)
{
  SDA_OUT();     //sda线输出
  SDA=1;         //发送起始条件的数据信号
  SCL=1;
  Delay_us(4); 
  SDA=0;         //发送起始信号
  Delay_us(4);      
  SCL=0;       //钳住I2C总线，准备发送或接收数据 
}


//结束总线函数               
//Stop_I2c();  
//功能:     结束I2C总线,即发送I2C结束条件.  
void Stop_I2c(void)
{
  SDA_OUT();     //sda线输出
  SCL=0;
  SDA=0;      //发送结束条件的数据信号
  Delay_us(4);
  SCL=1;      //结束条件建立时间大于4μs
  SDA=1;      //发送I2C总线结束信号
  Delay_us(4);
}


//等待应答信号到来
//返回值：1，接收应答失败
//       0，接收应答成功
u8 I2c_wait_ack(void)
{
	u8 Time=0; 
    SDA_IN();			  //配置SDA为输入
    SDA=1;                //准备接收应答位
    Delay_us(1);  
    SCL=1;
    Delay_us(1); 			 
	while(READ_SDA)			   
	{
		Time++;
		if(Time>250)
		{
			Stop_I2c();
			return 1;	 //无应答返回1
		}
	}
	SCL=0;//时钟输出0 
	
	return 0;  			 //有应答返回0
} 



//字节数据发送函数               
//SendByte(u8 c);
//功能:     将数据c发送出去,可以是地址,也可以是数据
void  SendByte(u8  c)
{
 u8  BitCnt;

 SDA_OUT();     //sda线输出
 SCL=0;			//拉低时钟开始数据传输
 								  //条件 一定要开启总线 保持SCL处于0状态 才能进行写入
 for(BitCnt=0;BitCnt<8;BitCnt++)  //要传送的数据长度为8位
  {
     if((c<<BitCnt)&0x80)SDA=1;   //判断发送位  发送是由高位开始发送
       else  SDA=0;                
     Delay_us(2); 
     SCL=1;               //置时钟线为高，通知被控器开始接收数据位
     Delay_us(2);        
     SCL=0;
     Delay_us(2); 	   	  
  }
}


//产生ACK应答
void I2C_Ack(void)
{
	SCL=0;
	SDA_OUT();     //sda线输出
	SDA=0;
	Delay_us(2); 		  
	SCL=1;
	Delay_us(2);
	SCL=0;
}
//不产生ACK应答		    
void I2C_NAck(void)
{
  SCL=0;
	SDA_OUT();     //sda线输出
	SDA=1;
	Delay_us(2); 
	SCL=1;
	Delay_us(2);
	SCL=0;
}


//字节数据接收函数               
//RcvByte();
//功能:  用来接收从器件传来的数据,并判断总线错误(发应答信号)
//ack  1 发送应答  0 不发送应答
u8 RcvByte(u8 ack)
{
  u8 retc=0,
     i;
  SDA_IN();//SDA设置为输入
  for(i=0;i<8;i++)
  {         
    SCL=0;                  //置时钟线为低，准备接收数据位
    Delay_us(2);
    SCL=1;                  //置时钟线为高使数据线上数据有效
		Delay_us(1); 
    retc<<=1;
    if(READ_SDA)retc++;       //读数据位,接收的数据位放入retc中 
		Delay_us(1); 
  }

  if (!ack)
      I2C_NAck();//发送nACK
  else
      I2C_Ack(); //发送ACK   

  return retc;					
}



//24c 默认的硬件地址都是 0xa0  即 1010   然后A2 A1 A0  最后一位 为读写位  1读 0写
//24c02  256x8 bit    
//24c04  512x8 bit
//而子地址 只是8位的 即最大也就256个字节  那么24c04 就要占用2个256个字节 
//这里24c04硬件上采用的是 分页方法  即有24c02里有A0 A1 A2 当是24c04时 A0做为分页位
//当A0为0 表示第一页 写入第一页256字节
//当A0为1 表示第二页 写入第二页256字节


//初始化IIC接口
void AT24CXX_Init(void)
{
	IIC_Init();
}

//在AT24CXX指定地址读出一个数据
//Addr:开始读数的地址  
//返回值  :读到的数据
u8 AT24C_Rcvone(u8 Addr)
{
   u8 temp=0;
   Start_I2c();                //启动总线
   SendByte(0xa0);             //发送写命令
   I2c_wait_ack();			   //等待应答
   SendByte(Addr);             //发送地址
   I2c_wait_ack();			   //等待应答

   Start_I2c();                //重新启动总线
   SendByte(0xa1);		       //设置为读操作
   I2c_wait_ack();			   //等待应答;

   temp=RcvByte(0);			   //读字节	非应答

   Stop_I2c();                 //结束总线 
   return temp;
}



//在AT24CXX指定地址写入一个数据	 此函数只限于 c02-c16
//Addr:写入数据的目的地址    
//Data:要写入的数据
void AT24C_Sendone(u8 Addr,u8 Data)
{
   Start_I2c();               //启动总线

   SendByte(0xa0);            //发送写命令
   I2c_wait_ack();			  //等待应答
   SendByte(Addr);            //发送地址
   I2c_wait_ack();			  //等待应答
   SendByte(Data);			  //发送字节数据
   I2c_wait_ack();			  //等待应答
   Stop_I2c();                //结束总线 
   Delay_ms(10);			  //如果是连续发送字节的时候这个延时很重要 否则将回传错
}


//在AT24CXX里面的指定地址开始写入长度为Len的数据
//该函数用于写入16bit或者32bit的数据.
//Addr  :开始写入的地址  
//Data  :数据数组首地址
//Len   :要写入数据的长度2,4
void AT24C_SendLenByte(u8 Addr,u8 *Data,u8 Len)
{  	
	while(Len--)
	{
		AT24C_Sendone(Addr,*Data);
		Addr++;
		Data++;
	}												    
}



//在AT24CXX里面的指定地址开始读出长度为Len的数据
//该函数用于读出16bit或者32bit的数据.
//Addr   :开始读出的地址 
//返回值     :数据
//Len        :要读出数据的长度2,4
void AT24C_RcvLenByte(u8 Addr,u8 *temp,u8 Len)
{  	

   while(Len)
   {
	 *temp++=AT24C_Rcvone(Addr++);
	 Len--;				   
	}
												    
}


/****************************************************************************************************************
 * End of file
 ****************************************************************************************************************/