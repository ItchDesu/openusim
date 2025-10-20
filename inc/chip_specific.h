#ifndef CHIP_SPECIFIC_H
#define CHIP_SPECIFIC_H

#include <stdint.h>
#include <stdbool.h>

// Configuración de memoria para THC20F17BD-V40
// 132 KB de FLASH accesible mediante banking de código
#define CODE_MEMORY_SIZE    (132UL * 1024UL)
// 2 KB de XDATA RAM disponibles para buffers y datos variables
#define XRAM_MEMORY_SIZE    2048
// 256 bytes de IRAM internos compatibles con 8051
#define IRAM_MEMORY_SIZE    256

// Pines para interfaz SIM
#define SIM_CLK_PIN         0x01
#define SIM_RST_PIN         0x02  
#define SIM_IO_PIN          0x04
#define SIM_VCC_PIN         0x08

// Registros específicos del THC20F17BD-V40
__sfr __at(0x80) P0;
__sfr __at(0x90) P1;
__sfr __at(0xA0) P2;
__sfr __at(0xB0) P3;
__sfr __at(0x98) SCON;
__sfr __at(0x99) SBUF;
__sfr __at(0x89) TMOD;
__sfr __at(0x8C) TH0;
__sfr __at(0x8A) TL0;
__sfr __at(0x88) TCON;

// Bits de control de temporizador
#define TCON_TF0            0x20
#define TCON_TR0            0x10

// Prototipos
void chip_init(void);
void chip_gpio_init(void);
void uart_init(uint32_t baudrate);
void uart_send_char(char c);
void uart_send_string(const char* str);
void timer_init(void);
void delay_ms(uint16_t ms);
void sim_power_on(void);
void sim_power_off(void);
void sim_reset(void);
bool sim_send_byte(uint8_t data);
bool sim_receive_byte(uint8_t* data, uint32_t timeout_cycles);
bool sim_wait_for_atr_window(void);
bool sim_detect_reset_request(void);
bool sim_handle_pps_sequence(void);

#ifndef USIM_ENABLE_LOGGING
#define USIM_ENABLE_LOGGING 0
#endif

#if USIM_ENABLE_LOGGING
#define USIM_LOG_STRING(str) uart_send_string(str)
#define USIM_LOG_CHAR(ch)    uart_send_char(ch)
#else
#define USIM_LOG_STRING(str) ((void)0)
#define USIM_LOG_CHAR(ch)    ((void)0)
#endif

#endif
