#include "apdu_handler.h"
#include "usim_files.h"
#include "usim_auth.h"
#include "usat_handler.h"
#include "config_apdu.h"
#include "chip_specific.h"
#include "usim_app.h"
#include "usim_constants.h"
#include <string.h>

static apdu_command_t g_apdu_cmd;
static apdu_response_t g_apdu_resp;

#if USIM_ENABLE_LOGGING
static void uart_send_uint16(uint16_t value) {
    char buffer[6];
    uint8_t digits = 0U;

    if(value == 0U) {
        uart_send_char('0');
        return;
    }

    while(value > 0U && digits < sizeof(buffer)) {
        buffer[digits++] = (char)('0' + (value % 10U));
        value /= 10U;
    }

    while(digits > 0U) {
        digits--;
        uart_send_char(buffer[digits]);
    }
}
#endif

#if USIM_ENABLE_LOGGING
#define USIM_LOG_UINT16(value) uart_send_uint16(value)
#else
#define USIM_LOG_UINT16(value) ((void)0)
#endif

// Procesar comando SELECT FILE
bool handle_select_file(apdu_command_t* cmd, apdu_response_t* resp) {
    if(cmd->lc != 2U) {
        resp->sw1sw2 = SW_WRONG_LENGTH;
        return false;
    }

    if(cmd->le != 0U && cmd->le < 13U) {
        resp->sw1sw2 = SW_WRONG_LENGTH;
        return false;
    }
    
    uint16_t file_id = (cmd->data[0] << 8) | cmd->data[1];
    const usim_file_t* file = usim_find_file(file_id);
    
    if(file == NULL) {
        resp->sw1sw2 = SW_FILE_NOT_FOUND;
        return false;
    }
    
    if(!usim_check_access(file, ACCESS_SELECT)) {
        resp->sw1sw2 = SW_SECURITY_STATUS_NOT_SATISFIED;
        return false;
    }
    
    // Actualizar archivo actual
    current_file.file_id = file_id;
    current_file.file_type = file->file_type;
    current_file.file_size = file->file_size;
    session.state |= USIM_STATE_SELECTED;

    // Preparar respuesta FCP mínima conforme a 3GPP TS 31.102
    resp->data[0] = 0x62;                 // Tag FCP template
    resp->data[1] = 0x0B;                 // Longitud del contenido
    resp->data[2] = 0x80;                 // Tag: Tamaño del archivo
    resp->data[3] = 0x02;
    resp->data[4] = (uint8_t)((file->file_size >> 8) & 0xFF);
    resp->data[5] = (uint8_t)(file->file_size & 0xFF);
    resp->data[6] = 0x82;                 // Descriptor de archivo
    resp->data[7] = 0x01;
    resp->data[8] = (file->file_type == FILE_TYPE_EF) ? 0x21U : 0x38U;
    resp->data[9] = 0x83;                 // File Identifier
    resp->data[10] = 0x02;
    resp->data[11] = (uint8_t)((file_id >> 8) & 0xFF);
    resp->data[12] = (uint8_t)(file_id & 0xFF);
    resp->data_len = 13U;

    resp->sw1sw2 = SW_OK;

    USIM_LOG_STRING("SELECT FILE: ");
    USIM_LOG_STRING(file->name);
    USIM_LOG_STRING("\r\n");
    
    return true;
}

// Procesar comando READ BINARY
bool handle_read_binary(apdu_command_t* cmd, apdu_response_t* resp) {
    uint16_t offset = (cmd->p1 << 8) | cmd->p2;

    const usim_file_t* file = usim_find_file(current_file.file_id);
    if(file == NULL) {
        resp->sw1sw2 = SW_FILE_NOT_FOUND;
        return false;
    }

    if(file->file_type != FILE_TYPE_EF) {
        resp->sw1sw2 = SW_COMMAND_NOT_ALLOWED;
        return false;
    }

    if(!usim_check_access(file, ACCESS_READ)) {
        resp->sw1sw2 = SW_SECURITY_STATUS_NOT_SATISFIED;
        return false;
    }

    if(offset >= file->file_size || file->file_data == NULL) {
        resp->sw1sw2 = SW_WRONG_PARAMETERS;
        return false;
    }

    {
        uint16_t available = file->file_size - offset;
        uint16_t requested = available;

        if(cmd->le != 0U) {
            if(cmd->le == 256U) {
                requested = (available > 256U) ? 256U : available;
            } else if(cmd->le < available) {
                requested = cmd->le;
            }
        } else if(requested > USIM_APDU_RESPONSE_DATA_MAX) {
            requested = USIM_APDU_RESPONSE_DATA_MAX;
        }

        if(current_file.file_id == 0x6F08 || current_file.file_id == 0x6F09) {
            uint8_t temp_buffer[16];
            uint16_t data_len = 0U;
            const uint8_t* file_data = usim_get_file_data(current_file.file_id, temp_buffer, &data_len);
            if(file_data != NULL) {
                memcpy(resp->data, &file_data[offset], requested);
            }
        } else {
            memcpy(resp->data, &file->file_data[offset], requested);
        }

        resp->data_len = requested;
    }
    resp->sw1sw2 = SW_OK;

    USIM_LOG_STRING("READ BINARY: ");
    USIM_LOG_UINT16(resp->data_len);
    USIM_LOG_STRING(" bytes\r\n");

    return true;
}

