
#include "main.h"
#include "stdio.h"
#include "usart.h"

static UART_HandleTypeDef *uart = &huart2;

#pragma import(__use_no_semihosting)
// __asm(".global __use_no_semihosting\n\t");   //AC6

void _sys_exit(int x)
{

    x = x;
}

void _ttywrch(int ch)
{
    ch = ch;
}

struct __FILE
{

    int handle;
};


FILE __stdout;

void print_char(char c)
{
    HAL_UART_Transmit(uart, (uint8_t *) (&c), 1, 1);
}

int fputc(int ch, FILE *f)
{
    // uint8_t temp[1]={ch};
    if (ch == '\n') {
        print_char('\r');
    }

    print_char(ch);
    
    return 0;
}
