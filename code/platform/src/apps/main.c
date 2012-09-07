/* Standard includes. */
#include <stdio.h>

/* Scheduler includes. */
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "semphr.h"

/* ST StdPeriph Driver includes. */
#include "stm32f4xx_conf.h"

unsigned int count = 0;
unsigned int p1=2,p2=10,p3=10000;
unsigned int data[6] = {0};

void vLoopTask( void *pvParameters );
void vDiagTask( void *pvParameters );
uint8_t I2C_ByteRead(u16 I2C_SLAVE_ADDRESS, u8 RegAddr);
void I2C_ByteWrite(u16 I2C_SLAVE_ADDRESS, u8 RegAddr,u8 CData);

void I2C2_Configuration(void);



int main(void)
{	
	unsigned int exit = 0;

	xTaskCreate( vLoopTask, ( signed portCHAR * ) "LOOP-1", configMINIMAL_STACK_SIZE*2, (void*)&p1, 3, NULL );
	xTaskCreate( vLoopTask, ( signed portCHAR * ) "LOOP-2", configMINIMAL_STACK_SIZE*2, (void*)&p2, 2, NULL );
	xTaskCreate( vDiagTask, ( signed portCHAR * ) "DIAG-3", configMINIMAL_STACK_SIZE*2, (void*)&p3, 1, NULL );
	
	/* Start the scheduler. */
	vTaskStartScheduler();
	
	while(exit == 1);

	return 0;
}

void vLoopTask( void *pvParameters )
{
    unsigned int delay = 0;

    if( pvParameters != NULL )
        delay = *((unsigned int*)pvParameters);
    else
        delay = 2;

	while(1)
	{
		count++;
		vTaskDelay(delay);
	}
}

#define PCA9536DP_ADDRESS 0x41
#define PCA9533DP_ADDRESS 0x62
#define MPU6050_ADDRESS   0x69
#define HMC5883L_ADDRESS  0x1E
#define MS5611_ADDRESS    0x76
#define I2C_SPEED 400000

void vDiagTask( void *pvParameters )
{
    GPIO_InitTypeDef  GPIO_PB;
    I2C_InitTypeDef   I2C_CH2;
    unsigned int delay = 0;
    
    //const portTickType xDelay = 5000 / portTICK_RATE_MS;

    if( pvParameters != NULL )
        delay = *((unsigned int*)pvParameters);
    else
        delay = 2;

    // BEEP
    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOB, ENABLE);

    GPIO_PB.GPIO_Pin = GPIO_Pin_12;
    GPIO_PB.GPIO_Mode = GPIO_Mode_OUT;
    GPIO_PB.GPIO_OType = GPIO_OType_PP;
    GPIO_PB.GPIO_Speed = GPIO_Speed_100MHz;
    GPIO_PB.GPIO_PuPd = GPIO_PuPd_NOPULL;
    
    GPIO_Init(GPIOB, &GPIO_PB);

    // I2C
    /*!< I2C Periph clock enable */
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_I2C2, ENABLE);
  
    /*!< I2C_SCL_GPIO_CLK and I2C_SDA_GPIO_CLK Periph clock enable */
    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOB, ENABLE);

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_SYSCFG, ENABLE);
  
    /* Reset I2C IP */
    RCC_APB1PeriphResetCmd(RCC_APB1Periph_I2C2, ENABLE);
  
    /* Release reset signal of I2C IP */
    RCC_APB1PeriphResetCmd(RCC_APB1Periph_I2C2, DISABLE);
    
    /*!< GPIO configuration */
    /* Connect PXx to I2C_SCL*/
    GPIO_PinAFConfig(GPIOB, GPIO_PinSource10, GPIO_AF_I2C2);
    /* Connect PXx to I2C_SDA*/
    GPIO_PinAFConfig(GPIOB, GPIO_PinSource11, GPIO_AF_I2C2);
    
    GPIO_PB.GPIO_Pin = GPIO_Pin_10|GPIO_Pin_11;
    GPIO_PB.GPIO_Mode = GPIO_Mode_AF;
    GPIO_PB.GPIO_OType = GPIO_OType_OD;
    GPIO_PB.GPIO_Speed = GPIO_Speed_100MHz;
    GPIO_PB.GPIO_PuPd = GPIO_PuPd_NOPULL;
    
    GPIO_Init(GPIOB, &GPIO_PB);

    /*!< I2C configuration */
    /* I2C configuration */
    I2C_CH2.I2C_Mode = I2C_Mode_I2C;
    I2C_CH2.I2C_DutyCycle = I2C_DutyCycle_2;
    I2C_CH2.I2C_OwnAddress1 = 0;
    I2C_CH2.I2C_Ack = I2C_Ack_Enable;
    I2C_CH2.I2C_AcknowledgedAddress = I2C_AcknowledgedAddress_7bit;
    I2C_CH2.I2C_ClockSpeed = I2C_SPEED;
  
    /* I2C Peripheral Enable */
    I2C_Cmd(I2C2, ENABLE);
    /* Apply I2C configuration after enabling it */
    I2C_Init(I2C2, &I2C_CH2);

    I2C_ByteWrite(PCA9536DP_ADDRESS, 0x03, 0xF7); // IO3 us output pin

    I2C_ByteWrite(PCA9533DP_ADDRESS, 0x01, 0xFF); // PSC0 : P = 1.68
    I2C_ByteWrite(PCA9533DP_ADDRESS, 0x02, 0x80); // PWM0
    I2C_ByteWrite(PCA9533DP_ADDRESS, 0x03, 0x97); // PSC1 : P = 1.00
    I2C_ByteWrite(PCA9533DP_ADDRESS, 0x04, 0x80); // PWM1
    I2C_ByteWrite(PCA9533DP_ADDRESS, 0x05, 0xBB); // LS0
    

    I2C_ByteWrite(MPU6050_ADDRESS, 0x37, 0x02);
    
    data[0]=I2C_ByteRead(MPU6050_ADDRESS, 0x75); // to check MPU6050
    //data[1]=I2C_ByteRead(MPU6050_ADDRESS, 0x24); // to check MPU6050
    data[1]=I2C_ByteRead(MPU6050_ADDRESS, 0x6A); // to check MPU6050
    data[2]=I2C_ByteRead(MPU6050_ADDRESS, 0x37); // to check MPU6050
    //data[3]=I2C_ByteRead(HMC5883L_ADDRESS, 0x0A); // to check HMC5883L
    //data[4]=I2C_ByteRead(MS5611_ADDRESS, 0x0); // to check MS5611
    while(1)
    {
       #if 1
        GPIO_SetBits(GPIOB, GPIO_Pin_12);
        delay = 20 / portTICK_RATE_MS;
        vTaskDelay(delay);
        GPIO_ResetBits(GPIOB, GPIO_Pin_12);
       #endif

        delay = 1000 / portTICK_RATE_MS;

        I2C_ByteWrite(PCA9536DP_ADDRESS, 0x01, 0xF7); // IO3 (ON)
        vTaskDelay(delay);
        I2C_ByteWrite(PCA9536DP_ADDRESS, 0x01, 0xFF); // IO3 (OFF)
        vTaskDelay(delay);

        //data=I2C_ByteRead(PCA9536DP_ADDRESS, 0x00);
        //data=I2C_ByteRead(MPU6050_ADDRESS, 0x75);
    }
}

