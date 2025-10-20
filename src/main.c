#include "chip_specific.h"
#include "usim_app.h"
#include "apdu_handler.h"
#include "usat_handler.h"
#include "usim_constants.h"
#include <string.h>

// Tamaños de buffer derivados de la especificación APDU
#define APDU_BUFFER_SIZE       (USIM_APDU_MAX_DATA_LEN + 5U)

// Variables globales
__xdata uint8_t apdu_buffer[APDU_BUFFER_SIZE];
__xdata uint8_t apdu_response[USIM_APDU_RESPONSE_MAX_LEN];
__xdata session_context_t session;
__xdata subscriber_data_t subscriber;
__xdata current_file_t current_file;

// Clave XOR para operaciones criptográficas
const uint8_t xor_key[16] = {
    0x2A, 0x4F, 0x1C, 0x93, 0x76, 0xA8, 0xDF, 0x35,
    0xB9, 0x62, 0x8C, 0x17, 0xE4, 0x50, 0x3B, 0xCE
};

// Prototipos de funciones locales
void send_hex_byte(uint8_t byte);
void simple_delay(void);
static void usim_send_default_atr(void);

void main(void) {
    // 1. Inicialización del hardware
    chip_init();
    
    // 2. Inicialización de la USIM
    usim_init();
    
    USIM_LOG_STRING("USIM COS Initialized - Ready\r\n");
    USIM_LOG_STRING("Version: 2.0\r\n");

    // 3. Esperar a que el lector active la tarjeta y enviar ATR
    if(sim_wait_for_atr_window()) {
        usim_send_default_atr();
        if(!sim_handle_pps_sequence()) {
            USIM_LOG_STRING("PPS handling failed\r\n");
        }
    } else {
        USIM_LOG_STRING("ATR window failed\r\n");
    }

    // 4. Bucle principal de operación
    USIM_LOG_STRING("Entering main loop...\r\n");
    while(1) {
        uint16_t cmd_len = 0U;

        if(sim_detect_reset_request()) {
            USIM_LOG_STRING("ISO7816 reset - reinitializing session\r\n");
            usim_init();
            if(sim_wait_for_atr_window()) {
                usim_send_default_atr();
                if(!sim_handle_pps_sequence()) {
                    USIM_LOG_STRING("PPS handling failed\r\n");
                }
            }
            continue;
        }

        if(usim_receive_apdu(apdu_buffer, &cmd_len) && cmd_len > 0U) {
            uint16_t resp_len = 0U;
            (void)apdu_process_command(apdu_buffer, cmd_len, apdu_response, &resp_len);

            if(resp_len > 0U) {
                usim_send_response(apdu_response, resp_len);
            }
        }

        usat_background_processing();
        simple_delay();
    }
}

// Enviar byte en formato HEX
void send_hex_byte(uint8_t byte) {
#if USIM_ENABLE_LOGGING
    USIM_LOG_CHAR(' ');
    USIM_LOG_CHAR("0123456789ABCDEF"[byte >> 4]);
    USIM_LOG_CHAR("0123456789ABCDEF"[byte & 0x0F]);
#else
    (void)byte;
#endif
}

// Delay simple sin parámetros
void simple_delay(void) {
    volatile uint16_t i;
    for(i = 0U; i < 1000U; i++) {
        /* NOP para delay */
    }
}

static void usim_send_default_atr(void) {
    // ATR genérico compatible con USIM (TS 102 221)
    static const uint8_t atr[] = {
        0x3B, 0x9F, 0x96, 0x80, 0x1F, 0xC7, 0x80, 0x31,
        0xE0, 0x73, 0xFE, 0x21, 0x13, 0x57, 0x4A
    };

    uint8_t index;

    for(index = 0U; index < sizeof(atr); ++index) {
        if(!sim_send_byte(atr[index])) {
            USIM_LOG_STRING("ATR transmission failure\r\n");
            break;
        }
    }
}

// Handler de interrupciones vacío
void usim_isr(void) __interrupt(0) {
    // Vacío por ahora
}
