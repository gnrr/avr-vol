/*
 * avr_vol.c
 *
 * Created: 2011/11/10 8:02:23
 *  Author: g
 *
 *  desc. : audio selector with digital volume
 *  schem.:
 *  device: ATMEGA88P
 *          PGA2311PA
 */

#include <avr/io.h>
#include <avr/interrupt.h>
#include "mystd.h"

/*=========*/
/* define  */
/*=========*/

/*=========*/
/* const   */
/*=========*/

/*=========*/
/* globals */
/*=========*/
u4 gtime;
u1 select_sw;
u1 mute_sw;
u2 vol_ad;


/*===========*/
/* utilities */
/*===========*/

/*===============*/
/* sub functions */
/*===============*/
static inline void dbg(u1 num, bool state)
{
    u1 mask, now;

    /* debug out: PC3..5, PC7 */
    switch(num) {
    case 3:
    case 4:
    case 5:
    case 7:
        mask = (0x01 << num);
        now  = (PORTC & mask)? ON : OFF;
        if(state == now) return;

        if(state == ON) PORTC |= mask;
        else            PORTC &= ~mask;

    default: return;
    }
}

static inline void led_out(bool state)
{
    const u1 mask = 0x01;         /* 0000_0001 */
    u1 now;

    now = (PORTB & mask)? ON : OFF;
    if(state == now) return;

    if(state == ON) PORTB |= mask;
    else            PORTB &= ~mask;
}

static inline void mute_out(bool state)
{
    const u1 mask = 0x02;         /* 0000_0010 */
    u1 now;

    now = (PORTB & mask)? ON : OFF;
    if(state == now) return;

    if(state == ON) PORTB |= mask;
    else            PORTB &= ~mask;
}

static inline void relay(bool state)
{
    const u1 mask = 0x80;         /* 1000_0000 */
    u1 now;

    now = (PORTD & mask)? ON : OFF;
    if(state == now) return;

    if(state == ON) PORTD |= mask;
    else            PORTD &= ~mask;
}

static inline void inc_gtime()
{
    static u1 i = 0;

    i = (i + 1) % 49;              /* 0..49 --> T= 1sec */
    if((i==0) && (gtime < 0xFFFFFFFF)) gtime++;
}

static inline void select_sw_input(void)
{
    static u1 cnt = 0;

    cnt = (cnt + 1) % 5;                        /* 0..4 */
    PORTD = (0xE0 & PORTD) | (0x01 << cnt);     /* 1110_0000 */
    if(0x20 & PIND) select_sw = cnt;            /* 0010_0000 */
}

static inline void select_sw_out(void)
{
    u1 sel_out;

    switch(select_sw) {
    case 0:                            /* phone input */
        relay(ON);
        break;

    case 1:                            /* PC1: USB IF */
    case 2:                            /* PC2: opt1 */
    case 3:                            /* PC3: opt2 */
    case 4:                            /* PC4: opt3 */
        sel_out = ((select_sw - 1) << 1) & 0x06;      /* xxxx_x11x */
        PORTC = (PORTC & 0xF9) | sel_out;             /* dai-out */
        relay(OFF);
        break;
    default:
        relay(OFF);
    }
}

static inline void mute_ctrl(void)
{
    if(PIND & 0x40) {
        mute_out(ON);    led_out(ON);
    } else {
        mute_out(OFF);   led_out(OFF);
    }
}

static inline void adc_start(void)
{
    disable_interrupt();
    ADCSRA |= 0x40;               /* 0100_0000 */
    enable_interrupt();
}

static inline void spi_send_byte(u1 byte)
{
    SPDR = byte;
    while((SPSR & 0x80) == 0);    /* wait for transmission */
}

u1 get_volume_data(u2 ad)
{
    u1 out;

    out = (u1)(ad >> 2);

    return out;
}

static inline void spi_ss_out(bool level)
{
    const u1 mask = 0x04;         /* 0000_0100 */

    if(level)
        PORTB |= mask;
    else
        PORTB &= ~mask;
}

bool in_range(s2 value, s2 min, s2 max)
{
    if(value < min) return FALSE;
    if(max < value) return FALSE;
    return TRUE;
}

static inline void volume_out(void)
{
    const  s2 thresh = 3;       /* dead band */
    static s2 vol_old = 0;
    s2 vol = (s2)get_volume_data(vol_ad);
    if(in_range(vol, vol_old-thresh, vol_old+thresh)) return;

    const  u1 offset = 0;
    u1 vol_left, vol_right;
    vol_left  = vol_old = vol;
    vol_right = vol + offset;

    disable_interrupt();
    spi_ss_out(LO);
    spi_send_byte(vol_right);
    spi_send_byte(vol_left);
    spi_ss_out(HI);
    enable_interrupt();
}


/*============================*/
/* interrupt service routines */
/*============================*/
/* timer0 compare A: volume input */
ISR(TIMER0_COMPA_vect)
{
    /* dbg(5, ON); */
    disable_interrupt();

    TCNT0 = 0;

    enable_interrupt();
    /* dbg(5, OFF); */
}


/* timer1 compare A:  */
ISR(TIMER1_COMPA_vect)
{
    /* dbg(3, ON); */
    disable_interrupt();

    enable_interrupt();
    /* dbg(3, OFF); */
}

/* timer2 compare A: select sw, gtime */
ISR(TIMER2_COMPA_vect)
{
    /* dbg(4, ON); */
    disable_interrupt();

    select_sw_input();
    TCNT2 =  0;

    inc_gtime();

    enable_interrupt();
    /* dbg(4, OFF); */
}


