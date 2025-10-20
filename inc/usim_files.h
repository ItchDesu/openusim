#ifndef USIM_FILES_H
#define USIM_FILES_H

#include <stdint.h>
#include <stdbool.h>

// Estructura de archivo USIM
typedef struct {
    uint16_t file_id;
    uint8_t file_type;
    uint16_t file_size;
    uint8_t access_conditions;
    uint8_t* file_data;
    uint16_t data_size;
    const char* name;
} usim_file_t;

extern usim_file_t usim_files[];

// Prototipos
void usim_filesystem_init(void);
const usim_file_t* usim_find_file(uint16_t file_id);
usim_file_t* usim_find_file_mutable(uint16_t file_id);
bool usim_check_access(const usim_file_t* file, uint8_t access_type);
void usim_xor_operation(uint8_t* data, uint16_t length, const uint8_t* key, uint8_t key_length);
const usim_file_t* usim_get_current_file(void);

#endif
