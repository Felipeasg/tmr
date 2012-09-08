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
unsigned int data_m[13] = {0};

// MS5611
unsigned long D1;    // ADC value of the pressure conversion 
unsigned long D2;    // ADC value of the temperature conversion  
unsigned int C[8];   // calibration coefficients 
double P;   // compensated pressure value 
double T;   // compensated temperature value 
double dT;   // difference between actual and measured temperature 
double OFF;   // offset at actual temperature 
double SENS;   // sensitivity at actual temperature 

unsigned char ms_crc4 = 0;
unsigned int nprom[] = {0x3132,0x3334,0x3536,0x3738,0x3940,0x4142,0x4344,0x450B}; // CRC =0x0B so..  0x4500 + 0x0B


void vLoopTask( void *pvParameters );
void vDiagTask( void *pvParameters );
uint8_t I2C_ByteRead(u16 Add, u8 Reg);
void I2C_ByteWrite(u16 Add, u8 Reg,u8 Data,u8 Cmd);
void I2C_MutiRead(u8* pBuffer, u8 Add, u8 Reg,u8 Count);
void I2C2_Configuration(void);
unsigned char crc4(unsigned int n_prom[]) ;

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
#define MS5611_ADDRESS    0x77
#define I2C_SPEED 400000

// MS5611
#define CMD_RESET    0x1E // ADC reset command 
#define CMD_ADC_READ 0x00 // ADC read command 
#define CMD_ADC_CONV 0x40 // ADC conversion command 
#define CMD_ADC_D1   0x00 // ADC D1 conversion 
#define CMD_ADC_D2   0x10 // ADC D2 conversion 
#define CMD_ADC_256  0x00 // ADC OSR=256 
#define CMD_ADC_512  0x02 // ADC OSR=512 
#define CMD_ADC_1024 0x04 // ADC OSR=1024 
#define CMD_ADC_2048 0x06 // ADC OSR=2056 
#define CMD_ADC_4096 0x08 // ADC OSR=4096 
#define CMD_PROM_RD  0xA0 // Prom read command


#define TRUE   (1)                      /* Boolean true value.   */
#define FALSE  (0)                      /* Boolean false value.  */


