
#include "can_driver.h"
#include <miosix.h>
#include <cstdio>
#include <algorithm>
using namespace std;
using namespace miosix;

class Message
{
public:
	unsigned char message[8];
	unsigned int id;
	unsigned char len;
};
Message RxMsg;

static Mutex rxMutex;
static Queue<Message,8> rxQueue;

using cantx = Gpio<GPIOA_BASE,11>;
using canrx = Gpio<GPIOA_BASE,12>;
using canEn = Gpio<GPIOD_BASE,3>;

void canfdirq()
{
	
}

void canDriverInit(unsigned int speed)
{
	{
		FastInterruptDisableLock dLock;
		RCC->APB1HENR |= RCC_APB1HENR_FDCANEN;	//enable clock gating
		
		/* setup the clock of the FDCAN module with PLL1_q
			1- enable PLL 1 oscillator from RCC->CR register
			2- Select pll-1_q_clk as FDCAN clock */
		RCC->CR |=RCC_CR_PLL1ON;
		RCC->D2CCIP1R |=(1<<28);  
		
		RCC_SYNC();

		/* GPIOA->MODER   &= ~(3<<22) & ~(3<<24);	//reset PIN11 & PIN12 
		GPIOA->MODER   |=  (3<<22) | (3<<24);   //Choose alternate function for PIN11 & PIN12 on Port A for FDCAN1 
		GPIOA->AFR[1]  &= ~(15<<12) & ~(15<<16);//AF9
		GPIOA->AFR[1]  |=  (9<<12) | (9<<16); */
		
		cantx::mode(Mode::ALTERNATE);
		cantx::alternateFunction(9);
		canrx::mode(Mode::ALTERNATE);
		canrx::alternateFunction(9);
		canEn::mode(Mode::OUTPUT);
		canEn::low();
	}

		
	FDCAN1->CCCR   |= FDCAN_CCCR_INIT;               //Initialize the CAN module  INIT bit is set configuration
	
	while( FDCAN_CCCR_INIT == 0);                      // wait until intialization begains
	FDCAN1->CCCR   |= FDCAN_CCCR_CCE;              //CCE bit enabled to start configuration cleared automatically when INIT is cleared
	
	FDCAN1->CCCR   &= ~ (FDCAN_CCCR_FDOE);               //FDOE cleared to operate as classical CAN
	FDCAN1->GFC    &=~(FDCAN_GFC_ANFE);               //FDCAN global filter for accepting any non matching extended ID message and store it in RX FIFO 0
	FDCAN1->GFC    |=(3<<FDCAN_GFC_ANFE);
	
	FDCAN1->XIDFC  &=0x00000000;            //Reset register to configure  
	FDCAN1->XIDFC  |=(1<<FDCAN_XIDFC_LSE_Pos) | (0x0);    //1 element extended ID filter , Extended Filter ID start address  0x4000 AC00
	
	FDCAN1->RXF0C  &=(0x00000000);          //reset register to configure
	FDCAN1->RXF0C  |=(1<<31) | (3<<16) | (1<<14) | (0x404);  //overwriting mode, fifo size 3 elemnts, start address 0x4000 B004
	
	FDCAN1->RXESC &=0x00000000;             // reset RX data field (read only ask prof)
	FDCAN1->TXESC &=0x00000000;             // reset TX data field (read only ask prof)
	
	FDCAN1->RXESC |=(1<<4);             // 12 byte RX data field (read only ask prof)
	FDCAN1->TXESC |=0x00000001;             // 12 byte TX data field (read only ask prof)
	
	FDCAN1->TXBC  &=0x00000000;             // register reset for configuration 
	FDCAN1->TXBC  &=~(1<<30);               // TX FIFO mode
	FDCAN1->TXBC  |= (3<<24) | (2<<14) | (0x804);  // assign 3 FIFO elements , TX FIFO start address 0x4000 B404
	
	
	/*Enable test mode for internal loop back*/
	FDCAN1->CCCR |= FDCAN_CCCR_TEST;
	FDCAN1->CCCR |= FDCAN_CCCR_MON;
	
	/*Enable loop back */
	FDCAN1->TEST |=FDCAN_TEST_LBCK;
	
	
	/* set Nominal Bit Timming register so that sample point is at about 87.5% bit time from bit start 
	 for 1Mbit/s, BRP = 5, TSEG1 = 17, TSEG2 =3, SJW = 1 => 1 CAN bit = 20 TQ, sample at 85%  */
	FDCAN1->NBTP  = 0x00000000 ;  // register reset value 
	FDCAN1->NBTP |= ((1-1)<<25) | ((5-1)<<16) | ((17-1)<<8) | ((3-1)<<0);
	
	
	
	/*if data bit rate switch is enabled */
	/* set DATA Bit Timming register so that sample point is at about 87.5% bit time from bit start 
	 for 1Mbit/s, BRP = 5, TSEG1 = 13, TSEG2 =2, SJW = 1 => 1 CAN bit = 20 TQ, sample at 85%    */
	/* FDCAN1->DBTP = 0x00000000;
	FDCAN1->DBTP |= (2<<16) | (12<<8) | (1<<4);  */
	
	
	
	
	
	
	FDCAN1->IE |=(1<<0);      // new message interrupt Enable RX FIFO 0
	
	
	//filter element configuration in RAM 
	unsigned int volatile * EX_filter_element = (unsigned int *) 0x4000AC00;
	*EX_filter_element++ = 0x20000001;   // store message in RX FIFO 0 , EX_filter_identifier1  0b0 & 0x0000001
	*EX_filter_element = 0x40000001;  // filter type :dual ID filter , EX_filter_identifier2  0b0 & 0x0000001
	
	
	
	
	
	
	FDCAN1->CCCR   &= ~(FDCAN_CCCR_INIT);
}