// Procesar comando AUTHENTICATE
bool handle_authenticate(apdu_command_t* cmd, apdu_response_t* resp) {
    if(cmd->lc < 16U) {
        resp->sw1sw2 = SW_WRONG_LENGTH;
        return false;
    }

    if((session.state & USIM_STATE_PIN_VERIFIED) == 0U) {
        resp->sw1sw2 = SW_SECURITY_STATUS_NOT_SATISFIED;
        return false;
    }

    if(cmd->le != 0U && cmd->le != 256U && cmd->le < 54U) {
        resp->sw1sw2 = SW_WRONG_LENGTH;
        return false;
    }

    {
        uint8_t rand[16];
        memcpy(rand, cmd->data, 16U);

        if(usim_run_xor_auth(rand, resp->data, &resp->data_len)) {
            resp->sw1sw2 = SW_OK;
            USIM_LOG_STRING("AUTHENTICATE: XOR Success\r\n");
            return true;
        }
    }

    resp->sw1sw2 = SW_AUTHENTICATION_FAILED;
    USIM_LOG_STRING("AUTHENTICATE: XOR Failed\r\n");
    return false;
}

// Procesar comando VERIFY CHV (PIN)
bool handle_verify_chv(apdu_command_t* cmd, apdu_response_t* resp) {
    if(cmd->lc != 8U) {
        resp->sw1sw2 = SW_WRONG_LENGTH;
        return false;
    }

    if((cmd->p2 & 0x01U) == 0U) {
        resp->sw1sw2 = SW_WRONG_PARAMETERS;
        return false;
    }

    if(subscriber.pin1_retries == 0U) {
        resp->sw1sw2 = SW_PIN_BLOCKED;
        return false;
    }

    // Verificar PIN
    if(memcmp(cmd->data, subscriber.pin1, 8) == 0) {
        session.state |= USIM_STATE_PIN_VERIFIED;
        subscriber.pin1_retries = 3;
        resp->sw1sw2 = SW_OK;
        USIM_LOG_STRING("VERIFY CHV: PIN Correct\r\n");
        return true;
    } else {
        if(subscriber.pin1_retries > 0) {
            subscriber.pin1_retries--;
        }
        if(subscriber.pin1_retries == 0U) {
            resp->sw1sw2 = SW_PIN_BLOCKED;
            USIM_LOG_STRING("VERIFY CHV: PIN Blocked\r\n");
        } else {
            resp->sw1sw2 = SW_REMAINING_ATTEMPTS(subscriber.pin1_retries);
            USIM_LOG_STRING("VERIFY CHV: PIN Incorrect\r\n");
        }
        return false;
    }
}

// Procesar comando CHANGE CHV
bool handle_change_chv(apdu_command_t* cmd, apdu_response_t* resp) {
    if(cmd->lc != 16U) {
        resp->sw1sw2 = SW_WRONG_LENGTH;
        return false;
    }

    if((cmd->p2 & 0x01U) == 0U) {
        resp->sw1sw2 = SW_WRONG_PARAMETERS;
        return false;
    }

    if(subscriber.pin1_retries == 0U) {
        resp->sw1sw2 = SW_PIN_BLOCKED;
        return false;
    }

    if(memcmp(cmd->data, subscriber.pin1, 8) != 0) {
        if(subscriber.pin1_retries > 0U) {
            subscriber.pin1_retries--;
        }

        if(subscriber.pin1_retries == 0U) {
            resp->sw1sw2 = SW_PIN_BLOCKED;
            USIM_LOG_STRING("CHANGE CHV: PIN Blocked\r\n");
        } else {
            resp->sw1sw2 = SW_REMAINING_ATTEMPTS(subscriber.pin1_retries);
            USIM_LOG_STRING("CHANGE CHV: Old PIN incorrect\r\n");
        }
        return false;
    }

    memcpy(subscriber.pin1, &cmd->data[8], 8);
    subscriber.pin1_retries = 3;
    session.state |= USIM_STATE_PIN_VERIFIED;

    resp->sw1sw2 = SW_OK;
    USIM_LOG_STRING("CHANGE CHV: PIN Updated\r\n");
    return true;
}

