#include "chip_specific.h"
#include "usim_constants.h"

#include <stddef.h>

#define SIM_MACHINE_CYCLE_DIV   4UL    /* Hoja de datos: 1 machine cycle = 4 clock cycles */
#define SIM_ETU_FACTOR          372UL  /* ISO/IEC 7816 Fi default */
#define SIM_DEFAULT_ETU_TICKS   (SIM_ETU_FACTOR / SIM_MACHINE_CYCLE_DIV)
#define SIM_MIN_ETU_TICKS       8UL
#define SIM_MAX_ETU_TICKS       0xFFFFUL
#define SIM_ATR_GUARD_ETUS      420U
#define SIM_MEASURE_GUARD       200000UL
#define SIM_PPS_START_TIMEOUT   120000UL
#define SIM_PPS_INTERBYTE_TIMEOUT 60000UL
#define SIM_VCC_FALLBACK_ITER   80000UL
#define SIM_PREFETCH_CAPACITY   8U

static uint32_t sim_etu_ticks = SIM_DEFAULT_ETU_TICKS;
static uint32_t sim_half_etu_ticks = SIM_DEFAULT_ETU_TICKS / 2U;
static uint32_t sim_quarter_etu_ticks = (SIM_DEFAULT_ETU_TICKS / 4U) ? (SIM_DEFAULT_ETU_TICKS / 4U) : 1U;
static bool sim_etu_ready = false;
static bool sim_vcc_present = false;
static bool sim_reset_pending = true;
static bool sim_atr_ready_flag = false;
static uint8_t sim_rst_last = 0U;
static uint32_t sim_poll_counter = 0UL;
static uint8_t sim_rx_prefetch_buf[SIM_PREFETCH_CAPACITY];
static uint8_t sim_rx_prefetch_count = 0U;
static bool sim_pps_processed = false;

static void sim_set_etu_ticks(uint32_t ticks);
static void sim_delay_ticks(uint32_t ticks);
static void sim_delay_etus(uint16_t etus);
static void sim_delay_quarter_etu(void);
static void sim_io_drive_low(void);
static void sim_io_release(void);
static uint8_t sim_io_read(void);
static bool sim_prefetch_pop(uint8_t* value);
static void sim_prefetch_push(uint8_t value);
static void sim_prefetch_clear(void);
static uint32_t sim_measure_clock_period(void);
static void sim_update_clock_from_reader(void);
static void sim_prepare_after_reset(void);
static void sim_transport_poll(void);

static void sim_set_etu_ticks(uint32_t ticks) {
    if(ticks < SIM_MIN_ETU_TICKS) {
        ticks = SIM_MIN_ETU_TICKS;
    } else if(ticks > SIM_MAX_ETU_TICKS) {
        ticks = SIM_MAX_ETU_TICKS;
    }

    sim_etu_ticks = ticks;
    sim_half_etu_ticks = ticks / 2UL;
    if(sim_half_etu_ticks == 0UL) {
        sim_half_etu_ticks = 1UL;
    }

    sim_quarter_etu_ticks = ticks / 4UL;
    if(sim_quarter_etu_ticks == 0UL) {
        sim_quarter_etu_ticks = 1UL;
    }
}

static void sim_delay_ticks(uint32_t ticks) {
    while(ticks > 0UL) {
        uint16_t chunk = (ticks > 0xFFFFUL) ? 0xFFFFU : (uint16_t)ticks;
        uint16_t reload = (uint16_t)(0x10000UL - chunk);

        TCON &= (uint8_t)~(TCON_TR0 | TCON_TF0);
        TH0 = (uint8_t)(reload >> 8);
        TL0 = (uint8_t)(reload & 0xFF);
        TCON |= TCON_TR0;

        while((TCON & TCON_TF0) == 0U) {
            /* Esperar a que finalice el temporizador */
        }

        TCON &= (uint8_t)~(TCON_TR0 | TCON_TF0);
        ticks -= chunk;
    }
}

static void sim_delay_etus(uint16_t etus) {
    while(etus > 0U) {
        sim_delay_ticks(sim_etu_ticks);
        etus--;
    }
}

static void sim_delay_quarter_etu(void) {
    sim_delay_ticks(sim_quarter_etu_ticks);
}

static void sim_io_drive_low(void) {
    P1 &= (uint8_t)~SIM_IO_PIN;
}

static void sim_io_release(void) {
    P1 |= SIM_IO_PIN;
}

static uint8_t sim_io_read(void) {
    return (uint8_t)((P1 & SIM_IO_PIN) != 0U ? 1U : 0U);
}

static bool sim_prefetch_pop(uint8_t* value) {
    if(sim_rx_prefetch_count == 0U) {
        return false;
    }

    sim_rx_prefetch_count--;
    *value = sim_rx_prefetch_buf[sim_rx_prefetch_count];
    return true;
}

