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

/* IAT CHANNEL VARIABLES */
uint64_t PERIOD = 10000;
uint64_t DELTA = 1000;

/* BOOKKEEPING VARIABLES */
uint8_t msg[CAN_PAYLOAD_LEN] =	{0x12, 0x34, 0x12, 0x34};
uint64_t timings[RUNS];
uint8_t message[RUNS];
uint64_t succesrates[ITERATIONS];
uint64_t average;
uint8_t goal_message[8] = { 1, 0, 0, 1, 1, 0, 1, 0 };
int k = 0; /* Amount of iterations done */
int counter = RUNS+1; /* Amount of runs within iteration to do */
uint16_t rec_id = 0x0;
uint8_t rec_msg[CAN_PAYLOAD_LEN] = {0x0};
uint16_t int_counter = 0;

// FPGA CAN interface
DECLARE_ICAN(msp_ican, 1, CAN_50_KHZ);

uint8_t decode(uint64_t timing)
{
    if (timing > PERIOD + DELTA/2 )
    {
        return 1;
    }
    if (timing < PERIOD - DELTA/2 )
    {
        return 0;
    }
    return 2;
}

void can_callback(void)
{
    // Measure + store IAT
    TSC_TIMER_END(timer);
    timings[int_counter] = timer_get_interval();
    TSC_TIMER_START(timer);

    // Decode IAT
    message[int_counter] = decode(timings[int_counter]);
    
    // Adjust message count
    int_counter = (int_counter+1)%RUNS;

    // Clear interrupt flag on MSP430
    P1IFG = P1IFG & 0xfc;
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
    uint8_t data = 0x00;

    // mask + filter 
    uint8_t mask_h = 0xff;
    uint8_t mask_l = 0xe0;
    uint8_t filter_h = 0x04;
    uint8_t filter_l = 0x00;

    /*************************************************/
    /* HARDWARE SETUP */
    /*************************************************/
    
    /* SET UP MSP430 */

    msp430_io_init();
    asm("eint\n\t");
    
    /* SET UP CAN CONTROLLER */

    pr_info("Setting up CAN module...");
    ican_init(&msp_ican);

    // Enter configuration mode
    data = MCP2515_CANCTRL_REQOP_CONFIGURATION;
    can_w_reg(&msp_ican, MCP2515_CANCTRL, &data, 1);

    // Zero-initialize some registers to be sure
    data = 0x0;
    can_w_reg(&msp_ican, MCP2515_TXRTSCTRL, &data, 1);
    can_w_reg(&msp_ican, MCP2515_BFPCTRL, &data, 1);

    // Receive only valid messages with standard/extended identifiers
    data = MCP2515_RXB0CTRL_MODE_RECV_STD_OR_EXT;
    can_w_reg(&msp_ican, MCP2515_RXB0CTRL, &data, 1);
    data = MCP2515_RXB1CTRL_MODE_RECV_STD_OR_EXT;
    can_w_reg(&msp_ican, MCP2515_RXB1CTRL, &data, 1);

    // Set RXB0 mask and filter
    can_w_reg(&msp_ican, MCP2515_RXM0SIDH, &mask_h, 1);
    can_w_reg(&msp_ican, MCP2515_RXM0SIDL, &mask_l, 1);
    can_w_reg(&msp_ican, MCP2515_RXF0SIDH, &filter_h, 1);
    can_w_reg(&msp_ican, MCP2515_RXF0SIDL, &filter_l, 1);

    // Set RXB1 mask and filter
    can_w_reg(&msp_ican, MCP2515_RXM1SIDH, &mask_h, 1);
    can_w_reg(&msp_ican, MCP2515_RXM1SIDL, &mask_l, 1);
    can_w_reg(&msp_ican, MCP2515_RXF2SIDH, &filter_h, 1);
    can_w_reg(&msp_ican, MCP2515_RXF2SIDL, &filter_l, 1);

    // Go back to normal mode
    data = MCP2515_CANCTRL_REQOP_NORMAL;
    can_w_reg(&msp_ican, MCP2515_CANCTRL, &data, 1);
    
    pr_info("Done");

    /* SET UP CAN INTERRUPTS */

    pr_info("Enabling CAN interrupts...");
    
    // CAN module enable interrupt
    can_w_bit(&msp_ican, MCP2515_CANINTE,  MCP2515_CANINTE_RX0IE, 0x01);
    can_w_bit(&msp_ican, MCP2515_CANINTE,  MCP2515_CANINTE_RX1IE, 0x02);
    
    // MSP P1.0 enable interrupt on negative edge
    P1IE = 0x01;
    P1IES = 0x01;
    P1IFG = 0x00;
    
    pr_info("Done");

    /*************************************************/
    /* IAT CHANNEL */
    /*************************************************/
    
    TSC_TIMER_START(timer);

    while (k <= ITERATIONS)
    {    
	counter = RUNS; 
	int_counter = 0;

	/* BLOCKING MESSAGE RECEIVING */

        while (counter > 0)
        {
	    ican_recv(&msp_ican, &rec_id, rec_msg, 1);
	    if (rec_id == CAN_MSG_ID)
	    {
		counter--;
	    }
	    else 
	    {
		pr_info("This should not happen!");
	    }
        }

    /*************************************************/
    /* IAT CHANNEL RELIABILITY MEASUREMENTS */
    /*************************************************/

        /* Processing of ONE iteration */
       
	// count correct transmissions
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

	// bookkeeping
	succesrates[k] = success;
	k++;

	/* Processing of ALL iterations */

	if (k >= ITERATIONS) 
	{
	    // Average
	    i = 0;
	    while (i<ITERATIONS)
	    {
	        sum = sum + succesrates[i];
		i++;
	    }
	    average = sum/ITERATIONS;

	    // Standard deviation
	    sum = 0;
	    i = 0;
	    while (i<ITERATIONS)
            {
                sum = sum + (succesrates[i]-average)*(succesrates[i]-average);
                i++;
            }
            stdev = (sum*100)/ITERATIONS;
	    
	    // Print results to output
	    pr_info1("average: %u", average);
	    pr_info1("stdev (*100): %u", stdev);

	    k = 0;
	}
    }

    // Just in case
    while (1)
    {
    }
}

CAN_ISR_ENTRY(can_callback);
