#include "CAN_interrupt.h"

void* __isr_sp = (void*) &__isr_stack[ISR_STACK_SIZE-1];