int canDriverSend(unsigned int id, const void *message, int size)
{
	const char *msg=reinterpret_cast<const char *>(message);
	
	if ((FDCAN1->TXFQS & (1<<21)) != 0) return -1;  // check if fifo is full or not
	
	// Accessing Tx FIFO in RAM at address 0x4000B404
	unsigned int volatile *  Tx_FIFO = (unsigned int *) 0x4000B404; 
	*Tx_FIFO = 0x00000000;
	
	printf("Memory address is: 0x%p\n",(void *)Tx_FIFO);
	printf("Content of that address is: 0x%x\n",*Tx_FIFO);
	
	*Tx_FIFO = 0x40000000 | id;	// ESI=0, XTD=1,RTR =0
	printf("Memory address is: 0x%p\n",(void *)Tx_FIFO);
	printf("Content of that address is: 0x%x\n",*Tx_FIFO++);
	*Tx_FIFO = (size<<16);     // no of Data Bytes, No bit rate switching, FDF = 0, Classic CAN Tx, don't store fifo event
	
	printf("Memory address is: 0x%p\n",(void *)Tx_FIFO);
	printf("Content of that address is: 0x%x\n",*Tx_FIFO++);
	
	uint32_t i = 0;
	int remaining=size;
	while(remaining>=4)
	{
		*Tx_FIFO = ((msg[i+3] << 24) |
                      (msg[i+2] << 16) |
                      (msg[i+1] << 8) |
                       msg[i+0]);
		i+=4;
		remaining-=4;
		
		printf("Memory address is: 0x%p\n",(void *)Tx_FIFO);
		printf("Content of that address is: 0x%x\n",*Tx_FIFO++);
	}
	
	switch(remaining)
	{
		case 1: *Tx_FIFO = msg[i];
			break;
		case 2:  *Tx_FIFO = msg[i] | (msg[i+1] << 8);
			break;
		case 3:  *Tx_FIFO = msg[i] | (msg[i+1] << 8) | (msg[i+2] << 16);
			break;
	}
	
	FDCAN1->TXBTIE |=0x7;                   // enable interrupt for first 3 FIFO elements
	FDCAN1->TXBAR &=0x00000000;             //reset TXBAR 
	FDCAN1->TXBAR |=0x1;                    //add transmission request for TX FIFO first element (0), start transmission
	
	//TODO: wait until interrupt has arrived
	
	return size;
}

