#include <avr/io.h>
#include <stdint.h>
#include <util/delay.h>
#include <avr/interrupt.h>
#include <avr/eeprom.h>
#include <avr/sleep.h>
#include <avr/pgmspace.h>

#include "I2CSlave_state_machine.h"
#include "avr_uart.h"

#if !defined(DEBUG)
    #define DEBUG 1 // If unspecified, activate DEBUG by  default
#endif

#if DEBUG
    #define dbg(x, ...) printf_P(PSTR(x) ,##__VA_ARGS__)
#else
    #define dbg(x, ...)
#endif

#define FIRMWARE_VERSION 0x01 // v0.1

#define LED_K PC2
#define LED_K_PCINT PCINT10
#define LED_A PC3
#define LED_DDR DDRC
#define LED_PORT PORTC

#define POWER_PIN PB2
#define POWER_DDR DDRB
#define POWER_PORT PORTB

#define EXCITATION_PIN PD6
#define EXCITATION_DDR DDRD
#define EXCITATION_PORT PORTD

// NB : CAP_H and CAP_L tracks are inverted on the schematics / PCB
#define CHANNEL_CAPACITANCE_HIGH   1
#define CHANNEL_CAPACITANCE_LOW    0
#define CHANNEL_CHIP_TEMP 0b00001000

#define I2C_BUFFER_SIZE            4

#define I2C_WAKEUP                 0x00
#define I2C_GET_CAPACITANCE        0x01
#define I2C_GET_LIGHT              0x02
#define I2C_RESET                  0x03
#define I2C_GET_VERSION            0x04
#define I2C_SLEEP                  0x05
// #define I2C_SET_ADDRESS         0x06
#define I2C_DEBUG_ENABLE_ADC       0x80
#define I2C_DEBUG_POWER_ON         0x81
#define I2C_DEBUG_SET_EXCIT_FREQ   0x82
#define I2C_DEBUG_START_EXCITATION 0x83
#define I2C_DEBUG_CAP_MEASUREMENT  0x84
#define I2C_DEBUG_STOP_EXITATION   0x85
#define I2C_DEBUG_POWER_OFF        0x86
#define I2C_DEBUG_DISABLE_ADC      0x87


#define I2C_NONE                   0xFF

#define I2C_ADDRESS_EEPROM_LOCATION (uint8_t*)0x01
#define I2C_ADDRESS_BASE        0x20
#ifndef I2C_FREQUENCY
    #define I2C_FREQUENCY       100000 // bauds
#endif

#define MAIN_LOOP_PERIOD_SECONDS   5

// -------------------- LED Management --------------------

inline static void ledSetup(){
    LED_DDR |= _BV(LED_A) | _BV(LED_K);
    LED_PORT &= ~(_BV(LED_A) | _BV(LED_K));
}

inline static void ledOn() {
    LED_PORT |= _BV(LED_A);
}

inline static void ledOff() {
    LED_PORT &= ~_BV(LED_A);
}

inline static void ledToggle() {
    LED_PORT ^= _BV(LED_A);
}

inline static void powerOn() {
	POWER_DDR |= _BV(POWER_PIN);
	POWER_PORT |= _BV(POWER_PIN);
}

inline static void powerOff() {
	POWER_PORT &= ~_BV(POWER_PIN);
}

// -------------------- light measurement -------------------

uint16_t light = 0;

volatile uint16_t lightCounter = 0;
volatile uint8_t lightCycleOver = 1;

static inline void stopLightMeasurement() {
    PCICR &= ~_BV(PCIE1);       // disable PC interrupt
    TCCR1B = 0;                 // reset config
    PCMSK1 &= ~_BV(PCINT10);    // remove PC interrupt mask
    TIMSK1 &= ~_BV(TOIE1);      // disable timer overflow interrupt

    lightCycleOver = 1;
}

ISR(PCINT1_vect) {
    lightCounter = TCNT1;
    stopLightMeasurement();
}

ISR(TIMER1_OVF_vect) {
    lightCounter = 65535;
    stopLightMeasurement();
}