// Procesar comando GET RESPONSE
bool handle_get_response(apdu_command_t* cmd, apdu_response_t* resp) {
    {
        uint16_t requested_length = cmd->le;

        if(requested_length == 0U) {
            requested_length = 256U;
        }

        if(requested_length > 256U) {
            resp->sw1sw2 = SW_WRONG_LENGTH;
            return false;
        }

        resp->data_len = (requested_length > 32U) ? 32U : requested_length;

        {
            uint16_t i;
            for(i = 0U; i < resp->data_len; i++) {
                resp->data[i] = (uint8_t)(i + 0x10U);
            }
        }
    }

    resp->sw1sw2 = SW_OK;
    return true;
}

// Procesar comando STATUS
bool handle_status(apdu_command_t* cmd, apdu_response_t* resp) {
    if(cmd->le != 0U && cmd->le < 5U) {
        resp->sw1sw2 = SW_WRONG_LENGTH;
        return false;
    }

    resp->data[0] = USIM_VERSION_MAJOR;
    resp->data[1] = USIM_VERSION_MINOR;
    resp->data[2] = session.state;
    resp->data[3] = subscriber.pin1_retries;
    resp->data[4] = subscriber.puk1_retries;
    resp->data_len = 5U;
    
    resp->sw1sw2 = SW_OK;
    return true;
}

bool handle_update_binary(apdu_command_t* cmd, apdu_response_t* resp) {
    usim_file_t* file = usim_find_file_mutable(current_file.file_id);
    const usim_file_t* file_const = usim_find_file(current_file.file_id);
    uint16_t offset = (cmd->p1 << 8) | cmd->p2;

    if(file == NULL || file_const == NULL) {
        resp->sw1sw2 = SW_FILE_NOT_FOUND;
        return false;
    }

    if(file_const->file_type != FILE_TYPE_EF) {
        resp->sw1sw2 = SW_COMMAND_NOT_ALLOWED;
        return false;
    }

    if(!usim_check_access(file_const, ACCESS_UPDATE)) {
        resp->sw1sw2 = SW_SECURITY_STATUS_NOT_SATISFIED;
        return false;
    }

    if(cmd->lc == 0U) {
        resp->sw1sw2 = SW_WRONG_LENGTH;
        return false;
    }

    if((offset + cmd->lc) > file_const->file_size) {
        resp->sw1sw2 = SW_WRONG_PARAMETERS;
        return false;
    }

    if(file->file_data == NULL) {
        resp->sw1sw2 = SW_MEMORY_PROBLEM;
        return false;
    }

    memcpy(&file->file_data[offset], cmd->data, cmd->lc);
    if((offset + cmd->lc) > file->data_size) {
        file->data_size = offset + cmd->lc;
    }

    resp->sw1sw2 = SW_OK;
    resp->data_len = 0U;

    USIM_LOG_STRING("UPDATE BINARY: ");
    USIM_LOG_UINT16(cmd->lc);
    USIM_LOG_STRING(" bytes written\r\n");
    return true;
}

