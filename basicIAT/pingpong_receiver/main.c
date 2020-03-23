#include <msp430.h>
#include <stdio.h>
#include <sancus/sm_support.h>
#include <sancus_support/sm_io.h>
#include <sancus_support/tsc.h>
#include "vulcan/drivers/mcp2515.c"

DECLARE_TSC_TIMER(timer);

#define CAN_MSG_ID		0x20
#define CAN_PAYLOAD_LEN      	4 /* max 8 */
#define RUNS		        81
#define ITERATIONS              2
#define MESG_LEN                8
#define PERIOD                  50
#define DELTA                   5

uint8_t msg[CAN_PAYLOAD_LEN] =	{0x12, 0x34, 0x12, 0x34};
volatile int last_time = 	0;
uint64_t timings[RUNS];
uint8_t message[RUNS];
uint64_t succesrates[ITERATIONS];
uint64_t average;
uint8_t goal_message[8] = { 1, 0, 0, 1, 1, 0, 1, 0 };
int k = 0;

// FPGA CAN interface
DECLARE_ICAN(msp_ican, 1, CAN_500_KHZ);

/* ======== UNTRUSTED CONTEXT ======== */

uint8_t decode(uint64_t timing)
{
     int intervals;
     
     intervals = (timing-261)/343;
     if (intervals >=  PERIOD + DELTA/2 )
     {
         return 0x1;
     }
     if (intervals <= PERIOD - DELTA/2 )
     {
         return 0x0;
     }
     return 0x2;
}

int main()
{
    int overhead;
    uint16_t rec_id = 0x0;
    uint8_t rec_msg[CAN_PAYLOAD_LEN] = {0x0};
    uint64_t temp;
    int len;
    int i = 0;
    uint64_t success = 0; 
    int counter = RUNS;
    uint64_t sum = 0;
    uint64_t stdev;
    uint8_t index;
    uint16_t mess_success;
    uint32_t it_count = 0;

    /* SETUP */
    msp430_io_init();
    asm("eint\n\t");
    
    pr_info("Setting up CAN module...");
    ican_init(&msp_ican);
    pr_info("Done");

    pr_info("Setting up Sancus module...");
    // sancus_enable(&iat);
    pr_info("Done");

    while (it_count < ITERATIONS)
    {    
	counter = RUNS; 
        TSC_TIMER_START(timer);
        while (counter > 0)
        {
	    ican_recv(&msp_ican, &rec_id, rec_msg,1);
	    if (rec_id == CAN_MSG_ID)
	    {
	        TSC_TIMER_END(timer);
	        timings[RUNS-counter] = timer_get_interval();
 	        TSC_TIMER_START(timer);
                counter--;
		// pr_info("re");
	        // Do processing here
	        message[RUNS-counter-1] = decode(timings[RUNS-counter-1]);
	    }
        } 

        /* Processing of one iteration */
        i = RUNS;
	success = 0;
	mess_success = 0;
	index = 0;
        while (i>0)
        {
	    i--;
	    if (goal_message[(RUNS-i-2)%8] == message[RUNS-i-1])
	    {
		mess_success++;
		if (index == 7 && mess_success >= 8)
		{
		     success++;
		     mess_success = 0;
		}
	        //pr_info1("%u", message[RUNS-i-1]);
		//pr_info1("%u", timings[RUNS-i-1]);
		//pr_info3("goal: %u - real: %u - time: %u ", goal_message[(RUNS-i-2)%8], message[RUNS-i-1], timings[RUNS-i-1]);
 	        //pr_info("OK");
	    }
	    else 
	    {
		mess_success = 0;
	        //pr_info1("%u", message[RUNS-i-1]);
		//pr_info1("%u", timings[RUNS-i-1]);
		//pr_info1("%u", goal_message[(RUNS-i-2)%8]);
		//pr_info3("goal: %u - real: %u - time: %u ", goal_message[(RUNS-i-2)%8], message[RUNS-i-1], timings[RUNS-i-1]);
                //pr_info("NOPE");
            }
	    index = (index+1)%8;
        }

	succesrates[k] = succesrates[k] + success;
        k++;

	if (k >= ITERATIONS) 
	{
	    it_count++;
	    //pr_info("itcount");
	    k = 0;
	}

	/* Processing of all iterations */
	if (it_count >= ITERATIONS) 
	{
	    i = 0;
	    while (i<ITERATIONS)
	    {
		pr_info1("rate: %u", succesrates[i]);
	        sum = sum + succesrates[i];
		i++;
	    }
	    average = sum/ITERATIONS;

	    sum = 0;
	    i = 0;
	    while (i<ITERATIONS)
            {
                sum = sum + (succesrates[i]-average)*(succesrates[i]-average);
                i++;
            }
            stdev = (sum*100)/ITERATIONS;

	    pr_info1("average: %u", average);
	    pr_info1("stdev (*100): %u", stdev);

	    k = 0;
	}
    }

    while(1);
}
