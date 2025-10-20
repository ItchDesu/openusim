#include "usim_app.h"
#include "usim_files.h"
#include "chip_specific.h"
#include "usim_constants.h"
#include "apdu_handler.h"
#include <string.h>

#define SIM_RX_START_TIMEOUT     (120000UL)
#define SIM_RX_INTERBYTE_TIMEOUT (60000UL)

static bool apdu_instruction_requires_lc(uint8_t ins) {
    switch(ins) {
        case INS_SELECT_FILE:
        case INS_UPDATE_BINARY:
        case INS_VERIFY_CHV:
        case INS_CHANGE_CHV:
        case INS_AUTHENTICATE:
#if USIM_ENABLE_USAT
        case INS_USAT_DATA_DOWNLOAD:
        case INS_USAT_ENVELOPE:
#endif
#if USIM_ENABLE_CONFIG_APDU
        case INS_WRITE_CONFIG:
        case INS_XOR_AUTH:
#endif
            return true;
        default:
            return false;
    }
}

// Inicialización de la USIM
void usim_init(void) {
    // Inicializar estructura del suscriptor
    memset(&subscriber, 0, sizeof(subscriber));
    subscriber.pin1_retries = 3;
    subscriber.puk1_retries = 10;
    
    // Configurar PIN por defecto (0000)
    memcpy(subscriber.pin1, "0000", 4U);
    memset(&subscriber.pin1[4], 0xFF, 4U);
    
    // Inicializar sesión
    memset(&session, 0, sizeof(session));
    session.state = USIM_STATE_IDLE;
    session.authenticated = false;
    
    // Inicializar archivo actual
    current_file.file_id = 0x3F00; // MF
    current_file.file_type = FILE_TYPE_MF;
    current_file.file_size = 0;
    
    // Inicializar sistema de archivos
    usim_filesystem_init();
    
    USIM_LOG_STRING("USIM Application Initialized\r\n");
}

// Recepción real de APDU a través de la interfaz SIM (modo T=0)
bool usim_receive_apdu(uint8_t* buffer, uint16_t* length) {
    uint8_t header[4];
    uint8_t index;
    uint16_t offset = 0U;
    uint8_t p3 = 0U;
    uint8_t data_buffer[USIM_APDU_MAX_DATA_LEN];
    uint16_t data_len = 0U;
    bool expects_lc = false;

    if(buffer == NULL || length == NULL) {
        return false;
    }

    *length = 0U;

    // Leer cabecera mínima (CLA, INS, P1, P2)
    if(!sim_receive_byte(&header[0], SIM_RX_START_TIMEOUT)) {
        return false;
    }

    for(index = 1U; index < 4U; ++index) {
        if(!sim_receive_byte(&header[index], SIM_RX_INTERBYTE_TIMEOUT)) {
            return false;
        }
    }

    memcpy(buffer, header, sizeof(header));
    offset = sizeof(header);

    expects_lc = apdu_instruction_requires_lc(header[1]);

    // Intentar obtener P3 (Lc o Le). Si no llega más información, Case 1.
    if(!sim_receive_byte(&p3, SIM_RX_INTERBYTE_TIMEOUT)) {
        if(!sim_send_byte(0x60U)) {
            USIM_LOG_STRING("APDU procedure NULL failed\r\n");
        }
        *length = offset;
        return true;
    }

    buffer[offset++] = p3;

    if(expects_lc) {
        uint16_t remaining = p3;

        if(remaining > 0U) {
            if(!sim_send_byte(header[1])) {
                USIM_LOG_STRING("APDU RX failed to request data\r\n");
                return false;
            }

            while(remaining > 0U) {
                uint8_t data_byte;

                if(!sim_receive_byte(&data_byte, SIM_RX_INTERBYTE_TIMEOUT)) {
                    USIM_LOG_STRING("APDU RX timeout in data phase\r\n");
                    return false;
                }

                if(data_len < USIM_APDU_MAX_DATA_LEN) {
                    data_buffer[data_len++] = data_byte;
                }
                remaining--;
            }

            if(data_len > 0U) {
                memcpy(&buffer[offset], data_buffer, data_len);
                offset = (uint16_t)(offset + data_len);
            }
        }

        // Intentar leer un posible Le adicional (Case 4).
        {
            uint8_t le_byte;
            if(sim_receive_byte(&le_byte, SIM_RX_INTERBYTE_TIMEOUT)) {
                buffer[offset++] = le_byte;
            }
        }
    } else {
        // Case 2: no se espera carga adicional, P3 actúa como Le
    }

    if(!sim_send_byte(0x60U)) {
        USIM_LOG_STRING("APDU procedure NULL failed\r\n");
    }

    *length = offset;
    return true;
}

