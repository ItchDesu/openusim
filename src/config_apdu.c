#include "config_apdu.h"
#include "usim_files.h"
#include "usim_auth.h"
#include "chip_specific.h"
#include "usim_app.h"
#include "usim_constants.h"
#include <string.h>

#if USIM_ENABLE_CONFIG_APDU

// Comando personalizado para escribir datos
bool handle_write_config(apdu_command_t* cmd, apdu_response_t* resp) {
    if(cmd->lc == 0U) {
        resp->sw1sw2 = SW_WRONG_LENGTH;
        return false;
    }

    uint8_t data_type = cmd->p1;
#if USIM_ENABLE_LOGGING
    const char* type_str = NULL;
    const uint8_t* print_data = cmd->data;
    uint8_t print_len = (uint8_t)cmd->lc;
#endif

    switch(data_type) {
        case DATA_TYPE_IMSI:
        {
            if(cmd->lc != 9U) {
                resp->sw1sw2 = SW_WRONG_LENGTH;
                return false;
            }

            usim_file_t* file = usim_find_file_mutable(0x6F07);
            if(file == NULL || file->file_data == NULL) {
                resp->sw1sw2 = SW_MEMORY_PROBLEM;
                return false;
            }

            memcpy(file->file_data, cmd->data, 9);
#if USIM_ENABLE_LOGGING
            type_str = "IMSI";
#endif
            USIM_LOG_STRING("CONFIG: IMSI updated via APDU\r\n");
            break;
        }

        case DATA_TYPE_KEY:
        {
            if(cmd->lc != 16U) {
                resp->sw1sw2 = SW_WRONG_LENGTH;
                return false;
            }

            usim_file_t* file = usim_find_file_mutable(0x6F08);
            if(file == NULL || file->file_data == NULL) {
                resp->sw1sw2 = SW_MEMORY_PROBLEM;
                return false;
            }

            uint8_t encrypted_key[16];
            memcpy(encrypted_key, cmd->data, 16);
            usim_xor_operation(encrypted_key, 16, xor_key, 16);
            memcpy(file->file_data, encrypted_key, 16);
#if USIM_ENABLE_LOGGING
            type_str = "KEY";
#endif
            USIM_LOG_STRING("CONFIG: Key updated via APDU\r\n");
            break;
        }

        case DATA_TYPE_OPC:
        {
            if(cmd->lc != 16U) {
                resp->sw1sw2 = SW_WRONG_LENGTH;
                return false;
            }

            usim_file_t* file = usim_find_file_mutable(0x6F09);
            if(file == NULL || file->file_data == NULL) {
                resp->sw1sw2 = SW_MEMORY_PROBLEM;
                return false;
            }

            uint8_t encrypted_opc[16];
            memcpy(encrypted_opc, cmd->data, 16);
            usim_xor_operation(encrypted_opc, 16, xor_key, 16);
            memcpy(file->file_data, encrypted_opc, 16);
#if USIM_ENABLE_LOGGING
            type_str = "OPC";
#endif
            USIM_LOG_STRING("CONFIG: OPC updated via APDU\r\n");
            break;
        }

        case DATA_TYPE_PIN:
            if(cmd->lc != 8U) {
                resp->sw1sw2 = SW_WRONG_LENGTH;
                return false;
            }

            memcpy(subscriber.pin1, cmd->data, 8);
            subscriber.pin1_retries = 3;
#if USIM_ENABLE_LOGGING
            type_str = "PIN";
#endif
            USIM_LOG_STRING("CONFIG: PIN updated via APDU\r\n");
            break;

        default:
            resp->sw1sw2 = SW_WRONG_PARAMETERS;
            USIM_LOG_STRING("CONFIG: Unknown data type\r\n");
            return false;
    }

#if USIM_ENABLE_LOGGING
    if(type_str != NULL) {
        USIM_LOG_STRING("CONFIG: ");
        USIM_LOG_STRING(type_str);
        USIM_LOG_STRING(" set to: ");
        {
            uint8_t bytes_to_print = (print_len > 8U) ? 8U : print_len;
            uint8_t i;
            for(i = 0U; i < bytes_to_print; i++) {
                USIM_LOG_CHAR("0123456789ABCDEF"[print_data[i] >> 4]);
                USIM_LOG_CHAR("0123456789ABCDEF"[print_data[i] & 0x0F]);
                USIM_LOG_CHAR(' ');
            }
        }
        USIM_LOG_STRING("\r\n");
    }
#endif

    resp->sw1sw2 = SW_OK;
    return true;
}