void vDiagTask( void *pvParameters )
{
    GPIO_InitTypeDef  GPIO_PB;
    I2C_InitTypeDef   I2C_CH2;
    unsigned int delay=0, i=0;

    if( pvParameters != NULL )
        delay = *((unsigned int*)pvParameters);
    else
        delay = 2;

    //------------------------------------------------
    // BEEP & GPIO Configuration
    //------------------------------------------------
    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOB, ENABLE);

    GPIO_PB.GPIO_Pin = GPIO_Pin_12;
    GPIO_PB.GPIO_Mode = GPIO_Mode_OUT;
    GPIO_PB.GPIO_OType = GPIO_OType_PP;
    GPIO_PB.GPIO_Speed = GPIO_Speed_100MHz;
    GPIO_PB.GPIO_PuPd = GPIO_PuPd_NOPULL;

    GPIO_Init(GPIOB, &GPIO_PB);
    //------------------------------------------------

    //------------------------------------------------
    // I2C & GPIO Configuration
    //------------------------------------------------
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
    //------------------------------------------------

    //------------------------------------------------
    // Test PCA9536
    //------------------------------------------------
    I2C_ByteWrite(PCA9536DP_ADDRESS, 0x03, 0xF7, NULL); // IO3 us output pin
    //------------------------------------------------

    //------------------------------------------------
    // Test PCA9533
    //------------------------------------------------
    I2C_ByteWrite(PCA9533DP_ADDRESS, 0x01, 0xFF, NULL); // PSC0 : P = 1.68
    I2C_ByteWrite(PCA9533DP_ADDRESS, 0x02, 0x40, NULL); // PWM0 50%(0x80)
    I2C_ByteWrite(PCA9533DP_ADDRESS, 0x03, 0x97, NULL); // PSC1 : P = 1.00
    I2C_ByteWrite(PCA9533DP_ADDRESS, 0x04, 0x40, NULL); // PWM1 25%(0x40)
    I2C_ByteWrite(PCA9533DP_ADDRESS, 0x05, 0xBB, NULL); // LS0
    //------------------------------------------------

    //------------------------------------------------
    // Test MPU6050
    //------------------------------------------------
    I2C_ByteWrite(MPU6050_ADDRESS, 0x37, 0x02, NULL);
    
    data[0]=I2C_ByteRead(MPU6050_ADDRESS, 0x75); // to check MPU6050
    data[1]=I2C_ByteRead(MPU6050_ADDRESS, 0x24); // to check MPU6050
    data[2]=I2C_ByteRead(MPU6050_ADDRESS, 0x6A); // to check MPU6050
    data[3]=I2C_ByteRead(MPU6050_ADDRESS, 0x37); // to check MPU6050
    //------------------------------------------------

    //------------------------------------------------
    // Test MS5611
    //------------------------------------------------
    I2C_ByteWrite(MS5611_ADDRESS, CMD_RESET, NULL, TRUE); // Reset command for MS5611
    
    delay = 20 / portTICK_RATE_MS; // delay 20 ms
    vTaskDelay(delay);

    // PROM READ SEQUENCE 
    for(i=0 ; i<8 ; i++)
    {
        I2C_MutiRead((u8*)&(C[i]), MS5611_ADDRESS, (CMD_PROM_RD + (i<<1)), 2);
        C[i]= (C[i]>>8) | ((C[i]&0x00FF)<<8);
    }

    // CONVERSION SEQUENCE

    // D1
    I2C_ByteWrite(MS5611_ADDRESS, (CMD_ADC_CONV|CMD_ADC_D1|CMD_ADC_4096), NULL, TRUE); // ADC command for MS5611

    delay = 200 / portTICK_RATE_MS; // delay 200 ms
    vTaskDelay(delay);

    I2C_MutiRead((u8*)&(D1), MS5611_ADDRESS, CMD_ADC_READ, 3);

    // D2
    I2C_ByteWrite(MS5611_ADDRESS, (CMD_ADC_CONV|CMD_ADC_D2|CMD_ADC_4096), NULL, TRUE); // ADC command for MS5611

    delay = 200 / portTICK_RATE_MS; // delay 200 ms
    vTaskDelay(delay);

    I2C_MutiRead((u8*)&(D2), MS5611_ADDRESS, CMD_ADC_READ, 3);

    // CYCLIC REDUNDANCY CHECK (CRC) 
    ms_crc4=crc4(C);

    //------------------------------------------------

    //------------------------------------------------
    // Test HMC5883L
    //------------------------------------------------
    //Read out all register data from HMC5883L
    for(i=0 ; i<13 ; i++)
    {
        data_m[i]=I2C_ByteRead(HMC5883L_ADDRESS, i); // to check HMC5883L
    }
    //------------------------------------------------

    //------------------------------------------------
    // LOOP
    //------------------------------------------------
    while(1)
    {
        #if 0
        GPIO_SetBits(GPIOB, GPIO_Pin_12);
        delay = 20 / portTICK_RATE_MS;
        vTaskDelay(delay);
        GPIO_ResetBits(GPIOB, GPIO_Pin_12);
        #endif

        delay = 1000 / portTICK_RATE_MS;

        I2C_ByteWrite(PCA9536DP_ADDRESS, 0x01, 0xF7, NULL); // IO3 (ON)
        vTaskDelay(delay);
        I2C_ByteWrite(PCA9536DP_ADDRESS, 0x01, 0xFF, NULL); // IO3 (OFF)
        vTaskDelay(delay);
    }
    //------------------------------------------------
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
uint8_t I2C_ByteRead(u16 Add, u8 Reg)
{
    u16 tempADD; 
    uint8_t rData;
    //I2C2_Configuration();
    while(I2C_GetFlagStatus(I2C2, I2C_FLAG_BUSY));    //�˴��`�u�O�_�� �N�O�� SCL ��SDA�O�_�� �C

    tempADD=Add<<1;
    I2C_AcknowledgeConfig(I2C2, ENABLE);    //���\1�r�`1�����Ҧ�

    I2C_GenerateSTART(I2C2, ENABLE);    // �o�e�_�l�� 
    while(!I2C_CheckEvent(I2C2, I2C_EVENT_MASTER_MODE_SELECT));    //EV5,�D�Ҧ� SR1.SB����=1
    //while((((u32)(I2C1->SR2) << 16) | (u32)(I2C1->SR1) & 0x00FFFFBF) != I2C_EVENT_MASTER_MODE_SELECT); 

    I2C_Send7bitAddress(I2C2, tempADD, I2C_Direction_Transmitter);    //�o�e����᪺����a�}(�g)
    while (!I2C_CheckEvent(I2C2, I2C_EVENT_MASTER_TRANSMITTER_MODE_SELECTED));    // Test on EV6 and clear it ��u�L���h�A�����B��S���D

    //  I2C_Cmd(I2C1, ENABLE);    /* Clear EV6 by setting again the PE bit */--------���ҬO�_�ݭn

    I2C_SendData(I2C2, Reg);    /*�o�eP�H�s���a�}*/
    while (!I2C_CheckEvent(I2C2, I2C_EVENT_MASTER_BYTE_TRANSMITTED));    /*�ƾڤw�o�e*/ /* Test on EV8 and clear it */

    I2C_GenerateSTART(I2C2, ENABLE);    //�_�l��
    while(!I2C_CheckEvent(I2C2, I2C_EVENT_MASTER_MODE_SELECT));    /* Test on EV5 and clear it */

    I2C_Send7bitAddress(I2C2, tempADD, I2C_Direction_Receiver);    /*�o�e����a�}(Ū)*/
    while(!I2C_CheckEvent(I2C2, I2C_EVENT_MASTER_RECEIVER_MODE_SELECTED));    /* Test on EV6 and clear it */

    while(!I2C_CheckEvent(I2C2, I2C_EVENT_MASTER_BYTE_RECEIVED));    /* EV7 */
    rData= I2C_ReceiveData(I2C2);    /* Ū Register*/
    I2C_AcknowledgeConfig(I2C2, DISABLE);    //�̫�@���n����������
    I2C_GenerateSTOP(I2C2, ENABLE);    //�o�e����� 

    return rData;
}

