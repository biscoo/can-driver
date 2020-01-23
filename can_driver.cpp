
#include "can_driver.h"
#include <cstdio>
#include <algorithm>
#include <miosix.h>
#include "kernel/scheduler/scheduler.h"
#include "kernel/sync.h"

using namespace std;
using namespace miosix;

class Message
{
public:
	unsigned char canMessage[8];
	unsigned int id;
	unsigned char len;
};

static Mutex rxMutex;
static Queue<Message,8> rxQueue;

using cantx = Gpio<GPIOA_BASE,11>;
using canrx = Gpio<GPIOA_BASE,12>;
using canEn = Gpio<GPIOD_BASE,3>;

void canDriverInit(CanSpeed bitRate)
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
	
	FDCAN_CCU->CCFG = (0<<16) | FDCANCCU_CCFG_BCC;
	
	FDCAN1->CCCR   &= ~ (FDCAN_CCCR_FDOE);               //FDOE cleared to operate as classical CAN
	FDCAN1->GFC    &=~(FDCAN_GFC_ANFE);               //FDCAN global filter for accepting any non matching extended ID message and store it in RX FIFO 0
	FDCAN1->GFC    |=(3<<FDCAN_GFC_ANFE);
	
	FDCAN1->XIDFC  &=0x00000000;            //Reset register to configure  
	FDCAN1->XIDFC  |=(1<<FDCAN_XIDFC_LSE_Pos) | (0x0);    //1 element extended ID filter , Extended Filter ID start address  0x4000 AC00
	
	FDCAN1->RXF0C  &=(0x00000000);          //reset register to configure
	FDCAN1->RXF0C  |=(1<<31) | (3<<16) | (1<<14) | (0x404);  //overwriting mode, fifo size 3 elemnts, start address 0x4000 B004
	
	FDCAN1->RXESC &=0x00000000;             // reset RX data field (read only ask prof)
	FDCAN1->TXESC &=0x00000000;             // reset TX data field (read only ask prof)
	
	FDCAN1->RXESC &=0x00000000;             // 8 byte RX data field (read only ask prof)
	FDCAN1->TXESC &=0x00000000;             // 8 byte TX data field (read only ask prof)
	
	FDCAN1->TXBC  &=0x00000000;             // register reset for configuration 
	FDCAN1->TXBC  &=~(1<<30);               // TX FIFO mode
	FDCAN1->TXBC  |= (3<<24) | (2<<14) | (0x804);  // assign 3 FIFO elements , TX FIFO start address 0x4000 B404
	
	
	/*Enable test mode for internal loop back*/
	//FDCAN1->CCCR |= FDCAN_CCCR_TEST;
	//FDCAN1->CCCR |= FDCAN_CCCR_MON;
	
	/*Enable loop back */
	//FDCAN1->TEST |=FDCAN_TEST_LBCK;
	
	int BRP = 0;
	switch(bitRate){
		case CanSpeed::SPEED_1Mbps:
		BRP = 2;
		break;
		case CanSpeed::SPEED_500Kbps:
		BRP = 4;
		break;
		case CanSpeed::SPEED_250Kbps:
		BRP = 8;
		break;
		case CanSpeed::SPEED_125Kbps:
		BRP = 16;
		break;
	}
	/* set Nominal Bit Timming register so that sample point is at about 87.5% bit time from bit start 
	 for 1Mbit/s, BRP = 2, TSEG1 = 19, TSEG2 =5, SJW = 9 => 1 CAN bit = 40 TQ, sample at 85%  */
	FDCAN1->NBTP  = 0x00000000 ;  // register reset value 
	FDCAN1->NBTP |= ((10-1)<<25) | ((BRP-1)<<16) | ((39-1)<<8) | ((10-1)<<0);
	
	FDCAN1->IE |= FDCAN_IE_RF0NE;      // new message interrupt Enable RX FIFO 0
	FDCAN1->ILE |= FDCAN_ILE_EINT0;
	
	//filter element configuration in RAM 
	unsigned int volatile * EX_filter_element = (unsigned int *) 0x4000AC00;
	*EX_filter_element++ = 0x20000001;   // store message in RX FIFO 0 , EX_filter_identifier1  0b0 & 0x0000001
	*EX_filter_element = 0x40000001;  // filter type :dual ID filter , EX_filter_identifier2  0b0 & 0x0000001
	
	FDCAN1->CCCR   &= ~(FDCAN_CCCR_INIT);
	
	NVIC_EnableIRQ(FDCAN1_IT0_IRQn);
}

