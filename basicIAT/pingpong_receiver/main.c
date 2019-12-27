#include <msp430.h>
#include <stdio.h>
#include <sancus/sm_support.h>
#include <sancus_support/sm_io.h>
#include <sancus_support/tsc.h>
#include "vulcan/drivers/mcp2515.c"

/* ======== IAT SM ======== */

DECLARE_SM(iat, 0x1234);
DECLARE_TSC_TIMER(timer);

#define CAN_MSG_ID		0x20
#define CAN_PAYLOAD_LEN      	4
#define RUNS		        1000
#define MESG_LEN                8
#define PERIOD                  20
#define DELTA                   1

uint8_t msg[CAN_PAYLOAD_LEN] =	{0x12, 0x34, 0x12, 0x34};
volatile int last_time = 	0;
uint64_t timings[RUNS];
int counter = RUNS;
uint8_t message[RUNS];
uint8_t goal_message[8] = { 1, 0, 0, 1, 1, 0, 1, 0 };

// FPGA CAN interface
DECLARE_ICAN(msp_ican, 1, CAN_500_KHZ);

void SM_ENTRY(iat) iat_send()
{
}

/* ======== UNTRUSTED CONTEXT ======== */

uint8_t decode(uint64_t timing)
{
     int intervals;
     
     intervals = (timing-261)/343;
     if (intervals >= (PERIOD + DELTA))
     {
         return 0x1;
     }
     if (intervals <= (PERIOD - DELTA))
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
    int success = 0; 

    /* SETUP */
    msp430_io_init();
    asm("eint\n\t");
    
    pr_info("Setting up CAN module...");
    ican_init(&msp_ican);
    pr_info("Done");

    pr_info("Setting up Sancus module...");
    sancus_enable(&iat);
    pr_info("Done");

    TSC_TIMER_START(timer);
    while (counter > 0)
    {
	ican_recv(&msp_ican, &rec_id, rec_msg,1);
	TSC_TIMER_END(timer);
	timings[RUNS-counter] = timer_get_interval();
	TSC_TIMER_START(timer);
	counter--;

	// Do processing here
	message[RUNS-counter-1] = decode(timings[RUNS-counter-1]);
    } 
    
    i = RUNS;

    while (i>0)
    {
	i--;
	if (goal_message[(RUNS-i-2)%8] == message[RUNS-i-1])
	{
	    success++;
	    pr_info1("%u", timings[RUNS-i-1]);
 	    // pr_info("OK");
	}
	else 
	{
	    pr_info1("%u", timings[RUNS-i-1]);
            // pr_info("NOPE");
        }
    }

    pr_info1("SUCCES RATE: %u ", success);
    pr_info1("of %u", RUNS);

    while (1);
}
