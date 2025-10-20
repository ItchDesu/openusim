#include "usat_handler.h"
#include "chip_specific.h"
#include "usim_constants.h"
#include <string.h>

#if USIM_ENABLE_USAT

// Procesar comando USAT DATA DOWNLOAD
bool usat_handle_data_download(apdu_command_t* cmd, apdu_response_t* resp) {
    if(cmd->lc < 5) {
        resp->sw1sw2 = SW_WRONG_LENGTH;
        return false;
    }
    
    uint8_t tag = cmd->data[0];
    uint8_t length = cmd->data[1];
    
    if(cmd->lc != (2 + length)) {
        resp->sw1sw2 = SW_WRONG_LENGTH;
        return false;
    }
    
    switch(tag) {
        case USAT_TAG_DISPLAY_TEXT:
            // Procesar comando DISPLAY TEXT
            resp->data[0] = USAT_RESPONSE_OK;
            resp->data_len = 1;
            resp->sw1sw2 = SW_OK;
            USIM_LOG_STRING("USAT: DISPLAY TEXT processed\r\n");
            break;
            
        case USAT_TAG_GET_INPUT:
            // Procesar comando GET INPUT
            resp->data[0] = USAT_RESPONSE_OK;
            resp->data[1] = 0x04;
            resp->data[2] = 'T';
            resp->data[3] = 'E';
            resp->data[4] = 'S';
            resp->data[5] = 'T';
            resp->data_len = 6;
            resp->sw1sw2 = SW_OK;
            USIM_LOG_STRING("USAT: GET INPUT processed\r\n");
            break;
            
        case USAT_TAG_SELECT_ITEM:
            // Procesar comando SELECT ITEM
            resp->data[0] = 0x01;
            resp->data_len = 1;
            resp->sw1sw2 = SW_OK;
            USIM_LOG_STRING("USAT: SELECT ITEM processed\r\n");
            break;
            
        case USAT_TAG_SETUP_MENU:
            // Procesar comando SETUP MENU
            resp->data[0] = USAT_RESPONSE_OK;
            resp->data_len = 1;
            resp->sw1sw2 = SW_OK;
            USIM_LOG_STRING("USAT: SETUP MENU processed\r\n");
            break;
            
        case USAT_TAG_SEND_SMS:
            // Procesar comando SEND SMS
            resp->data[0] = USAT_RESPONSE_OK;
            resp->data_len = 1;
            resp->sw1sw2 = SW_OK;
            USIM_LOG_STRING("USAT: SEND SMS processed\r\n");
            break;
            
        default:
            resp->sw1sw2 = SW_INS_NOT_SUPPORTED;
            USIM_LOG_STRING("USAT: Unknown tag\r\n");
            return false;
    }
    
    return true;
}

// Procesar comando ENVELOPE (USAT)
bool usat_handle_envelope(apdu_command_t* cmd, apdu_response_t* resp) {
    // Procesar datos de envoltura USAT
    resp->data[0] = USAT_RESPONSE_OK;
    resp->data_len = 1;
    resp->sw1sw2 = SW_OK;
    
    USIM_LOG_STRING("USAT: ENVELOPE processed\r\n");
    return true;
}

// Procesar comando FETCH (USAT)
bool usat_handle_fetch(apdu_command_t* cmd, apdu_response_t* resp) {
    // La USIM indica a la terminal que tiene comandos pendientes
    
    // Simular comando DISPLAY TEXT pendiente
    resp->data[0] = USAT_TAG_DISPLAY_TEXT;
    resp->data[1] = 0x0D;
    resp->data[2] = 0x81;
    resp->data[3] = 0x01;
    resp->data[4] = 0x82;
    resp->data[5] = 0x08;
    resp->data[6] = 'U';
    resp->data[7] = 'S';
    resp->data[8] = 'I';
    resp->data[9] = 'M';
    resp->data[10] = ' ';
    resp->data[11] = 'T';
    resp->data[12] = 'E';
    resp->data[13] = 'S';
    resp->data[14] = 'T';
    resp->data_len = 15;
    
    resp->sw1sw2 = SW_OK;
    
    USIM_LOG_STRING("USAT: FETCH - Display Text Pending\r\n");
    return true;
}

// Procesamiento en segundo plano USAT
void usat_background_processing(void) {
    static uint32_t usat_counter = 0;
    usat_counter++;

    // Ejemplo: cada 5000 iteraciones simular actividad USAT
    if(usat_counter % 5000 == 0) {
        USIM_LOG_STRING("USAT: Background processing active\r\n");
    }
}

#else

bool usat_handle_data_download(apdu_command_t* cmd, apdu_response_t* resp) {
    (void)cmd;
    if(resp != NULL) {
        resp->sw1sw2 = SW_INS_NOT_SUPPORTED;
        resp->data_len = 0U;
    }
    return false;
}

bool usat_handle_envelope(apdu_command_t* cmd, apdu_response_t* resp) {
    (void)cmd;
    if(resp != NULL) {
        resp->sw1sw2 = SW_INS_NOT_SUPPORTED;
        resp->data_len = 0U;
    }
    return false;
}

bool usat_handle_fetch(apdu_command_t* cmd, apdu_response_t* resp) {
    (void)cmd;
    if(resp != NULL) {
        resp->sw1sw2 = SW_INS_NOT_SUPPORTED;
        resp->data_len = 0U;
    }
    return false;
}

void usat_background_processing(void) {
    // No hay procesamiento cuando USAT está deshabilitado
}

#endif
