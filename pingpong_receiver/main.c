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

uint8_t msg[CAN_PAYLOAD_LEN] =	{0x12, 0x34, 0x12, 0x34};
volatile int last_time = 	0;
uint64_t timings[RUNS] = {0x12, 0x34, 0x12, 0x34};
int counter = RUNS;

// FPGA CAN interface
DECLARE_ICAN(msp_ican, 1, CAN_500_KHZ);

void SM_ENTRY(iat) iat_send()
{
}

/* ======== UNTRUSTED CONTEXT ======== */

int main()
{
    int overhead;
    uint16_t rec_id = 0x0;
    uint8_t rec_msg[CAN_PAYLOAD_LEN] = {0x0};
    uint64_t temp;
    int len;
    int i = 0;

    /* SETUP */
    msp430_io_init();
    asm("eint\n\t");
    
    pr_info("Setting up CAN module...");
    ican_init(&msp_ican);
    pr_info("Done");

    pr_info("Setting up Sancus module...");
    sancus_enable(&iat);
    pr_info("Done");

    while (counter > 0)
    {
	counter--;
	TSC_TIMER_START(timer);
	ican_recv(&msp_ican, &rec_id, rec_msg,1);
	TSC_TIMER_END(timer);
	if (rec_id==CAN_MSG_ID)
	{
	    timings[counter] = timer_get_interval();
        }
	rec_id = 0x0;
    } 
    
    while (i<RUNS)
    {
	pr_info1("%u ", timings[i]);
        i++;
    }

    while (1);
}
