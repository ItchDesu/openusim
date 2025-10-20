#ifndef USIM_APP_H
#define USIM_APP_H

#include <stdint.h>
#include <stdbool.h>

// Estructura de datos del suscriptor
typedef struct {
    uint8_t imsi[16];
    uint8_t key[16];
    uint8_t opc[16];
    uint8_t sqn[6];
    uint8_t amf[2];
    uint8_t pin1[8];
    uint8_t puk1[8];
    uint8_t pin1_retries;
    uint8_t puk1_retries;
} subscriber_data_t;

// Estructura de contexto de sesión
typedef struct {
    uint8_t ck[16];
    uint8_t ik[16];
    uint8_t res[8];
    uint8_t auts[14];
    uint8_t kc[8];
    bool authenticated;
    uint8_t state;
} session_context_t;

// Archivo actual
typedef struct {
    uint16_t file_id;
    uint8_t file_type;
    uint16_t file_size;
} current_file_t;

// Variables globales
extern subscriber_data_t subscriber;
extern session_context_t session;
extern current_file_t current_file;
extern const uint8_t xor_key[16];

// Prototipos
void usim_init(void);
bool usim_receive_apdu(uint8_t* buffer, uint16_t* length);
void usim_send_response(uint8_t* response, uint16_t length);
void usim_background_tasks(void);
const uint8_t* usim_get_file_data(uint16_t file_id, uint8_t* buffer, uint16_t* length);
void usim_update_file(uint16_t file_id, const uint8_t* data, uint16_t length);

#endif