int canDriverSend(unsigned int id, const void *message, int size)
{
	const char *msg=reinterpret_cast<const char *>(message);
	
	if ((FDCAN1->TXFQS & (1<<21)) != 0) return -1;  // check if fifo is full or not
	
	// Accessing Tx FIFO in RAM at address 0x4000B404
	unsigned int volatile *  Tx_FIFO = (unsigned int *) 0x4000B404; 
	*Tx_FIFO = 0x00000000;
	*Tx_FIFO = 0x40000000 | id;	// ESI=0, XTD=1,RTR =0
	Tx_FIFO++;
	*Tx_FIFO = (size<<16);     // no of Data Bytes, No bit rate switching, FDF = 0, Classic CAN Tx, don't store fifo event
	Tx_FIFO++;
	
	uint32_t i = 0;
	int remaining=size;
	while(remaining>=4)
	{
		*Tx_FIFO = ((msg[i+3] << 24) |
                    (msg[i+2] << 16) |
                    (msg[i+1] << 8)  |
                     msg[i+0]);
		i+=4;
		remaining-=4;
		Tx_FIFO++;
	}
	
	switch(remaining)
	{
		case 1: *Tx_FIFO = msg[i];
			break;
		case 2: *Tx_FIFO = msg[i] | (msg[i+1] << 8);
			break;
		case 3: *Tx_FIFO = msg[i] | (msg[i+1] << 8) | (msg[i+2] << 16);
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
	Message RxMsg;
	char *msg=reinterpret_cast<char *>(message);
	{
		Lock<Mutex> l(rxMutex);
		rxQueue.get(RxMsg);
	}
	
	for(int i = 0; i < min<int>(RxMsg.len, size); i++)
	{
		msg[i] = RxMsg.canMessage[i];
	}
	return make_tuple(RxMsg.len<=size ? RxMsg.len : -1, RxMsg.id);
}

void __attribute__((naked)) FDCAN1_IT0_IRQHandler() {
	saveContext();
	asm volatile("bl _Z18FDCAN1_IT0_IRQImplv");
	restoreContext();
}

void __attribute__((used)) FDCAN1_IT0_IRQImpl() {
	if(FDCAN1->IR & FDCAN_IR_RF0N) {
		Message RxMsg;
		// Fill message fields
		uint32_t GetIndex = 0;
		uint8_t  *pData;
		GetIndex = ((FDCAN1->RXF0S & FDCAN_RXF0S_F0GI) >> 8);
	
		//Accessing Rx FIFO in RAM at address 0x4000 B004
		unsigned int volatile * Rx_FIFo0SA = (unsigned int *) 0x4000B004;
		Rx_FIFo0SA += (GetIndex * 16)/sizeof(unsigned int);
		/* Retrieve Identifier */
		if(((*Rx_FIFo0SA && FDCAN_ELEMENT_MASK_XTD)>>30) == 1){
			RxMsg.id = *Rx_FIFo0SA & FDCAN_ELEMENT_MASK_EXTID;
		}else{
			RxMsg.id = ((*Rx_FIFo0SA & FDCAN_ELEMENT_MASK_STDID)>>18);
		} 
		Rx_FIFo0SA++;
		/* Retrieve DATA Length*/
		RxMsg.len = (*Rx_FIFo0SA & FDCAN_ELEMENT_MASK_DLC)>>16;
		Rx_FIFo0SA++;
		/* Retrieve Rx payload */
		pData = (uint8_t *)Rx_FIFo0SA;
		for(int i = 0; i < RxMsg.len; i++)
		{
			RxMsg.canMessage[i] = *pData++;
		}
		
		/*Acknoledge and increment GetIndex*/
		FDCAN1->RXF0A = GetIndex;
		FDCAN1->IR = FDCAN_IR_RF0N; //TODO: here is good?

		bool hppw=false;
		rxQueue.IRQput(RxMsg,hppw);
		if(hppw) Scheduler::IRQfindNextThread();
	}
}