/*******************************************************************************
* Function Name  : SKATER_I2C_ByteRead
* Description    : ��r�`Ū�����w�a�}���ƾڡ]7��a�}�^.
* Input          : - I2C_SLAVE_ADDRESS : �]�Ʀa�}�C
*                  - RegAddr:�H�s���a�}.
*                  - Data�GŪ������r�`�ƾ�
* Output         : None
* Return         : rData�GŪ�����ƾ�
*******************************************************************************/
uint8_t I2C_ByteRead(u16 I2C_SLAVE_ADDRESS, u8 RegAddr)
{
    u16 tempADD; 
    uint8_t rData;
    //I2C2_Configuration();
    while(I2C_GetFlagStatus(I2C2, I2C_FLAG_BUSY));            //�˴��`�u�O�_�� �N�O�� SCL ��SDA�O�_�� �C

    tempADD=I2C_SLAVE_ADDRESS<<1;
    I2C_AcknowledgeConfig(I2C2, ENABLE);                     //���\1�r�`1�����Ҧ�

    I2C_GenerateSTART(I2C2, ENABLE);                        // �o�e�_�l�� 
    while(!I2C_CheckEvent(I2C2, I2C_EVENT_MASTER_MODE_SELECT)); //EV5,�D�Ҧ� SR1.SB����=1
//while((((u32)(I2C1->SR2) << 16) | (u32)(I2C1->SR1) & 0x00FFFFBF) != I2C_EVENT_MASTER_MODE_SELECT); 

    I2C_Send7bitAddress(I2C2, tempADD, I2C_Direction_Transmitter);   //�o�e����᪺����a�}(�g)
    while (!I2C_CheckEvent(I2C2, I2C_EVENT_MASTER_TRANSMITTER_MODE_SELECTED)); // Test on EV6 and clear it ��u�L���h�A�����B��S���D

//  I2C_Cmd(I2C1, ENABLE);                              /* Clear EV6 by setting again the PE bit */--------���ҬO�_�ݭn
 
    I2C_SendData(I2C2, RegAddr);                                      /*�o�eP�H�s���a�}*/
    while (!I2C_CheckEvent(I2C2, I2C_EVENT_MASTER_BYTE_TRANSMITTED)); /*�ƾڤw�o�e*/ /* Test on EV8 and clear it */


   I2C_GenerateSTART(I2C2, ENABLE);                                     //�_�l��
   while(!I2C_CheckEvent(I2C2, I2C_EVENT_MASTER_MODE_SELECT));  /* Test on EV5 and clear it */


   I2C_Send7bitAddress(I2C2, tempADD, I2C_Direction_Receiver);                /*�o�e����a�}(Ū)*/
   while(!I2C_CheckEvent(I2C2, I2C_EVENT_MASTER_RECEIVER_MODE_SELECTED)); /* Test on EV6 and clear it */

   while(!I2C_CheckEvent(I2C2, I2C_EVENT_MASTER_BYTE_RECEIVED)); /* EV7 */
   rData= I2C_ReceiveData(I2C2);                                 /* Ū Register*/
    I2C_AcknowledgeConfig(I2C2, DISABLE); //�̫�@���n����������
    I2C_GenerateSTOP(I2C2, ENABLE);   //�o�e����� 

             
 return rData;
}

