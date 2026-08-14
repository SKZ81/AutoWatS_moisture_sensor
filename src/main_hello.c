#include <avr/io.h>
#include <stdio.h>
#include <avr/eeprom.h>

#include "uart/uart.h"
#ifndef BUFFER_SIZE
#define BUFFER_SIZE 128
#endif

#define I2C_ADDRESS_EEPROM_LOCATION (uint8_t*)0x01

int main (void) {
    avr_uart_init();
    stdout = &avr_uart_output;
    stdin  = &avr_uart_input_echo;

    uint8_t address = eeprom_read_byte(I2C_ADDRESS_EEPROM_LOCATION);

    printf("I²C Moisture Sensor says : \"Hello, world ! I'm sensor 0x%x\"\n", address);

    char buf[BUFFER_SIZE];

    while(1) {
        printf("What do YOU say ? ");
        if (fgets(buf, BUFFER_SIZE, stdin) != NULL) {
            printf("\nSo, you're saying : %s\n", buf);
        } else {
            printf("\nError reading input...\n");
        }
    }
}