void I2C2_Configuration(void)
{
    I2C_InitTypeDef  I2C_InitStructure; 
    I2C_DeInit(I2C2); 
    I2C_Cmd(I2C2, DISABLE); 

    /* I2C configuration */
    I2C_InitStructure.I2C_Mode = I2C_Mode_I2C;    //�]�m��i2c�Ҧ�
    I2C_InitStructure.I2C_DutyCycle = I2C_DutyCycle_2;    //I2C �ֳt�Ҧ� Tlow / Thigh = 2 
    I2C_InitStructure.I2C_OwnAddress1 = 0;    //�o�Ӧa�褣�ө��դ���@�ΡA�ۨ��a�}�H�O���O�u���q�Ҧ��~���ġH
    I2C_InitStructure.I2C_Ack = I2C_Ack_Enable;    //�ϯ� ���� �\��
    I2C_InitStructure.I2C_AcknowledgedAddress = I2C_AcknowledgedAddress_7bit;    //����7��a�}
    I2C_InitStructure.I2C_ClockSpeed = I2C_SPEED;    //�]�mi2c���t�v�A���ప�� 400KHz

    I2C_Cmd(I2C2, ENABLE);    //�ϯ�i2c1�~�]
    I2C_Init(I2C2, &I2C_InitStructure);    //�t�mi2c1

    I2C_AcknowledgeConfig(I2C2, ENABLE);    //���\1�r�`1�����Ҧ�

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
void I2C_ByteWrite(u16 Add, u8 Reg,u8 Data,u8 Cmd)
{
    u16 tempADD;
    //I2C2_Configuration();
    //*((u8 *)0x4001080c) |=0x80; 
    while(I2C_GetFlagStatus(I2C2, I2C_FLAG_BUSY));    // �˴�i2c�`�u���A
    tempADD=Add<<1;    

    I2C_AcknowledgeConfig(I2C2, ENABLE);    //���\1�r�`1�����Ҧ�

    I2C_GenerateSTART(I2C2, ENABLE);    //�o�estart�H��

    /* Test on EV5 and clear it */
    while(!I2C_CheckEvent(I2C2, I2C_EVENT_MASTER_MODE_SELECT));    //EV5�GSB=1�AŪSR1�M��N�a�}�g�JDR�H�s���N�M���Өƥ� 
    I2C_Send7bitAddress(I2C2, tempADD, I2C_Direction_Transmitter);    //�o�e�]�Ʀa�}�A�D�o�e

    /* Test on EV6 and clear it */
    while(!I2C_CheckEvent(I2C2, I2C_EVENT_MASTER_TRANSMITTER_MODE_SELECTED));    //EV6�GADDR=1�AŪSR1�M��ŪSR2�N�M���Өƥ�

    /* Clear EV6 by setting again the PE bit */
    I2C_Cmd(I2C2, ENABLE);    //���ҬO�_�ݭn���B�H************

    /* Send the EEPROM's internal address to write to */
    I2C_SendData(I2C2, Reg);    //�o�e�H�s���a�}

    /* Test on EV8 and clear it */
    while(!I2C_CheckEvent(I2C2, I2C_EVENT_MASTER_BYTE_TRANSMITTED));    //EV8�GTxE=1�A�g�JDR�H�s���N�M���Өƥ�

    if(Cmd == NULL)
    {
        /* Send the byte to be written */
        I2C_SendData(I2C2, Data);    //�o�e�t�m�r

        /* Test on EV8 and clear it */
        while(!I2C_CheckEvent(I2C2, I2C_EVENT_MASTER_BYTE_TRANSMITTED));
    }
  
    /* Send STOP condition */
    I2C_GenerateSTOP(I2C2, ENABLE);    //�o�e����H��
}

/*******************************************************************************
* Function Name  : SKATER_I2C_MultRead
* Description    : �h�r�`�gŪ�����w�a�}���ƾڡ]7��a�}�^
* Input          : - pBuffer�G�ƾڦs�x��
*                  - I2C_SLAVE_ADDRESS : �]�Ʀa�}
*                  - RegAddr:�_�l�H�s���a�}
*                  - NumByteToRead�G�s��Ū�����r�`�ƥ�
* Output         : None
* Return         : None
*******************************************************************************/
void I2C_MutiRead(u8* pBuffer, u8 Add, u8 Reg,u8 Count)
{
    u8 tempADD;
    //I2C1_Configuration();
    //*((u8 *)0x4001080c) |=0x80; 
    while(I2C_GetFlagStatus(I2C2, I2C_FLAG_BUSY));    // Added by Najoua 27/08/2008
    I2C_AcknowledgeConfig(I2C2, ENABLE);    //���\1�r�`1�����Ҧ�    

    /* Send START condition */
    I2C_GenerateSTART(I2C2, ENABLE);
    //*((u8 *)0x4001080c) &=~0x80;
    tempADD=Add<<1;

    while(!I2C_CheckEvent(I2C2, I2C_EVENT_MASTER_MODE_SELECT));    /* Test on EV5 and clear it */
    I2C_Send7bitAddress(I2C2, tempADD, I2C_Direction_Transmitter);    /* �o�e����᪺�]�Ʀa�}�A�]�m�D�o�e�Ҧ� */

    while(!I2C_CheckEvent(I2C2, I2C_EVENT_MASTER_TRANSMITTER_MODE_SELECTED));    /* Test on EV6 and clear it */
    //  I2C_Cmd(I2C1, ENABLE); /* Clear EV6 by setting again the PE bit */

    I2C_SendData(I2C2, Reg);    /* �o�e�nŪ�����H�s���a�} */
    while(!I2C_CheckEvent(I2C2, I2C_EVENT_MASTER_BYTE_TRANSMITTED));    /* Test on EV8 and clear it */

    I2C_GenerateSTART(I2C2, ENABLE);    /* �o�e �}�l �H�� */  
    while(!I2C_CheckEvent(I2C2, I2C_EVENT_MASTER_MODE_SELECT));    /* Test on EV5 and clear it */

    I2C_Send7bitAddress(I2C2, tempADD, I2C_Direction_Receiver);    /* �o�e����᪺�]�Ʀa�} */
    while(!I2C_CheckEvent(I2C2, I2C_EVENT_MASTER_RECEIVER_MODE_SELECTED));    /* Test on EV6 and clear it */

    /* While there is data to be read */
    while(Count)  
    {
        if(Count == 1)
        {
            I2C_AcknowledgeConfig(I2C2, DISABLE);    /* Disable Acknowledgement */
            I2C_GenerateSTOP(I2C2, ENABLE);    /* Send STOP Condition */
        }

        /* Test on EV7 and clear it */

        /* ���F�b����̫�@�Ӧr�`�Უ�ͤ@��NACK�߽ġA�bŪ�˼ƲĤG�Ӽƾڦr�`����(�b�˼ƲĤG��RxNE�ƥ󤧫�)�����M��ACK��C
        ���F���ͤ@�Ӱ���/���_�l����A�n�󥲶��bŪ�˼ƲĤG�Ӽƾڦr�`����(�b�˼ƲĤG��RxNE�ƥ󤧫�)�]�mSTOP/START��C
        �u�����@�Ӧr�`�ɡA��n�bEV6����(EV6_1�ɡA�M��ADDR����)�n���������M������󪺲��ͦ�C*/

        if(I2C_CheckEvent(I2C2, I2C_EVENT_MASTER_BYTE_RECEIVED))  
        // while(!I2C_CheckEvent(I2C1, I2C_EVENT_MASTER_BYTE_RECEIVED)); 
        {      
            /* Read a byte from the EEPROM */
            *pBuffer = I2C_ReceiveData(I2C2);      
            pBuffer++;    /* Point to the next location where the byte read will be saved */
            Count--;    /* Decrement the read bytes counter */
        }   
    }

    /* Enable Acknowledgement to be ready for another reception */
    I2C_AcknowledgeConfig(I2C2, ENABLE);
}

//********************************************************
//! @brief calculate the CRC code for details look into CRC CODE NOTES
//! 
//! @return crc code
//********************************************************
unsigned char crc4(unsigned int n_prom[])
{
    int cnt;    // simple counter
    unsigned int n_rem;    // crc reminder
    unsigned int crc_read;    // original value of the crc
    unsigned char  n_bit;
    n_rem = 0x00;
    crc_read=n_prom[7];    //save read CRC
    n_prom[7]=(0xFF00 & (n_prom[7]));    //CRC byte is replaced by 0

    for (cnt = 0; cnt < 16; cnt++)    // operation is performed on bytes
    {
        // choose LSB or MSB
        if (cnt%2==1) n_rem ^= (unsigned short) ((n_prom[cnt>>1]) & 0x00FF);
        else n_rem ^= (unsigned short) (n_prom[cnt>>1]>>8);

        for (n_bit = 8; n_bit > 0; n_bit--)
        {
            if (n_rem & (0x8000))
            {
                n_rem = (n_rem << 1) ^ 0x3000;
            }
            else
            {
                n_rem = (n_rem << 1);
            }
        }
    }

    n_rem=  (0x000F & (n_rem >> 12));    // final 4-bit reminder is CRC code
    n_prom[7]=crc_read;    // restore the crc_read to its original place
    
    return (n_rem ^ 0x00);
}
