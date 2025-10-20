#ifndef USAT_HANDLER_H
#define USAT_HANDLER_H

#include "apdu_handler.h"

// Prototipos USAT
bool usat_handle_data_download(apdu_command_t* cmd, apdu_response_t* resp);
bool usat_handle_envelope(apdu_command_t* cmd, apdu_response_t* resp);
bool usat_handle_fetch(apdu_command_t* cmd, apdu_response_t* resp);
void usat_background_processing(void);

#endif
