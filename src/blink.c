#include <avr/io.h>
#include <util/delay.h>

int main(void) {
    int status = 0;

    DDRC = _BV(PC2) | _BV(PC3);
    while(1) {
        if (status == 0) {
            PORTC = 0;
            status = 1;
        } else {
            PORTC = _BV(PC3);
            status = 0;
        }
        _delay_ms(500);
    }
}
