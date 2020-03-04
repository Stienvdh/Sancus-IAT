#include <msp430.h>
#include <stdio.h>
#include <sancus/sm_support.h>
#include <sancus_support/sm_io.h>
#include <sancus_support/timer.h>
#include <sancus_support/tsc.h>
#include "vulcan/drivers/mcp2515.c"

/* ======== IAT SM ======== */

DECLARE_SM(iat, 0x1234);

#define CAN_MSG_ID		0x20
#define CAN_PAYLOAD_LEN      	4
#define RUNS         		100
#define DELTA                   1000
#define PERIOD                  10000
#define ITERATIONS              1

int counter = RUNS;
uint8_t msg[CAN_PAYLOAD_LEN] =	{0x12, 0x34, 0x12, 0x34};
volatile int last_time = 	0;
uint64_t timings[RUNS];
uint8_t covert_payload[8] = { 1, 0, 0, 1, 1, 0, 1, 0 };
int payload_counter = 0;
int iterations = 0;

// FPGA CAN interface
DECLARE_ICAN(msp_ican, 1, CAN_50_KHZ);
DECLARE_TSC_TIMER(timer);

long time_to_sleep()
{
    int result = 0;

    if (covert_payload[payload_counter] == 1)
    {
	result = PERIOD + DELTA;
    }
    else 
    {
	result = PERIOD - DELTA;
    }

    payload_counter++;

    if (payload_counter >= 8) 
    {
	payload_counter = 0;
    }

    return result;
}

void timer_callback(void)
{
    int i = 0;
    timer_disable();

    /*************************************************/
    /* RUN ONE ITERATION */
    /*************************************************/
    
    if (iterations < ITERATIONS)
    { 	    
        if (counter > 0) 
        {
	    // Timer management
	    timer_irq(time_to_sleep());
            ican_send(&msp_ican, CAN_MSG_ID, msg, CAN_PAYLOAD_LEN, 0);
	    TSC_TIMER_END(timer);

	    // Bookkeeping
	    counter--;
    	    timings[counter] = timer_get_interval();
  	    TSC_TIMER_START(timer);
        }

    /*************************************************/
    /* SET UP NEXT ITERATION */
    /*************************************************/
        else 
        {
	    // For computational latency measurements
	    while (i<RUNS)
	    {
	        pr_info1("ITT: %u \n", timings[i]);
	        i++;
            }
	    
	    // Initialisation next iteration
            iterations++;
            counter = RUNS;
            payload_counter = 0;
	    i = 0;

	    // Arbitrary delay until start next iteration of transmissions
            timer_irq(20000);
        }
    }
}


int main()
{
    /*************************************************/
    /* HARDWARE SETUP */
    /*************************************************/

    msp430_io_init();
    timer_init();
    asm("eint\n\t");
    
    pr_info("Setting up CAN module...");
    ican_init(&msp_ican);
    pr_info("Done");

    pr_info("Setting up Sancus module...");
    sancus_enable(&iat);
    pr_info("Done");

    /*************************************************/
    /* IAT CHANNEL SETUP */
    /*************************************************/
    
    // Arbitrary delay until start of message transmission
    timer_irq(200);
    TSC_TIMER_START(timer);

    while (1);
}

TIMER_ISR_ENTRY(timer_callback)