#if defined(STUB_MEASUREMENT_FUNC)
static inline uint16_t getLight() {return 0xBB66;}
#else
static inline uint16_t getLight() {
    PRR &= ~_BV(PRTIM1);                    // disable PowerReduction for Timer1

    TIMSK1 |= _BV(TOIE1);                   // enable timer overflow interrupt

    LED_DDR |= _BV(LED_A) | _BV(LED_K);     // forward bias the LED
    LED_PORT &= ~_BV(LED_K);                // flash it to discharge the PN junction capacitance
    LED_PORT |= _BV(LED_A);

    LED_PORT |= _BV(LED_K);                 // reverse bias LED to charge capacitance in it
    LED_PORT &= ~_BV(LED_A);
    _delay_us(100);
    LED_DDR &= ~_BV(LED_K);                 // make Cathode input
    LED_PORT &= ~(_BV(LED_A) | _BV(LED_K)); // disable pullups

    TCNT1 = 0;                              // reset counter
    TCCR1A = 0;                             // normal operation (OC1A/B disconnected)

    PCMSK1 |= _BV(LED_K_PCINT);                 // enable pin change interrupt on LED_K
    PCICR |= _BV(PCIE1);

    lightCycleOver = 0;
    TCCR1B = _BV(CS10) | _BV(CS11);         // start Timer1 with prescaler clk/64

    // go to sleep until processed
    while (!lightCycleOver) {
        set_sleep_mode(SLEEP_MODE_IDLE);
        sleep_mode();
    }

    return lightCounter;
}
#endif


// -------------------- ADC Management --------------------

inline static void adcSetup() {
    // setting ADPS0..2 == 0b111 applies a 128 factor
    // Given F_CPU is 16MHz, it gives a 125KHz freq
    // Datasheets advices for 50..200KHz
    ADCSRA = _BV(ADPS2) | _BV(ADPS1) | _BV(ADPS0) | _BV(ADIE);
    ADMUX = 0;
}

inline static void enableADC() {
    ADCSRA |= _BV(ADEN);
}

inline static void disableADC() {
    ADCSRA &= ~_BV(ADEN);
}

inline static void sleepWhileADC() {
    while(!(ADCSRA & ADIF)) {}
}

uint16_t adcReadChannel(uint8_t channel) {
    // Conversion on `channel` using AVcc as Vref
    ADMUX = _BV(REFS0) | channel;

    ADCSRA |= _BV(ADSC);
    sleepWhileADC();
    // First conversion after mux switch is discarded

    ADCSRA |= _BV(ADSC);
    sleepWhileADC();

    return ADC;
}



// -------------------- Capacitance Measurement --------------------

uint16_t capacitance = 0;

bool capMeasurementInProgress = false;
bool excitation_enabled = false;

uint8_t excitation_freq_index // = 3; // default 1MHz
                              = 5;

// TODO : recheck frequencies
uint8_t ocr0a_values[] = {
    39, // 100 kHz  measured: 51.50 kHz (50k)
    15, // 250 kHz  measured: 133.8 kHz (133.3k)
     7, // 500 kHz  measured: 297.4 kHz (285.7k)
     4, // 800 kHz  measured: 502   kHz (500k)
     3, // 1   MHz  measured: 670   kHz (666k)
     1, // 2   MHz  measured: 2     MHz (2M)
     0  // 4   MHz  measured: 4     MHz (4M)
};

static inline void excitationEnable() {
    // Disable Power Reduction for Timer0
    PRR &= ~_BV(PRTIM0);

    // OC0A as output
    EXCITATION_DDR |= _BV(EXCITATION_PIN);
    EXCITATION_PORT &= ~_BV(EXCITATION_PIN);
    // Reset counter
    TCNT0 = 0;
    // Setup TOP value for WGM
    OCR0A = ocr0a_values[excitation_freq_index];

    // Phase correct PWM (mode5), toggling, no frequency prescale
    TCCR0A = _BV(COM0A0) | _BV(WGM00);
    TCCR0B = _BV(WGM02) | _BV(CS00);

    excitation_enabled = true;
}

static inline void excitationDisable() {
    excitation_enabled = false;

    // Stop Timer0
    TCCR0A = 0;
    TCCR0B = 0;
    // reset excitation pin state
    EXCITATION_PORT &= ~_BV(EXCITATION_PIN);
    EXCITATION_DDR &= ~_BV(EXCITATION_PIN);
    // Reset counter
    TCNT0 = 0;

    // Enable Power Reduction for Timer0
    PRR |= _BV(PRTIM0);
}

#if defined(STUB_MEASUREMENT_FUNC)
uint16_t getCapacitance() {return 0xAA55;}
#else
uint16_t getCapacitance() {
    uint16_t caph, capl, result;

    capMeasurementInProgress = true;
    enableADC();
    powerOn();
    excitationEnable();
    _delay_ms(5); // useful for CAP_H/L stabilisation

    caph = adcReadChannel(CHANNEL_CAPACITANCE_HIGH);
    capl = adcReadChannel(CHANNEL_CAPACITANCE_LOW);

    excitationDisable();
    powerOff();
    disableADC();
    capMeasurementInProgress = false;

    result = 1023 - (caph - capl);
    dbg("getCapacitance() caph=%u, capl=%u, result=%u\n", caph, capl, result);
    return result;
}
#endif



