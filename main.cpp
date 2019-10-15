
#include <cstdio>
#include "miosix.h"
#include "can_driver.h"

using namespace std;
using namespace miosix;



int main()
{
	
    
		printf("Testing\n");
		Thread::sleep(1000);
		unsigned char TxMSg[]={'1','2','3','4','5','6','7','8'};
		unsigned int id = 0;
		int TxMsgSize = sizeof(TxMSg)/sizeof(TxMSg[0]);
		
		
		canDriverInit(CanSpeed::SPEED_125Kbps);
		
		Thread::sleep(500);
		
		
		 for(int i=0;i<8;i++)
		{
			// int txSize=1+(i%8);
			printf("Tx Msg: ");
			for(int i=0;i<8;i++)printf("%c", TxMSg[i]);
			printf("\n");
			//if(canDriverSend(i,TxMSg,i+1)!=txSize) printf("TX FIFO is  full \n"); 
			
			canDriverSend(id++,TxMSg,TxMsgSize);
			Thread::sleep(100);
			
			char recvmsg[8];
			auto result=canDriverReceive(recvmsg,(sizeof(recvmsg)/sizeof(recvmsg[0])));
			int size=get<0>(result);
			unsigned int id=get<1>(result);
			
			//canDriverReceive(recvmsg,TxMsgSize);
			
			printf("size=%d id=%d\n",size,id);
			
			if(size>=0) 
				memDump(recvmsg,TxMsgSize);
				
			Thread::sleep(2000);
		} 
	
	
	
    
}