/* adc conversion complete: volume */
ISR(ADC_vect)
{
    /* dbg(5, ON); */
    disable_interrupt();

    vol_ad = (ADC >> 6);            /* 10 bits, left adjusted */

    enable_interrupt();
    /* dbg(5, OFF); */
}

/*============*/
/* initialize */
/*============*/
void init_devices(void)
{
    /* ------------------------------ */
    /* unlock peripheral restrictions */
    /* ------------------------------ */
    PRR = 0x00;

    /* clock: external xtal 10MHz */

    /* ------------- */
    /* wdt           */
    /* ------------- */
    WDTCSR = 0x00;                /* disable wdt */


    /* ------------- */
    /* gpio          */
    /* ------------- */
    /* port B */
    PORTB = 0xFF;
    DDRB  = 0xEF;
    /*   7     4   3     0                    */
    /*   o o o i___o o o o                    */
    /*   | | | |   | | | |                    */
    /*   | | | |   | | | |                    */
    /*   | | | |   | | | +-- PB0  : LED       */
    /*   | | | |   | | +---- PB1  : MUTE-OUT  */
    /*   | | | |   | +------ PB2  : ~SS (SPI) */
    /*   | | | |   +-------- PB3  : MOSI(SPI) */
    /*   | | | +------------ PB4  : MISO(SPI) not use */
    /*   | | +-------------- PB5  : SCK(SPI)  */
    /*   | +---------------- PB6  : XTAL      */
    /*   +------------------ PB7  : XTAL      */

    /* port C */
    PORTC = 0x00;
    DDRC  = 0xBE;
    /*   7     4   3     0                    */
    /*   o i o o___o o o i                    */
    /*   | | |     | | | |                    */
    /*   | | +-----+ +-+ |                    */
    /*   | |     |    |  +-- PC0  : VOL(ADIN) */
    /*   | |     |    +----- PC2-1: DAI-OUT   */
    /*   | |     +---------- PC5-3: debug     */
    /*   | +---------------- PC6  : RESET     */
    /*   +------------------ PC7  : debug     */

    /* port D */
    PORTD = 0x00;
    DDRD  = 0x9F;
    /*   7     4   3     0                    */
    /*   o i i o___o o o o                    */
    /*   | | | |         |                    */
    /*   | | | +---------+                    */
    /*   | | |       |                        */
    /*   | | |       +------ PD4-0: SEL-OUT   */
    /*   | | +-------------- PD5  : SEL-IN    */
    /*   | +---------------- PD6  : MUTE-SW   */
    /*   +------------------ PD7  : RELAY-OUT */

    /* ------------- */
    /* timer         */
    /* ------------- */
    /* timer0: volume --> adc */
    TCCR0A = 0x00;
    TCCR0B = 0x05;               /* Tck= 102.4us (= 10MHz / 1024) */
    OCR0A  = 90;                /* T=10ms (=102.4us * 98) */
    TCNT0  = 0;
    TIMSK0 = 0x02;               /* enable comapre A */

#if 0
    /* timer1: - */
    TCCR1A = 0x00;
    TCCR1B = 0x05;               /* Tck= 102.4us (= 10MHz / 1024) */
    OCR1A  = 9766;               /* T= 1s (= 102.4us * 9766)  */
    TCNT1  = 0;
    TIMSK1 = 0x02;                /* enable compare A */
#endif

    /* timer2: select sw */
    TCCR2A = 0x00;
    TCCR2B = 0x07;                /* Tck=102.4us (= 10MHz / 1024) */
    OCR2A  = 195;                 /* T=20ms select sw */
    TCNT2  = 0;
    TIMSK2 = 0x02;                /* enable compare A */

    /* ------------- */
    /* adc: volume   */
    /* ------------- */
    ADMUX  = 0x60;                /* AREF, PC0, left adjust */
    DIDR0  = 0x01;                /* PC0 as adc input */
    ADCSRB = 0x03;                /* auto start adc (by timer 0 compare A) */
    ADCSRA = 0xAF;                /* 1010_1111 */

    /* ------------- */
    /* spi: PGA2311  */
    /* ------------- */
    /* DDR_SPI = EF; */
    SPCR = 0x5E;
    /*   7     4   3     0                    */
    /*   0 1 0 1___1 1 1 0                    */
    /*   | | | |   | | | |                    */
    /*   | | | |   | | +-+                    */
    /*   | | | |   | |   |                    */
    /*   | | | |   | |   +-- SPR  : ck/64     */
    /*   | | | |   | +------ CPHA : 1         */
    /*   | | | |   +-------- CPOL : 1         */
    /*   | | | +------------ MSTR : MASTER    */
    /*   | | +-------------- DORD : MSB First */
    /*   | +---------------- SPE  : ENABLE    */
    /*   +------------------ SPIE : -         */

#if 0
    SPSR = 0x00;                  /* 156.25kHz (= ck/64) */
#endif
    /*   7     4   3     0                    */
    /*   0 0 x x___x x x 0                    */
    /*   | |             |                    */
    /*   | |             +-- SPI2X: OFF       */
    /*   | +---------------- WCOL : -         */
    /*   +------------------ SPIF : -         */

    enable_interrupt();
}

void init_rams(void)
{
    gtime     = 0;
    mute_sw   = 0;
    select_sw = 1;                /* USB IF */
    vol_ad    = 0;

}

/*===========*/
/*    main   */
/*===========*/
int main(void)
{
    init_devices();
    init_rams();
    adc_start();

    while(gtime < 3)
        led_out(ON);
    led_out(OFF);

    while(1) {
        select_sw_out();
        volume_out();
        mute_ctrl();
    }
    return 0;
}

/* this program ends here */