// -------------------- WatchDog & Reset --------------------

bool reset_requested = false;

/* NOTE: Watchdog is only used to programtically reset the chip, as for now */
inline static void wdt_disable() {
    MCUSR = 0;
    WDTCSR &= ~_BV(WDE);
}

inline static void wdt_enable() {
    WDTCSR = _BV(WDE);
}

// TODO: This is not the valid checking !
// static inline bool twiIsValidAddress(unsigned char adr) {
//     return (adr >= I2C_ADDRESS_BASE) && (adr < I2C_ADDRESS_BASE + 8);
// }

#define reset() {wdt_enable(); while(1) {}}


// -------------------- Power Saving --------------------

bool sleep_requested = false;
bool is_woke = false;

static inline void setupPowerSaving() {
    //shut down everything we don't use
    PRR = _BV(PRTIM0) | _BV(PRTIM1) | _BV(PRTIM2) | _BV(PRSPI)
#if ! DEBUG
        | _BV(PRUSART0)
#endif
        ;
    ACSR = _BV(ACD); //disable comparators
    // DIDR0 = _BV(ADC0D) | _BV(ADC1D); //disable input buffers for analog pins
}

void wakeup() {
    ledOn();
    _delay_ms(100);
    light = getLight();
    capacitance = getCapacitance();
    is_woke = true;
}


// -------------------- I²C commands --------------------

uint8_t i2c_buffer[I2C_BUFFER_SIZE];

uint8_t i2c_wakeup(uint8_t *buffer, uint8_t buffer_len) {
    dbg("I²C: WAKEUP\n");
    wakeup();
    return 0;
}

uint8_t i2c_get_capacitance(uint8_t *buffer, uint8_t buffer_len) {
    dbg("I²C: GET CAPACITANCE: %d\n", capacitance);
    buffer[0] = (capacitance & 0xFF00) >> 8;
    buffer[1] = (capacitance & 0x00FF);
    return sizeof(uint16_t);
}

uint8_t i2c_get_light(uint8_t *buffer, uint8_t buffer_len) {
    dbg("I²C: GET LIGHT : %d\n", light);
    buffer[0] = (light & 0xFF00) >> 8;
    buffer[1] = (light & 0x00FF);
    return sizeof(uint16_t);
}

uint8_t i2c_get_version(uint8_t *buffer, uint8_t buffer_len) {
    dbg("I²C: GET VERSION: 0x%x\n", FIRMWARE_VERSION);
    buffer[0] = FIRMWARE_VERSION;
    return sizeof(uint8_t);
}

uint8_t i2c_reset(uint8_t *buffer, uint8_t buffer_len) {
    dbg("I²C: RESET\n");
    reset_requested = true;
    return 0;
}

uint8_t i2c_sleep(uint8_t *buffer, uint8_t buffer_len) {
    dbg("I²C: SLEEP\n");
    sleep_requested = true;
    return 0;
}


// -------------------- I²C DEBUG commands --------------------
#if DEBUG

uint8_t i2c_debug_enable_adc(uint8_t *buffer, uint8_t buffer_len) {
    dbg("DBG : ENABLE ADC\n");
    enableADC();
    return 0;
}

uint8_t i2c_debug_power_on(uint8_t *buffer, uint8_t buffer_len) {
    dbg("DBG : POWER ON\n");
    powerOn();
    return 0;
}

#if DEBUG
    char* frequencies_txt[] = {
        "100  kHz",
        "250  kHz",
        "500  kHz",
        "800  kHz",
        "1    MHz",
        "2    MHz",
        "4    MHz"
    };
#endif

uint8_t i2c_debug_set_exitation_freq(uint8_t *buffer, uint8_t buffer_len) {
    if (buffer[0] < sizeof(ocr0a_values)) {
        dbg("DBG : SET EXCITATION FREQ (index: %d, OCR0A:%d, freq: %s)\n",
            buffer[0],
            ocr0a_values[buffer[0]],
            frequencies_txt[buffer[0]]);
        bool is_exc_running = excitation_enabled;
        if (is_exc_running) excitationDisable();
        excitation_freq_index = buffer[0];
        // restart if need
        if (is_exc_running) excitationEnable();
    } else {
        dbg("DBG: SET EXCITATION FREQ: index %d is INVALID !\n");
    }
    return 0;
}

uint8_t i2c_debug_start_exitation(uint8_t *buffer, uint8_t buffer_len) {
    dbg("DBG : START EXCITATION\n");
    excitationEnable();
    return 0;
}