void I2C2_Configuration(void)
{
  I2C_InitTypeDef  I2C_InitStructure; 
  I2C_DeInit(I2C2); 
  I2C_Cmd(I2C2, DISABLE); 

  /* I2C configuration */
  I2C_InitStructure.I2C_Mode = I2C_Mode_I2C;                   //�]�m��i2c�Ҧ�
  I2C_InitStructure.I2C_DutyCycle = I2C_DutyCycle_2;           //I2C �ֳt�Ҧ� Tlow / Thigh = 2 
  I2C_InitStructure.I2C_OwnAddress1 = 0;     //�o�Ӧa�褣�ө��դ���@�ΡA�ۨ��a�}�H�O���O�u���q�Ҧ��~���ġH
  I2C_InitStructure.I2C_Ack = I2C_Ack_Enable;                  //�ϯ� ���� �\��
  I2C_InitStructure.I2C_AcknowledgedAddress = I2C_AcknowledgedAddress_7bit;   //����7��a�}
  I2C_InitStructure.I2C_ClockSpeed = I2C_SPEED;                               //�]�mi2c���t�v�A���ప�� 400KHz
  

  I2C_Cmd(I2C2, ENABLE);                                       //�ϯ�i2c1�~�]
  I2C_Init(I2C2, &I2C_InitStructure);                          //�t�mi2c1

  I2C_AcknowledgeConfig(I2C2, ENABLE);                        //���\1�r�`1�����Ҧ�
    
//  printf("\n\r I2C1_��l�Ƨ���\n\r");
    
}


/*******************************************************************************
* Function Name  : SKATER_I2C_ByteWrite
* Description    : ��r�`�g�J���w�a�}���ƾګ��w�ƾڡ]7��a�}�^--�@�묰����r�ΰt�m.
* Input          : - I2C_SLAVE_ADDRESS : �]�Ʀa�}�C
*                  - RegAddr:�H�s���a�}.
*                  - CData�G�ƾ�
* Output         : None
* Return         : None
*******************************************************************************/
void I2C_ByteWrite(u16 I2C_SLAVE_ADDRESS, u8 RegAddr,u8 CData)
{
   u16 tempADD;
   //I2C2_Configuration();
//*((u8 *)0x4001080c) |=0x80; 
    while(I2C_GetFlagStatus(I2C2, I2C_FLAG_BUSY)); // �˴�i2c�`�u���A
    tempADD=I2C_SLAVE_ADDRESS<<1;    
   
  I2C_AcknowledgeConfig(I2C2, ENABLE);           //���\1�r�`1�����Ҧ�
  
  I2C_GenerateSTART(I2C2, ENABLE);                //�o�estart�H��
  
  /* Test on EV5 and clear it */
  while(!I2C_CheckEvent(I2C2, I2C_EVENT_MASTER_MODE_SELECT));  //EV5�GSB=1�AŪSR1�M��N�a�}�g�JDR�H�s���N�M���Өƥ� 
  I2C_Send7bitAddress(I2C2, tempADD, I2C_Direction_Transmitter);   //�o�e�]�Ʀa�}�A�D�o�e

  /* Test on EV6 and clear it */
  while(!I2C_CheckEvent(I2C2, I2C_EVENT_MASTER_TRANSMITTER_MODE_SELECTED));  //EV6�GADDR=1�AŪSR1�M��ŪSR2�N�M���Өƥ�
  
  /* Clear EV6 by setting again the PE bit */
  I2C_Cmd(I2C2, ENABLE);                                                   //���ҬO�_�ݭn���B�H************

  /* Send the EEPROM's internal address to write to */
  I2C_SendData(I2C2, RegAddr);                                             //�o�e�H�s���a�}

  /* Test on EV8 and clear it */
  while(!I2C_CheckEvent(I2C2, I2C_EVENT_MASTER_BYTE_TRANSMITTED));         //EV8�GTxE=1�A�g�JDR�H�s���N�M���Өƥ�

  /* Send the byte to be written */
  I2C_SendData(I2C2, CData);                                               //�o�e�t�m�r
   
  /* Test on EV8 and clear it */
  while(!I2C_CheckEvent(I2C2, I2C_EVENT_MASTER_BYTE_TRANSMITTED));
  
  /* Send STOP condition */
  I2C_GenerateSTOP(I2C2, ENABLE);                                          //�o�e����H��
  
}

