/**
 * ATtiny85 HVSP reset lock recovery
 *  code for master ATtiny85
 * 
 *  WARNING: ONLY TO BE USED AGAINST OTHER ATtiny85's
 * 
 * writes the fueses
 *  H   0xDF
 * 
 * note:
 *  - code is pov of the master, but datasheet is slave pov
 *  - master runs at 8 MHz
 *  - master has Reset pin as GPIO
 */

#ifndef __AVR_ATtiny85__
#define __AVR_ATtiny85__
#endif

#ifndef F_CPU
#define F_CPU 8000000UL
#endif

#define CLK_DELAY_us    3   

#include <stdbool.h>
#include <stdint.h>
#include <avr/io.h>
#include <util/delay.h>

// uP chapter 20.6
#define SDI             _BV(PB0)
#define SII             _BV(PB1)
#define SDO             _BV(PB2)
#define SCI             _BV(PB3)
#define VCC_CTRL        _BV(PB4)
#define HV_CTRL         _BV(PB5)
#define PROG_ENABLE0    SDI
#define PROG_ENABLE1    SII
#define PROG_ENABLE2    SDO

typedef struct {
    uint8_t input;
    uint8_t instruction;
    uint8_t output;
} Instr;
void transfer_instruction(Instr* txrx, uint8_t len);

int main(){
    //------------------------------------------------------------------------------------
    //  WARNING: ONLY TO BE USED AGAINST OTHER ATtiny85's
    //------------------------------------------------------------------------------------
    
    //------------------------------------------------------------------------------------
    //  init
    //------------------------------------------------------------------------------------

    DDRB |= SCI | SII | SDI | SDO | HV_CTRL | VCC_CTRL; // outputs

    // everything to low
    PORTB &= ~(SCI | SII | SDI | SDO | HV_CTRL | VCC_CTRL);

    //------------------------------------------------------------------------------------
    // enter HVSP mode - uP chapter 20.7.1
    //------------------------------------------------------------------------------------

    // 1. Set Prog_enable pins listed in Table 20-14 to “000”, RESET pin and VCC to 0V.
    PORTB &= ~(PROG_ENABLE0 | PROG_ENABLE1 | PROG_ENABLE2 | HV_CTRL | VCC_CTRL);

    _delay_ms(100); // wait for latches

    // 2. Apply 4.5 - 5.5V between VCC and GND. Ensure that VCC reaches at least 1.8V within the next 20 μs.
    PORTB |= VCC_CTRL;

    // 3. Wait 20 - 60 μs, and apply 11.5 - 12.5V to RESET.
    _delay_us(30);
    PORTB |= HV_CTRL;

    // 4. Keep the Prog_enable pins unchanged for at least 10 μs after the High-voltage has been applied to
    //      ensure the Prog_enable Signature has been latched.
    _delay_us(15);

    // 5. Release the Prog_enable[2] pin to avoid drive contention on the Prog_enable[2]/SDO pin.
    
    // set PROG_ENABLE2 to Hi-Z input
    DDRB    &= ~PROG_ENABLE2;         // set as input
    PORTB   &= ~PROG_ENABLE2;         // disable bias resistors

    // 6. Wait at least 300 μs before giving any serial instructions on SDI/SII.
    _delay_us(350);


    //------------------------------------------------------------------------------------
    // program high fuses
    //------------------------------------------------------------------------------------
    {
        Instr *instrs;
        
        // set fuse bits to 0xDF
        instrs = (Instr[4]){
                  {0x40  ,   0x4C},
                  {0xDF  ,   0x2C},
                  {0x00  ,   0x74},
                  {0x00  ,   0x7C},
                };
        transfer_instruction(instrs, 4);
    }

    //------------------------------------------------------------------------------------
    // exit
    //------------------------------------------------------------------------------------
    
    PORTB &= ~(VCC_CTRL | HV_CTRL);

    while(true)
        ;

    return 0;
}

void transfer_instruction(Instr* txrx, uint8_t len){
    if(!txrx)
        return;

    /* basically just 11 bit SPI with the following gimics
     *  - 2 parallel data lines, SDI and SII
     *  - SDI, SII, and SDO are 8 bit values, MSB first, left aligned within the 11 bits
     *  - SDI and SII uses SPI mode 1
     *  - SDO uses SPI mode 0
     *  - 0's are used as padding bits
     */

    PORTB &= ~SCI; // ensure clock is low (it should already be low)

    uint8_t packet_i;
    for(packet_i = 0; packet_i < len; packet_i++){
        
        // this delay is not necesasry but makes waveform easier to read
        _delay_us(CLK_DELAY_us);

        uint8_t bit_i;
        for(bit_i = 0; bit_i < 11; bit_i++){

            _delay_us(CLK_DELAY_us);
            PORTB |= SCI;

            _delay_us(CLK_DELAY_us);
            PORTB &= ~SCI;

            // SDO
            txrx[packet_i].output |= ((PINB | SDO) != 0) << (7-bit_i);

            // SDI
            if((txrx[packet_i].input       >> (7-bit_i)) & 1)
                PORTB |= SDI;
            else
                PORTB &= ~SDI;

            // SII
            if((txrx[packet_i].instruction >> (7-bit_i)) & 1)
                PORTB |= SII;
            else
                PORTB &= ~SII;

        }
    }
}
