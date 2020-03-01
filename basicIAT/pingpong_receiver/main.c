#include <msp430.h>
#include <stdio.h>
#include <sancus/sm_support.h>
#include <sancus_support/sm_io.h>
#include <sancus_support/tsc.h>
#include "vulcan/drivers/mcp2515.c"
#include "../CAN_interrupt/CAN_interrupt.h"

DECLARE_TSC_TIMER(timer);

#define CAN_MSG_ID		0x20
#define CAN_PAYLOAD_LEN      	4 /* max 8 */
#define RUNS		        100
#define ITERATIONS              1
#define MESG_LEN                8
uint64_t PERIOD = 10000;
uint64_t DELTA = 1000;

uint8_t msg[CAN_PAYLOAD_LEN] =	{0x12, 0x34, 0x12, 0x34};
volatile int last_time = 	0;
uint64_t timings[RUNS];
uint8_t message[RUNS];
uint64_t succesrates[ITERATIONS];
uint64_t average;
uint8_t goal_message[8] = { 1, 0, 0, 1, 1, 0, 1, 0 };
int k = 0;
int counter = RUNS+1;
uint16_t rec_id = 0x0;
uint8_t rec_msg[CAN_PAYLOAD_LEN] = {0x0};
int available = 0;
uint16_t int_counter = 0;
uint64_t interval;

// FPGA CAN interface
DECLARE_ICAN(msp_ican, 1, CAN_50_KHZ);

/* ======== UNTRUSTED CONTEXT ======== */

uint8_t decode(uint64_t timing)
{
    //pr_info1("input: %u", timing);
    if (timing > PERIOD + DELTA/2 )
    {
	//pr_info("output 1");
        return 1;
    }
    if (timing < PERIOD - DELTA/2 )
    {
	//pr_info("output 0");
        return 0;
    }
    //pr_info("output 2");
    return 2;
}

void can_callback(void)
{
    /* Store IAT */
    TSC_TIMER_END(timer);
    timings[int_counter] = timer_get_interval();
    TSC_TIMER_START(timer);
    message[int_counter] = decode(timings[int_counter]);
    int_counter = (int_counter+1)%RUNS;
    /* Clear interrupt flag */
    P1IFG = P1IFG & 0xfe;
}

int main()
{
    int overhead;
    uint64_t temp;
    int len;
    int i = 0;
    uint64_t success = 0; 
    uint64_t sum = 0;
    uint64_t stdev;
    uint8_t caninte = 0xff;

    /* SETUP */
    msp430_io_init();
    asm("eint\n\t");
    
    pr_info("Setting up CAN module...");
    ican_init(&msp_ican);
    pr_info("Done");

    pr_info("Enabling CAN interrupts...");
    /* CAN module enable interrupt */
    can_w_bit(&msp_ican, MCP2515_CANINTE,  MCP2515_CANINTE_RX0IE, 0x01);
    can_w_bit(&msp_ican, MCP2515_CANINTE,  MCP2515_CANINTE_RX1IE, 0x02);
    /* MSP P1.0 enable interrupt on negative edge */
    P1IE = 0x01;
    P1IES = 0x01;
    P1IFG = 0x00;
    pr_info("Done");

    TSC_TIMER_START(timer);

    while (k <= ITERATIONS)
    {    
	counter = RUNS; 
	int_counter = 0;
        while (counter > 0)
        {
	    ican_recv(&msp_ican, &rec_id, rec_msg, 1);
	    if (rec_id == CAN_MSG_ID)
	    {
	        /* Decode IAT */ 
	        // message[RUNS-counter] = decode(timings[RUNS-counter]);
		counter--;
	    }
        }

	/* PROCESSING */	

        /* Processing of one iteration: count correct transmissions */
        i = RUNS;
	success = 0;
        while (i>0)
        {
	    i--;
	    if (goal_message[(RUNS-i-1)%8] == message[RUNS-i])
	    {
	        success++;
            }
	 }

	/* Bookkeeping */
	succesrates[k] = success;
	k++;

	/* Processing of all iterations: calculate average & stdev */
	if (k >= ITERATIONS) 
	{
	    i = 0;
	    while (i<ITERATIONS)
	    {
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

    while (1)
    {
    }
}

CAN_ISR_ENTRY(can_callback);
