
#include <cstdio>
#include "miosix.h"
#include "can_driver.h"

using namespace std;
using namespace miosix;

int main()
{
	
    
		printf("Testing\n");
		Thread::sleep(1000);
		
		unsigned char TxMSg[]={'1','2','3','4','5'};
		
		canDriverInit(1000);
		Thread::sleep(500);
		//canDriverSend(11,TxMSg,5); 
	
	 unsigned int id=0; int size=5;
	
	printf("TX FIFO RAM MEMORY \n");
	
	// Accessing Tx FIFO in RAM at address 0x4000B404
	unsigned int *  Tx_FIFO = (unsigned int *) 0x4000B404; 
	*Tx_FIFO = 0;
	printf("Memory address is: 0x%p\n",(void *)Tx_FIFO);
	printf("Content of that address is: 0x%x\n",*Tx_FIFO);
	
	*Tx_FIFO = 0x40000001 | id;   // ESI=0, XTD=1,RTR =0
	
	printf("Memory address is: 0x%p\n",(void *)Tx_FIFO);
	printf("Content of that address is: 0x%x\n",*Tx_FIFO);
	
	
	
	*++Tx_FIFO = (size<<16);     // no of Data Bytes, No bit rate switching, FDF = 0, Classic CAN Tx, don't store fifo event
	
	printf("Memory address is: 0x%p\n",(void *)Tx_FIFO);
	printf("Content of that address is: 0x%x\n",*Tx_FIFO);
	
	*Tx_FIFO++;
	uint32_t i = 0;
	
	while(size>4)
	{
		*Tx_FIFO = ((TxMSg[i+3] << 24) |
                      (TxMSg[i+2] << 16) |
                      (TxMSg[i+1] << 8) |
                       TxMSg[i+0]);
		
		size-=4;
		i +=4;
	}
	
	printf("Memory address is: 0x%p\n",(void *)Tx_FIFO);
	printf("Content of that address is: 0x%x\n",*Tx_FIFO);
	
	*Tx_FIFO++;
	switch(size)
	{
		case 1: *Tx_FIFO = TxMSg[i];
			break;
		case 2:  *Tx_FIFO = TxMSg[i] | (TxMSg[i+1] << 8);
			break;
		case 3:  *Tx_FIFO = TxMSg[i] | (TxMSg[i+1] << 8) | (TxMSg[i+2] << 16);
			
	}
	
	printf("Memory address is: 0x%p\n",(void *)Tx_FIFO);
	printf("Content of that address is: 0x%x\n",*Tx_FIFO);
	
	FDCAN1->TXBTIE |=0x7;                   // enable interrupt for first 3 FIFO elements
	FDCAN1->TXBAR &=0x00000000;             //reset TXBAR 
	FDCAN1->TXBAR |=0x1;                    //add transmission request for TX FIFO first element (0), start transmission
	
	
	printf("===============================================\n");
	printf("RX FIFO 0 RAM MEMORY \n");
	
	//Accessing RX FIFO 0 element
	unsigned int * Rx_FIFO = (unsigned int *) 0x4000B004;
	
	 /* Retrieve Identifier */
	  id = (*Rx_FIFO++ & FDCAN_ELEMENT_MASK_EXTID); 
	  
	  /* Retrieve DATA Length*/
	   int len = (*Rx_FIFO++ & FDCAN_ELEMENT_MASK_DLC)>>16; 
	  
	   /* Retrieve Rx payload */
	   unsigned char rxBuffer[16];
     uint8_t* pData = (uint8_t *)Rx_FIFO;
    for(int i = 0; i < len; i++)
    {
		rxBuffer[i] = *pData++;
    } 
	printf("id = %d len = %d\n",id,len);
	memDump(rxBuffer,len);
	
	/* printf("Memory  RX address is: 0x%p\n",(void *)Rx_FIFO);
	printf("Content of that address is: 0x%x\n",*Rx_FIFO);
	*Rx_FIFO++;
	
	
	printf("Memory  RX address is: 0x%p\n",(void *)Rx_FIFO);
	printf("Content of that address is: 0x%x\n",*Rx_FIFO);
	*Rx_FIFO++;
	printf("Memory  RX address is: 0x%p\n",(void *)Rx_FIFO);
	printf("Content of that address is: 0x%x\n",*Rx_FIFO);
	*Rx_FIFO++;
	printf("Memory  RX address is: 0x%p\n",(void *)Rx_FIFO);
	printf("Content of that address is: 0x%x\n",*Rx_FIFO); */
	
	
    
}