uint8_t i2c_debug_cap_measurement(uint8_t *buffer, uint8_t buffer_len) {
    dbg("DBG : CAPACITIVE MEASUREMENT\n");
    enableADC();
    TWCR |= (1<<TWINT);
    sei();
    getCapacitance();
    cli();
    disableADC();
    // _delay_ms(10); // useful ?
    // capMeasurementInProgress = 1;
    // uint16_t caph = adcReadChannel(CHANNEL_CAPACITANCE_HIGH);
    // uint16_t capl = adcReadChannel(CHANNEL_CAPACITANCE_LOW);
    // capMeasurementInProgress = 0;
    // dbg("** Read capacitance : %d (capH=%d, capL=%d)\n", 1023 - (caph - capl), caph, capl);
    return 0;
}

uint8_t i2c_debug_stop_exitation(uint8_t *buffer, uint8_t buffer_len) {
    dbg("DBG : STOP EXITATION\n");
    excitationDisable();
    return 0;
}

uint8_t i2c_debug_power_off(uint8_t *buffer, uint8_t buffer_len) {
    dbg("DBG : POWER OFF\n");
    powerOff();
    return 0;
}

uint8_t i2c_debug_disable_adc(uint8_t *buffer, uint8_t buffer_len) {
    dbg("DBG : DISABLE ADC\n");
    disableADC();
    return 0;
}

#endif


i2c_slaveSM_command_t commands[] = {
    {I2C_WAKEUP,                 0, i2c_wakeup},
    {I2C_GET_CAPACITANCE,        0, i2c_get_capacitance},
    {I2C_GET_LIGHT,              0, i2c_get_light},
    {I2C_RESET,                  0, i2c_reset},
    {I2C_GET_VERSION,            0, i2c_get_version},
    {I2C_SLEEP,                  0, i2c_sleep},
#if DEBUG
    {I2C_DEBUG_ENABLE_ADC,       0, i2c_debug_enable_adc},
    {I2C_DEBUG_POWER_ON,         0, i2c_debug_power_on},
    {I2C_DEBUG_SET_EXCIT_FREQ,   1, i2c_debug_set_exitation_freq},
    {I2C_DEBUG_START_EXCITATION, 0, i2c_debug_start_exitation},
    {I2C_DEBUG_CAP_MEASUREMENT,  0, i2c_debug_cap_measurement},
    {I2C_DEBUG_STOP_EXITATION,   0, i2c_debug_stop_exitation},
    {I2C_DEBUG_POWER_OFF,        0, i2c_debug_power_off},
    {I2C_DEBUG_DISABLE_ADC,      0, i2c_debug_disable_adc},
#endif
};


// -------------------- Main --------------------

int main (void) {
    wdt_disable();

    avr_uart_init();
    stdout = &avr_uart_output;
    stdin  = &avr_uart_input_echo;

    uint8_t address = eeprom_read_byte(I2C_ADDRESS_EEPROM_LOCATION);
    // if(!twiIsValidAddress(address)) {
    if(address == 0xFF) {
        // EEPROM was not programmed, use default address
        address = I2C_ADDRESS_BASE;
    }

    dbg("I²C moisture sensor, address = 0x%x\n", address);
    dbg("Excitation frequency: %s\n", frequencies_txt[excitation_freq_index]);


    dbg("Set power saving params...\n");
    setupPowerSaving();
    dbg("Setup LED params...\n");
    ledSetup();
    dbg("Setup ADC params...\n");
    adcSetup();
    // powerOn();
    sei();

    dbg("Wakeup...\n");
    wakeup();

    dbg("Setup i²c...\n");
    i2c_slaveSM_init(address, I2C_FREQUENCY,
                     commands, sizeof(commands)/sizeof(i2c_slaveSM_command_t),
                     i2c_buffer, I2C_BUFFER_SIZE);

    dbg("Enter loop.\n");
    while(1) {
        dbg("Loop...\n");

        if (is_woke) {
            dbg("start measurements...\n");
            ledOff();
            light = getLight();
            capacitance = getCapacitance();
            ledOn();
            dbg("measurements done (cap=%u ; light=%u).\n", capacitance, light);
        }

        if (sleep_requested) {
            ledOff();
            is_woke = false;
            set_sleep_mode(SLEEP_MODE_PWR_DOWN);
            sleep_mode();
        }

        if (reset_requested) {
            reset_requested = false;
            reset();
        }

        _delay_ms(1000 * MAIN_LOOP_PERIOD_SECONDS);
    }
}
