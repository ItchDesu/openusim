#include "usim_files.h"
#include "usim_constants.h"
#include "usim_app.h"
#include <string.h>

// Archivos USIM según 3GPP TS 31.102
static uint8_t imsi_data[9];
static uint8_t key_data[16];
static uint8_t opc_data[16];
static uint8_t acc_data[2];
static uint8_t loci_data[11];
static uint8_t ad_data[2];
static uint8_t phase_data[1];

static const __code uint8_t imsi_data_init[9] = {0x08, 0x09, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07};
static const __code uint8_t key_data_init[16] = {0x46, 0x5B, 0x5C, 0xE8, 0xB1, 0x99, 0xB4, 0x9F,
                                                 0xAA, 0x5F, 0x0A, 0x2E, 0xE2, 0x38, 0xA6, 0xBC};
static const __code uint8_t opc_data_init[16] = {0xCD, 0x63, 0xCB, 0x71, 0x95, 0x4A, 0x9F, 0x4E,
                                                 0x48, 0xA5, 0x99, 0x4B, 0x86, 0x5A, 0xE9, 0x55};
static const __code uint8_t acc_data_init[2] = {0x00, 0x01};
static const __code uint8_t loci_data_init[11] = {0x07, 0x25, 0x43, 0x10, 0x00, 0x62, 0xF5, 0x35, 0x01, 0x00, 0x00};
static const __code uint8_t ad_data_init[2] = {0x00, 0x00};
static const __code uint8_t phase_data_init[1] = {0x03};

#if USIM_ENABLE_LOGGING
#define FILE_NAME(str) (str)
#else
#define FILE_NAME(str) NULL
#endif

usim_file_t usim_files[] = {
    // MF (Master File) - 3F00
    {0x3F00, FILE_TYPE_MF, 0x0000, AC_ALWAYS, NULL, 0, FILE_NAME("MF")},
    
    // DF_Telecom (7F10)
    {0x7F10, FILE_TYPE_DF, 0x0000, AC_ALWAYS, NULL, 0, FILE_NAME("DF_TELECOM")},
    
    // DF_GSM (7F20) 
    {0x7F20, FILE_TYPE_DF, 0x0000, AC_ALWAYS, NULL, 0, FILE_NAME("DF_GSM")},
    
    // EF_IMSI (6F07) - International Mobile Subscriber Identity
    {0x6F07, FILE_TYPE_EF, 0x0009, AC_CHV1, imsi_data, 9, FILE_NAME("EF_IMSI")},
    
    // EF_KEY (6F08) - Clave de autenticación Ki
    {0x6F08, FILE_TYPE_EF, 0x0010, AC_NEVER, key_data, 16, FILE_NAME("EF_KEY")},
    
    // EF_OPc (6F09) - Parámetro del operador
    {0x6F09, FILE_TYPE_EF, 0x0010, AC_NEVER, opc_data, 16, FILE_NAME("EF_OPC")},
    
    // EF_PLMNwAcT (6F60) - Lista de redes preferidas
    {0x6F60, FILE_TYPE_EF, 0x0016, AC_ALWAYS, NULL, 0, FILE_NAME("EF_PLMN")},
    
    // EF_ACC (6F78) - Access Control Class
    {0x6F78, FILE_TYPE_EF, 0x0002, AC_ALWAYS, acc_data, 2, FILE_NAME("EF_ACC")},
    
    // EF_LOCI (6F7E) - Location Information
    {0x6F7E, FILE_TYPE_EF, 0x000B, AC_CHV1, loci_data, 11, FILE_NAME("EF_LOCI")},
    
    // EF_AD (6FAD) - Administrative Data
    {0x6FAD, FILE_TYPE_EF, 0x0002, AC_ALWAYS, ad_data, 2, FILE_NAME("EF_AD")},
    
    // EF_PHASE (6FAE) - Phase Identification
    {0x6FAE, FILE_TYPE_EF, 0x0001, AC_ALWAYS, phase_data, 1, FILE_NAME("EF_PHASE")},
    
    // Terminador de tabla
{0x0000, 0x00, 0x0000, 0x00, NULL, 0, FILE_NAME("")}
};

#undef FILE_NAME

// Aplicar operación XOR a datos
void usim_xor_operation(uint8_t* data, uint16_t length, const uint8_t* key, uint8_t key_length) {
    {
        uint16_t i;
        for(i = 0U; i < length; i++) {
            data[i] ^= key[i % key_length];
        }
    }
}

// Buscar archivo por ID
const usim_file_t* usim_find_file(uint16_t file_id) {
    {
        uint16_t i = 0U;
        while(usim_files[i].file_id != 0x0000) {
            if(usim_files[i].file_id == file_id) {
                return &usim_files[i];
            }
            i++;
        }
    }
    return NULL;
}

usim_file_t* usim_find_file_mutable(uint16_t file_id) {
    {
        uint16_t i = 0U;
        while(usim_files[i].file_id != 0x0000) {
            if(usim_files[i].file_id == file_id) {
                return &usim_files[i];
            }
            i++;
        }
    }
    return NULL;
}

// Verificar condiciones de acceso
bool usim_check_access(const usim_file_t* file, uint8_t access_type) {
    if(file == NULL) {
        return false;
    }

    if(access_type == ACCESS_SELECT) {
        return true;
    }

    switch(file->access_conditions) {
        case AC_ALWAYS:
            return true;

        case AC_NEVER:
            return false;

        case AC_CHV1:
            return (session.state & USIM_STATE_PIN_VERIFIED) != 0U;

        case AC_ADM:
            return (session.state & USIM_STATE_AUTHENTICATED) != 0U;

        default:
            return false;
    }
}

// Inicializar sistema de archivos
void usim_filesystem_init(void) {
    memcpy(imsi_data, imsi_data_init, sizeof(imsi_data));
    memcpy(key_data, key_data_init, sizeof(key_data));
    memcpy(opc_data, opc_data_init, sizeof(opc_data));
    memcpy(acc_data, acc_data_init, sizeof(acc_data));
    memcpy(loci_data, loci_data_init, sizeof(loci_data));
    memcpy(ad_data, ad_data_init, sizeof(ad_data));
    memcpy(phase_data, phase_data_init, sizeof(phase_data));

    // Aplicar XOR a los datos sensibles durante inicialización
    usim_xor_operation(key_data, 16, xor_key, 16);
    usim_xor_operation(opc_data, 16, xor_key, 16);
}

// Obtener archivo actual
const usim_file_t* usim_get_current_file(void) {
    return usim_find_file(current_file.file_id);
}
