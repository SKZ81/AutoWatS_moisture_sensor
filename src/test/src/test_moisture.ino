#include "config.h"
#include <Wire.h>
// #include "../../scale_i2c_interface.h"
// #include <string.h>
// #include <stdlib.h>

typedef struct {
    uint8_t code;
    uint8_t arg_len;
    void (*read_callback) (uint8_t*);
//     uint8_t (*callback)(uint8_t*, uint8_t);
    uint8_t reply_len;
} command_t;


void no_read(uint8_t *buffer) {
    Serial.println("No arg needed...");
}

void read_freq(uint8_t *buffer) {
    long l = -1;
    Serial.println(F("Select new excitation frequency:"));
    Serial.println(F(" 0 - 100  kHz"));
    Serial.println(F(" 1 - 250  kHz"));
    Serial.println(F(" 2 - 500  kHz"));
    Serial.println(F(" 3 - 800  kHz"));
    Serial.println(F(" 4 - 1    MHz"));
    Serial.println(F(" 5 - 2    MHz"));
    Serial.println(F(" 6 - 4    MHz"));
    while (l<0 || l>6) {
        Serial.print(F("Enter uint8_t value ([0..5], decimal base) : "));
        l = Serial.readString().toInt();
    }
    Serial.print(F("Read value : "));
    Serial.println(l, DEC);
    buffer[0] = uint8_t(l&0xFF);
}

void read_uint8(uint8_t *buffer) {
    long l = -1;
    while (l<0 || l>255) {
        Serial.print(F("Enter uint8_t value ([0..255], decimal base) : "));
        l = Serial.readString().toInt();
    }
    Serial.print(F("Read value : "));
    Serial.println(l, DEC);
    buffer[0] = uint8_t(l&0xFF);
}

void read_i2c_addr(uint8_t *buffer) {
    long l = -1;
    while (l<1 || l>127) {
        Serial.print(F("Enter I²C adress ([1..127], decimal base) : "));
        l = Serial.readString().toInt();
    }
    Serial.print(F("I²C addr : 0x"));
    Serial.println(l, HEX);
    Serial.print(F(" ("));
    Serial.println(l, DEC);
    Serial.print(F(")"));
    buffer[0] = uint8_t(l&0xFF);
}

void read_uint24(uint8_t *buffer) {
    long l = -9000000;
    while (l<-8388608 || l>8388607) {
        Serial.print(F("Enter int24_t value ([-8388608..8388607], decimal base) : "));
        l = Serial.readString().toInt();
    }
    Serial.print(F("Read value : "));
    Serial.println(l, DEC);
    buffer[0] = (uint8_t)((l&0x00FF0000)>>16);
    buffer[1] = (uint8_t)((l&0x0000FF00)>>8);
    buffer[2] = (uint8_t)((l&0x000000FF));
}

void read_float(uint8_t *buffer) {
    float f = 0.0;
    Serial.print(F("Enter a float : "));
    f = Serial.readString().toFloat();
    Serial.print(F("Read value : "));
    Serial.println(f, 6);
    memcpy(buffer, &f, sizeof(float));
}

// void read_freq(uint8_t *buffer) {
//     Serial.println("Select Excitation Frequency:");
//     Serial.println("0 - 100  kHz");
//     Serial.println("1 - 250  kHz");
//     Serial.println("2 - 500  kHz");
//     Serial.println("3 - 1    MHz");
//     Serial.println("4 - 2    MHz");
//     Serial.println("5 - 4    MHz");
//     while (l<0 || l>5) {
//         Serial.print("Enter value ([0..5]) : ");
//         l = Serial.readString().toInt();
//     }
//     Serial.print("Read value : ");
//     Serial.println(l, DEC);
//     buffer[0] = uint8_t(l&0xFF);
// }
#define I2C_GET_CAPACITANCE        0x00
#define I2C_SET_ADDRESS            0x01
#define I2C_GET_LIGHT              0x04
#define I2C_RESET                  0x06
#define I2C_GET_VERSION            0x07
#define I2C_SLEEP                  0x08
#define I2C_WAKEUP                 0x10

#define I2C_DEBUG_ENABLE_ADC       0x80
#define I2C_DEBUG_POWER_ON         0x81
#define I2C_DEBUG_SET_EXCIT_FREQ   0x82
#define I2C_DEBUG_START_EXCITATION 0x83
#define I2C_DEBUG_CAP_MEASUREMENT  0x84
#define I2C_DEBUG_STOP_EXITATION   0x85
#define I2C_DEBUG_POWER_OFF        0x86
#define I2C_DEBUG_DISABLE_ADC      0x87


