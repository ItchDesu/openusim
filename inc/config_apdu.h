#ifndef CONFIG_APDU_H
#define CONFIG_APDU_H

#include "apdu_handler.h"

// Prototipos configuración
bool handle_write_config(apdu_command_t* cmd, apdu_response_t* resp);
bool handle_read_config(apdu_command_t* cmd, apdu_response_t* resp);
bool handle_xor_auth(apdu_command_t* cmd, apdu_response_t* resp);
bool handle_reset_sim(apdu_command_t* cmd, apdu_response_t* resp);

#endif