tuple<int, unsigned int> canDriverReceive(void *message, int size)
{
	uint32_t GetIndex = 0;
	uint8_t  *pData;
	
	FDCAN1->IE    &=~(1<<0);      // new message interrupt Disable RX FIFO 0
	
	if ((FDCAN1->RXF0S & (1<<24)) == 1){
		//printf("RX FIFO 0 is  full \n");   // check if fifo is full or not
		return make_tuple(-2,0);
	} else{
		
		GetIndex = ((FDCAN1->RXF0S & FDCAN_RXF0S_F0GI) >> 8);
		
		printf("===============================================\n");
		printf("RX FIFO 0 RAM MEMORY \n");
		
		//Accessing Rx FIFO in RAM at address 0x4000 B004
		unsigned int volatile * Rx_FIFo0SA = (unsigned int *) 0x4000B004; 
		printf("Memory  RX address is: 0x%p\n",(void *)Rx_FIFo0SA);
		printf("Content of that address is: 0x%x\n",*Rx_FIFo0SA);
		
		Rx_FIFo0SA = Rx_FIFo0SA + (GetIndex * size * 4); 
		
		/* Retrieve Identifier */
		RxMsg.id = *Rx_FIFo0SA & FDCAN_ELEMENT_MASK_EXTID;
		printf("id of recieved msg = %d\n",RxMsg.id);
		
		printf("Memory  RX address is: 0x%p\n",(void *)Rx_FIFo0SA);
		printf("Content of that address is: 0x%x\n",*Rx_FIFo0SA++);
		  
		//printf("\nDLC 0x%x\n",*Rx_FIFo0SA);
		  
		/* Retrieve DATA Length*/
		
		RxMsg.len = (*Rx_FIFo0SA & FDCAN_ELEMENT_MASK_DLC)>>16; 
		
		printf("Memory  RX address is: 0x%p\n",(void *)Rx_FIFo0SA);
		printf("Content of that address is: 0x%x\n",*Rx_FIFo0SA++);
		  
		/* Retrieve Rx payload */
		pData = (uint8_t *)Rx_FIFo0SA;
		unsigned char rxBuffer[16];
		for(int i = 0; i < RxMsg.len; i++)
		{
			rxBuffer[i] = *pData++;
		};
		
		printf("Memory  RX address is: 0x%p\n",(void *)Rx_FIFo0SA);
		printf("Content of that address is: 0x%x\n",*Rx_FIFo0SA++);
		
		printf("Memory  RX address is: 0x%p\n",(void *)Rx_FIFo0SA);
		printf("Content of that address is: 0x%x\n",*Rx_FIFo0SA++);
		
		
		/* int minSize = min<int>(RxMsg.len,size);
		memcpy(message,pData,minSize); */
		
		printf("\nid = %d len = %d\n",RxMsg.id,RxMsg.len);
		memDump(rxBuffer,RxMsg.len);
		
		/*Acknoledge and increment GetIndex*/
		 FDCAN1->RXF0A = GetIndex; 
		
		
		/*Lock l(rxMutex);
		rxQueue.get(RxMsg);
		*/
		if(RxMsg.len>size) printf("wrong size=%d\n",RxMsg.len);
		FDCAN1->IE    |=(1<<0);      // new message interrupt Enable RX FIFO 0
		return make_tuple(RxMsg.len<=size ? RxMsg.len : -1, RxMsg.id);
	}
}


void FDCAN1_IT0_IRQHandler(void) {

  /* if 	 {			      // message pending ?
	canDriverReceive(&RxMsg);                         // read the message

  } */
}