static void sim_prefetch_push(uint8_t value) {
    if(sim_rx_prefetch_count < SIM_PREFETCH_CAPACITY) {
        sim_rx_prefetch_buf[sim_rx_prefetch_count] = value;
        sim_rx_prefetch_count++;
    }
}

static void sim_prefetch_clear(void) {
    sim_rx_prefetch_count = 0U;
}

static uint32_t sim_measure_clock_period(void) {
    uint32_t guard;

    guard = SIM_MEASURE_GUARD;
    while(((P1 & SIM_CLK_PIN) != 0U) && guard-- != 0UL) {
        /* Esperar a que el reloj vaya a bajo */
    }
    if(guard == 0UL) {
        return 0UL;
    }

    guard = SIM_MEASURE_GUARD;
    while(((P1 & SIM_CLK_PIN) == 0U) && guard-- != 0UL) {
        /* Esperar flanco ascendente */
    }
    if(guard == 0UL) {
        return 0UL;
    }

    TH0 = 0U;
    TL0 = 0U;
    TCON &= (uint8_t)~(TCON_TR0 | TCON_TF0);
    TCON |= TCON_TR0;

    guard = SIM_MEASURE_GUARD;
    while(((P1 & SIM_CLK_PIN) != 0U) && guard-- != 0UL) {
        /* Alto */
    }
    if(guard == 0UL) {
        TCON &= (uint8_t)~TCON_TR0;
        return 0UL;
    }

    guard = SIM_MEASURE_GUARD;
    while(((P1 & SIM_CLK_PIN) == 0U) && guard-- != 0UL) {
        /* Bajo */
    }

    TCON &= (uint8_t)~TCON_TR0;

    if(guard == 0UL) {
        return 0UL;
    }

    return ((uint32_t)TH0 << 8) | TL0;
}

static void sim_update_clock_from_reader(void) {
    uint32_t period = sim_measure_clock_period();

    if(period != 0UL) {
        uint32_t etu = period * 372UL;
        sim_set_etu_ticks(etu);
        sim_etu_ready = true;
        USIM_LOG_STRING("SIM clock synchronised\r\n");
    } else {
        if(!sim_etu_ready) {
            sim_set_etu_ticks(SIM_DEFAULT_ETU_TICKS);
            sim_etu_ready = true;
        }
        USIM_LOG_STRING("SIM clock measurement fallback\r\n");
    }
}

static void sim_prepare_after_reset(void) {
    sim_update_clock_from_reader();
    sim_io_release();
    sim_prefetch_clear();
    sim_pps_processed = false;
}

static void sim_transport_poll(void) {
    uint8_t rst_state;

    sim_poll_counter++;

    if(!sim_vcc_present) {
        if((P1 & SIM_VCC_PIN) != 0U) {
            sim_vcc_present = true;
            USIM_LOG_STRING("SIM VCC detected\r\n");
        } else if(sim_poll_counter > SIM_VCC_FALLBACK_ITER) {
            sim_vcc_present = true;
            USIM_LOG_STRING("Assuming SIM VCC present\r\n");
        } else {
            return;
        }
    }

    rst_state = (uint8_t)((P1 & SIM_RST_PIN) != 0U ? 1U : 0U);

    if(rst_state == 0U) {
        sim_reset_pending = true;
    } else if(sim_reset_pending && sim_rst_last == 0U) {
        sim_prepare_after_reset();
        sim_atr_ready_flag = true;
        sim_reset_pending = false;
        sim_poll_counter = 0UL;
        USIM_LOG_STRING("ISO7816 reset detected\r\n");
    }

    sim_rst_last = rst_state;
}

// Configuración de puertos para interfaz SIM
void chip_gpio_init(void) {
    // Liberar líneas para que el lector controle CLK, RST y VCC
    P1 |= (SIM_CLK_PIN | SIM_RST_PIN | SIM_VCC_PIN | SIM_IO_PIN);
}

// Configuración UART para debugging (baudrate fijo)
void uart_init(uint32_t baudrate) {
    (void)baudrate;

#if USIM_ENABLE_LOGGING
    // Configurar UART en modo 1 (8 bits, variable baud) con reloj externo.
    // El THC20F17BD solo expone un temporizador de propósito general,
    // por lo que dejamos la generación de baudios a la configuración
    // predeterminada del cargador o al host de depuración.
    SCON = 0x50;
#else
    // Con logging deshabilitado la UART permanece inactiva.
    SCON = 0x00;
#endif
}