command_t commands[] = {
    {I2C_WAKEUP,                 0, no_read,       0},
    {I2C_GET_CAPACITANCE,        0, no_read,       2},
    {I2C_GET_LIGHT,              0, no_read,       2},
    {I2C_RESET,                  0, no_read,       0},
    {I2C_GET_VERSION,            0, no_read,       1},
    {I2C_SLEEP,                  0, no_read,       0},
    {I2C_SET_ADDRESS,            1, read_i2c_addr, 0},
    {I2C_DEBUG_ENABLE_ADC,       0, no_read,       0},
    {I2C_DEBUG_POWER_ON,         0, no_read,       0},
    {I2C_DEBUG_SET_EXCIT_FREQ,   1, read_freq,     0},
    {I2C_DEBUG_START_EXCITATION, 0, no_read,       0},
    {I2C_DEBUG_CAP_MEASUREMENT,  0, no_read,       0},
    {I2C_DEBUG_STOP_EXITATION,   0, no_read,       0},
    {I2C_DEBUG_POWER_OFF,        0, no_read,       0},
    {I2C_DEBUG_DISABLE_ADC,      0, no_read,       0}
};




void setup(void) {
    Serial.begin(115200);
    Serial.println(F("I2C master, for testing scale"));
    Serial.setTimeout(5000);
    Wire.begin(I2C_PINS);

#if defined(ARDUINO_ARCH_ESP8266)
    // useless on Arduino Uno/Nano, unavaible on esp32, but necessary for esp82266
    Wire.setClockStretchLimit(400000);
#endif
}

uint8_t sensor_address = 0xff;

void loop(void) {
    uint8_t buffer[5] = {0};

    Serial.println();
    if (sensor_address > 127)
    Serial.println(F(" * - Change sensor address"));
    Serial.println(F("__________________________"));
    Serial.print  (F("**** Sensor address : 0x"));
    Serial.println(sensor_address, HEX);
    Serial.println(F(" 0 - WAKEUP"));
    Serial.println(F(" 1 - GET_CAPACITANCE"));
    Serial.println(F(" 2 - GET_LIGHT"));
    Serial.println(F(" 3 - RESET"));
    Serial.println(F(" 4 - GET_VERSION"));
    Serial.println(F(" 5 - SLEEP"));
    Serial.println(F(" 7 - SET ADDRESS"));
    Serial.println(F(" ---- Debug ----"));
    Serial.println(F(" A - ENABLE ADC"));
    Serial.println(F(" B - POWER ON"));
    Serial.println(F(" C - SET EXCITATION FREQ"));
    Serial.println(F(" D - START EXCITATION"));
    Serial.println(F(" E - READ CAPACITANCE"));
    Serial.println(F(" F - STOP EXCITATION"));
    Serial.println(F(" G - POWER OFF"));
    Serial.println(F(" H - DISABLE ADC"));
    Serial.println();
    Serial.print(F("Enter choice : "));
    while(!Serial.available()) {}
    char char_cmd = toupper(Serial.read());

    uint8_t cmd = 0xFF;

    if (char_cmd == '*') {
        read_i2c_addr(&sensor_address);
        return;
    }

    if (char_cmd >= '0' && char_cmd <= '5') {
        cmd = char_cmd - '0';
    }
    if (char_cmd >= 'A' && char_cmd <= 'H') {
        cmd = char_cmd - 'A' + 6;
    }

    Serial.print(char_cmd);
    Serial.print(F(" => "));
    Serial.print(cmd);

    if (cmd == 0xFF) {
        Serial.print(F(">>> Unknown command "));
        Serial.println(char_cmd);
        return; // next loop()
    } else {
        Serial.println();
    }

    if (sensor_address == 0 || sensor_address > 127) {
        Serial.println(F("Sensor address is invalid, please set a valid address."));
        return;
    }
    buffer[0] = commands[cmd].code;
    commands[cmd].read_callback(buffer+1);

    Serial.print(F(">>> I2C communication, will send command_code: "));
    Serial.print(commands[cmd].code, DEC);
    Serial.print(F(", with "))  ;
    Serial.print(commands[cmd].arg_len, DEC);
    Serial.print(F(" bytes of data = [ "));
    for(uint8_t i=0; i<commands[cmd].arg_len; i++) {
        Serial.print(buffer[i+1], HEX);
        Serial.print(F(" "));
    }

    Serial.println(F("]"));
    Serial.print(F(">>> Reply ("));
    Serial.print(commands[cmd].reply_len);
    Serial.print(F(" expected...)"));

    Wire.beginTransmission(sensor_address);
//     Wire.write((uint8_t)commands[cmd].code);
//     for(uint8_t i=0; i<commands[cmd].arg_len; i++) {
//         Wire.write((uint8_t)buffer[i]);
//     }
    Wire.write(buffer, commands[cmd].arg_len+1);
    int ret = 0;
    if (commands[cmd].reply_len == 0) {
        // nothing to read, free the bus
        ret = Wire.endTransmission( /*true*/ );
        Serial.print(F(" ...done : "));
        Serial.println(ret);
    } else {
        ret = Wire.endTransmission( /*false*/ ); // ESP8266 doesn't support repeated start
        Serial.print(F(" ("));
        Serial.print(ret);
        Serial.print(F(": [ "));
        Wire.requestFrom(sensor_address, (uint8_t)commands[cmd].reply_len);
        while(Wire.available()) {
            uint8_t result = Wire.read() ;
            Serial.print(result, HEX);
            Serial.print(F(" "));
        }
        Serial.println(F("]"));
    }
}