// Enviar respuesta APDU
void usim_send_response(uint8_t* response, uint16_t length) {
    uint16_t index;

    if(response == NULL || length == 0U) {
        return;
    }

    for(index = 0U; index < length; ++index) {
        if(!sim_send_byte(response[index])) {
            USIM_LOG_STRING("SIM TX failure\r\n");
            break;
        }
    }
}

// Tareas de fondo
void usim_background_tasks(void) {
    // Aquí se pueden añadir tareas periódicas
    static uint32_t counter = 0;
    counter++;
    
    // Ejemplo: cada 100 iteraciones enviar estado
    if ((counter % 100U) == 0U) {
        USIM_LOG_STRING("USIM Background - State: ");
        if ((session.state & USIM_STATE_AUTHENTICATED) != 0U) USIM_LOG_CHAR('A');
        if ((session.state & USIM_STATE_PIN_VERIFIED) != 0U) USIM_LOG_CHAR('P');
        if ((session.state & USIM_STATE_SELECTED) != 0U) USIM_LOG_CHAR('S');
        if (session.state == USIM_STATE_IDLE) USIM_LOG_CHAR('I');
        USIM_LOG_STRING("\r\n");
    }
}

// Obtener datos de archivo
const uint8_t* usim_get_file_data(uint16_t file_id, uint8_t* buffer, uint16_t* length) {
    const usim_file_t* file = usim_find_file(file_id);
    if (file == NULL || file->file_data == NULL) {
        return NULL;
    }
    
    // Para archivos sensibles, aplicar XOR inverso
    if (file_id == 0x6F08 || file_id == 0x6F09) {
        memcpy(buffer, file->file_data, file->file_size);
        usim_xor_operation(buffer, file->file_size, xor_key, 16U);
        *length = file->file_size;
        return buffer;
    }
    
    *length = file->file_size;
    return file->file_data;
}

// Actualizar archivo (versión corregida)
void usim_update_file(uint16_t file_id, const uint8_t* data, uint16_t length) {
    usim_file_t* file = usim_find_file_mutable(file_id);
    if(file == NULL || file->file_data == NULL) {
        USIM_LOG_STRING("File update failed: not writable\r\n");
        return;
    }

    if(length > file->file_size) {
        length = file->file_size;
    }

    if(length > 0U) {
        memcpy(file->file_data, data, length);
        file->data_size = length;
    }

    USIM_LOG_STRING("File update - ID: 0x");

    {
        char hex[5];
        hex[0] = "0123456789ABCDEF"[(file_id >> 12) & 0x0F];
        hex[1] = "0123456789ABCDEF"[(file_id >> 8) & 0x0F];
        hex[2] = "0123456789ABCDEF"[(file_id >> 4) & 0x0F];
        hex[3] = "0123456789ABCDEF"[file_id & 0x0F];
        hex[4] = '\0';
        USIM_LOG_STRING(hex);
    }

    USIM_LOG_STRING(" Len: ");

    {
        char len_str[6];
        uint8_t pos = 0U;
        uint16_t temp = length;

        if(temp == 0U) {
            len_str[pos++] = '0';
        } else {
            while(temp > 0U && pos < 5U) {
                len_str[pos++] = (char)('0' + (temp % 10U));
                temp /= 10U;
            }
        }

        while(pos > 0U) {
            pos--;
            USIM_LOG_CHAR(len_str[pos]);
        }
    }

    USIM_LOG_STRING("\r\n");
}