// Enviar carácter por UART
void uart_send_char(char c) {
#if USIM_ENABLE_LOGGING
    SBUF = c;
    while((SCON & 0x02) == 0U) {
        /* Esperar a que finalice la transmisión */
    }
    SCON &= (uint8_t)~0x02;
#else
    (void)c;
#endif
}

// Enviar string por UART
void uart_send_string(const char* str) {
#if USIM_ENABLE_LOGGING
    while(*str != '\0') {
        uart_send_char(*str++);
    }
#else
    (void)str;
#endif
}

// Inicialización de temporizadores
void timer_init(void) {
    // Timer 0 para temporizaciones ISO7816 (modo 1, 16-bit)
    TMOD &= 0xF0;
    TMOD |= 0x01;
    TH0 = 0U;
    TL0 = 0U;
    TCON &= (uint8_t)~(TCON_TR0 | TCON_TF0);
}

// Delay aproximado en milisegundos
void delay_ms(uint16_t ms) {
    uint16_t i;
    uint16_t j;

    for(i = 0; i < ms; i++) {
        for(j = 0; j < 120; j++) {
            __asm nop __endasm;
        }
    }
}

// Control de energía SIM (ahora solo se asegura que las líneas queden liberadas)
void sim_power_on(void) {
    sim_io_release();
}

void sim_power_off(void) {
    sim_io_release();
}

void sim_reset(void) {
    P1 |= SIM_RST_PIN;
}

// Inicialización completa del chip
void chip_init(void) {
    chip_gpio_init();
    uart_init(9600);
    timer_init();

    sim_set_etu_ticks(SIM_DEFAULT_ETU_TICKS);
    sim_etu_ready = true;

    sim_vcc_present = ((P1 & SIM_VCC_PIN) != 0U);
    sim_reset_pending = true;
    sim_atr_ready_flag = false;
    sim_rst_last = 0U;
    sim_poll_counter = 0UL;
    sim_prefetch_clear();
    sim_pps_processed = false;

    sim_io_release();

    USIM_LOG_STRING("\r\n=== THC20F17BD-V40 USIM COS ===\r\n");
    USIM_LOG_STRING("Waiting for ISO7816 reset...\r\n");
}

bool sim_wait_for_atr_window(void) {
    while(!sim_atr_ready_flag) {
        sim_transport_poll();
        delay_ms(1);
    }

    sim_atr_ready_flag = false;

    if(!sim_etu_ready) {
        sim_set_etu_ticks(SIM_DEFAULT_ETU_TICKS);
        sim_etu_ready = true;
    }

    sim_delay_etus(SIM_ATR_GUARD_ETUS);
    return true;
}

bool sim_detect_reset_request(void) {
    sim_transport_poll();

    if(sim_atr_ready_flag) {
        sim_atr_ready_flag = false;

        if(!sim_etu_ready) {
            sim_set_etu_ticks(SIM_DEFAULT_ETU_TICKS);
            sim_etu_ready = true;
        }

        sim_delay_etus(SIM_ATR_GUARD_ETUS);
        sim_prefetch_clear();
        sim_pps_processed = false;
        return true;
    }

    return false;
}

bool sim_send_byte(uint8_t data) {
    uint8_t bit_index;
    uint8_t parity = 0U;

    if(!sim_etu_ready) {
        sim_set_etu_ticks(SIM_DEFAULT_ETU_TICKS);
        sim_etu_ready = true;
    }

    sim_io_drive_low();
    sim_delay_ticks(sim_etu_ticks);

    for(bit_index = 0U; bit_index < 8U; ++bit_index) {
        if((data & 0x01U) != 0U) {
            sim_io_release();
            parity ^= 1U;
        } else {
            sim_io_drive_low();
        }

        sim_delay_ticks(sim_etu_ticks);
        data >>= 1U;
    }

    if(parity != 0U) {
        sim_io_release();
    } else {
        sim_io_drive_low();
    }
    sim_delay_ticks(sim_etu_ticks);

    sim_io_release();
    sim_delay_ticks(sim_etu_ticks);

    sim_delay_ticks(sim_half_etu_ticks);
    return true;
}

