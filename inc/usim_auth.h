#ifndef USIM_AUTH_H
#define USIM_AUTH_H

#include <stdint.h>
#include <stdbool.h>

// Prototipos
bool usim_run_xor_auth(uint8_t rand[16], uint8_t* output, uint16_t* output_len);
void usim_generate_derived_keys(const uint8_t* input, uint16_t input_len, 
                               uint8_t* output, uint16_t output_len);
bool usim_verify_data_integrity(const uint8_t* data, uint16_t data_len, 
                               const uint8_t* expected_mac, uint8_t mac_len);
const uint8_t* usim_get_key(void);
const uint8_t* usim_get_opc(void);

#endif
