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
#define ITERATIONS              1000
#define MESG_LEN                8
#define PERIOD                  50
#define DELTA                   2

uint8_t msg[CAN_PAYLOAD_LEN] =	{0x12, 0x34, 0x12, 0x34};
volatile int last_time = 	0;
uint64_t timings[RUNS];
uint8_t message[RUNS];
uint64_t succesrates[ITERATIONS];
uint64_t average;
uint8_t goal_message[8] = { 1, 0, 0, 1, 1, 0, 1, 0 };
int k = 0;

// FPGA CAN interface
DECLARE_ICAN(msp_ican, 1, CAN_50_KHZ);

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

void can_callback(void)
{
    pr_info("INTERRUPT");
    {
	P1IFG = P1IFG & 0xfe;
	can_w_reg(&msp_ican, MCP2515_CANINTF, 0x00, 1);
        can_dump_regs(&msp_ican);
    }
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
    uint8_t caninte = 0xff;

    /* SETUP */
    msp430_io_init();
    asm("eint\n\t");
    
    pr_info("Setting up CAN module...");
    ican_init(&msp_ican);
    pr_info("Done");

    pr_info("Enabling CAN interrupts...");
    /* CAN module enable interrupt */
    can_dump_regs(&msp_ican);
    can_w_bit(&msp_ican, MCP2515_CANINTE,  MCP2515_CANINTE_RX0IE, 0x01);
    can_w_bit(&msp_ican, MCP2515_CANINTE,  MCP2515_CANINTE_RX1IE, 0x02);
    // can_w_bit(&msp_ican, MCP2515_CANINTE,  MCP2515_CANINTE_MERRE, 0x80);
    pr_info("before IE");
    P1IE = 0x1;
    pr_info("after IE");
    P1IES = 0x01;
    P1IFG = 0x00;
    pr_info("Done");
    can_dump_regs(&msp_ican);

    pr_info("Setting up Sancus module...");
    // sancus_enable(&iat);
    pr_info("Done");

    ican_send(&msp_ican, CAN_MSG_ID, msg, CAN_PAYLOAD_LEN, 0);
    pr_info("Sent");

    while (1)
    {    
	counter = RUNS; 
        // enable when using TSC_TIMER_START(timer);
        while (counter > 0)
        {
	    ican_recv(&msp_ican, &rec_id, rec_msg,1);
	    pr_info("received message");
	    if (rec_id == CAN_MSG_ID)
	    {
	        TSC_TIMER_END(timer);
	        timings[RUNS-counter] = timer_get_interval();
 	        TSC_TIMER_START(timer);
                counter--;
	        // Do processing here
	        message[RUNS-counter-1] = decode(timings[RUNS-counter-1]);
	    }
        } 

        /* Processing of one iteration */
        i = RUNS;
	success = 0;
        while (i>0)
        {
	    i--;
	    if (goal_message[(RUNS-i-2)%8] == message[RUNS-i-1])
	    {
	        success++;
	        // pr_info1("%u", message[RUNS-i-1]);
 	        // pr_info("OK");
	    }
	    else 
	    {
	        // pr_info1("%u", message[RUNS-i-1]);
                // pr_info("NOPE");
            }
        }

	succesrates[k] = success;
        k++;

	if (k%100 == 0) 
	{
	    pr_info1("runs done: %u", k);
	}

	/* Processing of all iterations */
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