// Comando para leer datos de configuración
bool handle_read_config(apdu_command_t* cmd, apdu_response_t* resp) {
    uint8_t data_type = cmd->p1;

    switch(data_type) {
        case DATA_TYPE_IMSI:
        {
            const usim_file_t* file = usim_find_file(0x6F07);
            if(file == NULL || file->file_data == NULL) {
                resp->sw1sw2 = SW_MEMORY_PROBLEM;
                return false;
            }

            memcpy(resp->data, file->file_data, 9U);
            resp->data_len = 9U;
            USIM_LOG_STRING("CONFIG: Reading IMSI\r\n");
            break;
        }

        case DATA_TYPE_STATUS:
            resp->data[0] = session.state;
            resp->data[1] = subscriber.pin1_retries;
            resp->data[2] = USIM_VERSION_MAJOR;
            resp->data[3] = USIM_VERSION_MINOR;
            resp->data_len = 4U;
            USIM_LOG_STRING("CONFIG: Reading status\r\n");
            break;
            
        default:
            resp->sw1sw2 = SW_WRONG_PARAMETERS;
            USIM_LOG_STRING("CONFIG: Cannot read unknown type\r\n");
            return false;
    }
    
    resp->sw1sw2 = SW_OK;
    return true;
}

// Comando para autenticación XOR personalizada
bool handle_xor_auth(apdu_command_t* cmd, apdu_response_t* resp) {
    if(cmd->lc != 16U) {
        resp->sw1sw2 = SW_WRONG_LENGTH;
        return false;
    }

    uint8_t rand[16];
    memcpy(rand, cmd->data, 16U);
    
    // Usar nuestro algoritmo XOR personalizado
    if(usim_run_xor_auth(rand, resp->data, &resp->data_len)) {
        USIM_LOG_STRING("XOR_AUTH: Authentication successful\r\n");
        
        // Debug: mostrar resultados
#if USIM_ENABLE_LOGGING
        USIM_LOG_STRING("XOR_AUTH: RES=");
        {
            uint8_t i;
            for(i = 0U; i < 8U; i++) {
                USIM_LOG_CHAR("0123456789ABCDEF"[resp->data[i] >> 4]);
                USIM_LOG_CHAR("0123456789ABCDEF"[resp->data[i] & 0x0F]);
            }
        }
        USIM_LOG_STRING("\r\n");
#endif
        
        resp->sw1sw2 = SW_OK;
        return true;
    } else {
        resp->sw1sw2 = SW_AUTHENTICATION_FAILED;
        USIM_LOG_STRING("XOR_AUTH: Authentication failed\r\n");
        return false;
    }
}

// Comando para resetear la SIM
bool handle_reset_sim(apdu_command_t* cmd, apdu_response_t* resp) {
    (void)cmd;

    // Resetear estado de la SIM
    memset(&session, 0, sizeof(session));
    session.state = USIM_STATE_IDLE;
    subscriber.pin1_retries = 3;
    subscriber.puk1_retries = 10;

    // Resetear archivo actual
    current_file.file_id = 0x3F00;
    current_file.file_type = FILE_TYPE_MF;
    current_file.file_size = 0;

    resp->sw1sw2 = SW_OK;

    USIM_LOG_STRING("CONFIG: SIM reset performed\r\n");
    return true;
}

#else

bool handle_write_config(apdu_command_t* cmd, apdu_response_t* resp) {
    (void)cmd;
    if(resp != NULL) {
        resp->sw1sw2 = SW_INS_NOT_SUPPORTED;
        resp->data_len = 0U;
    }
    return false;
}

bool handle_read_config(apdu_command_t* cmd, apdu_response_t* resp) {
    (void)cmd;
    if(resp != NULL) {
        resp->sw1sw2 = SW_INS_NOT_SUPPORTED;
        resp->data_len = 0U;
    }
    return false;
}

bool handle_xor_auth(apdu_command_t* cmd, apdu_response_t* resp) {
    (void)cmd;
    if(resp != NULL) {
        resp->sw1sw2 = SW_INS_NOT_SUPPORTED;
        resp->data_len = 0U;
    }
    return false;
}

bool handle_reset_sim(apdu_command_t* cmd, apdu_response_t* resp) {
    (void)cmd;

    memset(&session, 0, sizeof(session));
    session.state = USIM_STATE_IDLE;
    subscriber.pin1_retries = 3;
    subscriber.puk1_retries = 10;

    current_file.file_id = 0x3F00;
    current_file.file_type = FILE_TYPE_MF;
    current_file.file_size = 0;

    if(resp != NULL) {
        resp->sw1sw2 = SW_OK;
        resp->data_len = 0U;
    }

    return true;
}

#endif