bool sim_receive_byte(uint8_t* data, uint32_t timeout_cycles) {
    uint32_t guard = timeout_cycles;
    uint8_t bit_index;
    uint8_t value = 0U;
    uint8_t parity = 0U;
    uint8_t parity_bit;
    uint8_t stop_bit;

    if(data == NULL) {
        return false;
    }

    if(sim_prefetch_pop(data)) {
        return true;
    }

    if(!sim_etu_ready) {
        sim_set_etu_ticks(SIM_DEFAULT_ETU_TICKS);
        sim_etu_ready = true;
    }

    if(guard == 0UL) {
        guard = SIM_MEASURE_GUARD;
    }

    sim_io_release();

    while(guard > 0UL) {
        if(sim_io_read() == 0U) {
            break;
        }
        sim_delay_quarter_etu();
        guard--;

        sim_transport_poll();
        if(sim_atr_ready_flag) {
            return false;
        }
    }

    if(guard == 0UL && sim_io_read() != 0U) {
        return false;
    }

    sim_delay_ticks(sim_half_etu_ticks);
    if(sim_io_read() != 0U) {
        return false;
    }

    sim_delay_ticks(sim_etu_ticks);

    for(bit_index = 0U; bit_index < 8U; ++bit_index) {
        if(sim_io_read() != 0U) {
            value |= (uint8_t)(1U << bit_index);
            parity ^= 1U;
        }
        sim_delay_ticks(sim_etu_ticks);
    }

    parity_bit = sim_io_read();
    sim_delay_ticks(sim_etu_ticks);

    stop_bit = sim_io_read();
    sim_delay_ticks(sim_etu_ticks);

    sim_delay_ticks(sim_half_etu_ticks);

    if(((parity ^ parity_bit) & 0x01U) != 0U) {
        USIM_LOG_STRING("SIM RX parity error\r\n");
    }

    if(stop_bit == 0U) {
        USIM_LOG_STRING("SIM RX stop bit missing\r\n");
    }

    *data = value;
    return true;
}

bool sim_handle_pps_sequence(void) {
    uint8_t first_byte;
    uint8_t pps0;
    uint8_t optional_mask;
    uint8_t optional_bytes[3];
    uint8_t optional_count = 0U;
    uint8_t pck;
    uint8_t xor_acc;
    uint8_t index;
    uint8_t consumed[6];
    uint8_t consumed_len = 0U;

    if(sim_pps_processed) {
        return true;
    }

    if(!sim_receive_byte(&first_byte, SIM_PPS_START_TIMEOUT)) {
        sim_pps_processed = true;
        return true;
    }

    consumed[consumed_len++] = first_byte;

    if(first_byte != 0xFFU) {
        sim_prefetch_push(first_byte);
        sim_pps_processed = true;
        return true;
    }

    if(!sim_receive_byte(&pps0, SIM_PPS_INTERBYTE_TIMEOUT)) {
        sim_prefetch_push(first_byte);
        sim_pps_processed = true;
        return true;
    }

    if(consumed_len < sizeof(consumed)) {
        consumed[consumed_len++] = pps0;
    }

    if(pps0 < 0x10U || pps0 > 0x1FU) {
        for(index = 0U; index < consumed_len; ++index) {
            sim_prefetch_push(consumed[consumed_len - 1U - index]);
        }
        sim_pps_processed = true;
        return true;
    }

    optional_mask = (uint8_t)(pps0 & 0x0FU);
    xor_acc = (uint8_t)(first_byte ^ pps0);

    for(index = 0U; index < 3U; ++index) {
        if((optional_mask & (1U << index)) != 0U) {
            if(optional_count >= 3U) {
                break;
            }

            if(!sim_receive_byte(&optional_bytes[optional_count], SIM_PPS_INTERBYTE_TIMEOUT)) {
                sim_pps_processed = true;
                return true;
            }

            if(consumed_len < sizeof(consumed)) {
                consumed[consumed_len++] = optional_bytes[optional_count];
            }

            xor_acc ^= optional_bytes[optional_count];
            optional_count++;
        }
    }

    if(!sim_receive_byte(&pck, SIM_PPS_INTERBYTE_TIMEOUT)) {
        sim_pps_processed = true;
        return true;
    }

    if(consumed_len < sizeof(consumed)) {
        consumed[consumed_len++] = pck;
    }

    xor_acc ^= pck;

    if(xor_acc != 0U) {
        USIM_LOG_STRING("PPS checksum mismatch - treating as APDU\r\n");
        for(index = 0U; index < consumed_len; ++index) {
            sim_prefetch_push(consumed[consumed_len - 1U - index]);
        }
        sim_pps_processed = true;
        return true;
    }

    sim_pps_processed = true;

    if((pps0 & 0xF0U) != 0x10U) {
        USIM_LOG_STRING("PPS protocol unsupported\r\n");
        return true;
    }

    if((optional_mask & 0x08U) != 0U) {
        USIM_LOG_STRING("PPS reserved bits set\r\n");
        return true;
    }

    if(optional_count > 0U) {
        USIM_LOG_STRING("PPS parameter change ignored\r\n");
        return true;
    }

    if(!sim_send_byte(first_byte)) {
        return false;
    }
    if(!sim_send_byte(pps0)) {
        return false;
    }
    if(!sim_send_byte(pck)) {
        return false;
    }

    USIM_LOG_STRING("PPS echoed\r\n");
    return true;
}
