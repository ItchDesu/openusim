#ifndef APDU_HANDLER_H
#define APDU_HANDLER_H

#include <stdint.h>
#include <stdbool.h>

// Límite máximo para datos APDU en modo corto según 3GPP TS 31.101
#define USIM_APDU_MAX_DATA_LEN 255

// Límite máximo para la respuesta (datos + SW1SW2)
#define USIM_APDU_RESPONSE_MAX_LEN (USIM_APDU_MAX_DATA_LEN + 2U)
#define USIM_APDU_RESPONSE_DATA_MAX (USIM_APDU_MAX_DATA_LEN)

// Estructura comando APDU
typedef struct {
    uint8_t cla;
    uint8_t ins;
    uint8_t p1;
    uint8_t p2;
    uint16_t lc;
    uint8_t* data;
    uint16_t le;
} apdu_command_t;

// Estructura respuesta APDU
typedef struct {
    uint8_t* data;
    uint16_t data_len;
    uint16_t sw1sw2;
} apdu_response_t;

// Prototipos
bool apdu_process_command(uint8_t* command, uint16_t cmd_len, uint8_t* response, uint16_t* resp_len);
bool handle_select_file(apdu_command_t* cmd, apdu_response_t* resp);
bool handle_read_binary(apdu_command_t* cmd, apdu_response_t* resp);
bool handle_authenticate(apdu_command_t* cmd, apdu_response_t* resp);
bool handle_verify_chv(apdu_command_t* cmd, apdu_response_t* resp);
bool handle_change_chv(apdu_command_t* cmd, apdu_response_t* resp);
bool handle_get_response(apdu_command_t* cmd, apdu_response_t* resp);
bool handle_status(apdu_command_t* cmd, apdu_response_t* resp);
bool handle_update_binary(apdu_command_t* cmd, apdu_response_t* resp);

#endif