// Procesador principal de APDU
bool apdu_process_command(uint8_t* command, uint16_t cmd_len, uint8_t* response, uint16_t* resp_len) {
    apdu_command_t* cmd = &g_apdu_cmd;
    apdu_response_t* resp = &g_apdu_resp;
    bool has_le = false;

    memset(cmd, 0, sizeof(*cmd));
    memset(resp, 0, sizeof(*resp));
    resp->data = response;

    if(cmd_len < 4U) {
        resp->sw1sw2 = SW_WRONG_LENGTH;
        goto send_response;
    }

    cmd->cla = command[0];
    cmd->ins = command[1];
    cmd->p1 = command[2];
    cmd->p2 = command[3];
    cmd->lc = 0U;
    cmd->le = 0U;

    if(cmd_len == 5U) {
        cmd->le = command[4];
        has_le = true;
    } else if(cmd_len > 5U) {
        cmd->lc = command[4];

        if(cmd->lc > USIM_APDU_MAX_DATA_LEN) {
            resp->sw1sw2 = SW_WRONG_LENGTH;
            goto send_response;
        }

        if(cmd_len < (uint16_t)(5U + cmd->lc)) {
            resp->sw1sw2 = SW_WRONG_LENGTH;
            goto send_response;
        }

        if(cmd->lc > 0U) {
            cmd->data = &command[5];
        }

        if(cmd_len == (uint16_t)(5U + cmd->lc + 1U)) {
            cmd->le = command[5 + cmd->lc];
            has_le = true;
        } else if(cmd_len != (uint16_t)(5U + cmd->lc)) {
            resp->sw1sw2 = SW_WRONG_LENGTH;
            goto send_response;
        }
    }

    if(has_le && cmd->le == 0U) {
        cmd->le = 256U;
    }

    if(has_le && cmd->le > 256U) {
        resp->sw1sw2 = SW_WRONG_LENGTH;
        goto send_response;
    }

    {
        bool invoked = false;
        bool success = false;

        if(cmd->cla == CLA_STANDARD || cmd->cla == CLA_GSM) {
            switch(cmd->ins) {
                case INS_SELECT_FILE:
                    success = handle_select_file(cmd, resp);
                    invoked = true;
                    break;

                case INS_READ_BINARY:
                    success = handle_read_binary(cmd, resp);
                    invoked = true;
                    break;

                case INS_UPDATE_BINARY:
                    success = handle_update_binary(cmd, resp);
                    invoked = true;
                    break;

                case INS_VERIFY_CHV:
                    success = handle_verify_chv(cmd, resp);
                    invoked = true;
                    break;

                case INS_CHANGE_CHV:
                    success = handle_change_chv(cmd, resp);
                    invoked = true;
                    break;

                case INS_AUTHENTICATE:
                    success = handle_authenticate(cmd, resp);
                    invoked = true;
                    break;

                case INS_GET_RESPONSE:
                    success = handle_get_response(cmd, resp);
                    invoked = true;
                    break;

                case INS_STATUS:
                    success = handle_status(cmd, resp);
                    invoked = true;
                    break;

                default:
                    break;
            }
        }
#if USIM_ENABLE_USAT || USIM_ENABLE_CONFIG_APDU
        else if(cmd->cla == CLA_USAT || cmd->cla == CLA_CONFIG) {
            switch(cmd->ins) {
#if USIM_ENABLE_USAT
                case INS_USAT_DATA_DOWNLOAD:
                    success = usat_handle_data_download(cmd, resp);
                    invoked = true;
                    break;

                case INS_USAT_ENVELOPE:
                    success = usat_handle_envelope(cmd, resp);
                    invoked = true;
                    break;

                case INS_USAT_FETCH:
                    success = usat_handle_fetch(cmd, resp);
                    invoked = true;
                    break;
#endif
#if USIM_ENABLE_CONFIG_APDU
                case INS_WRITE_CONFIG:
                    success = handle_write_config(cmd, resp);
                    invoked = true;
                    break;

                case INS_READ_CONFIG:
                    success = handle_read_config(cmd, resp);
                    invoked = true;
                    break;

                case INS_XOR_AUTH:
                    success = handle_xor_auth(cmd, resp);
                    invoked = true;
                    break;

                case INS_RESET_SIM:
                    success = handle_reset_sim(cmd, resp);
                    invoked = true;
                    break;
#endif
                default:
                    break;
            }
        }
#endif
        else {
            resp->sw1sw2 = SW_CLA_NOT_SUPPORTED;
        }

        if(!invoked) {
            if(resp->sw1sw2 == 0U) {
                resp->sw1sw2 = SW_INS_NOT_SUPPORTED;
            }
            if(resp->sw1sw2 == SW_INS_NOT_SUPPORTED) {
                USIM_LOG_STRING("APDU: Instruction Not Supported\r\n");
            }
        }

        if(!success) {
            // Mantener resp->sw1sw2 establecido por el handler correspondiente.
        }
    }

send_response:
    // Construir respuesta
    if(resp->data_len > 0U && resp->data != response) {
        memcpy(response, resp->data, resp->data_len);
    }
    response[resp->data_len] = (uint8_t)((resp->sw1sw2 >> 8) & 0xFFU);
    response[resp->data_len + 1U] = (uint8_t)(resp->sw1sw2 & 0xFFU);
    *resp_len = (uint16_t)(resp->data_len + 2U);

    return (resp->sw1sw2 == SW_OK);
}
