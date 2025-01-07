#include <stdio.h>

#pragma once

#define UART_PORT UART_NUM_0
#define TXD_PIN (GPIO_NUM_1)
#define RXD_PIN (GPIO_NUM_3)
static const int RX_BUF_SIZE = 1024;

void init(void);

void rx_task(void *arg);

void handle_data(char* input);