#ifndef USIM_CONSTANTS_H
#define USIM_CONSTANTS_H

#ifndef USIM_ENABLE_USAT
#define USIM_ENABLE_USAT 0
#endif

#ifndef USIM_ENABLE_CONFIG_APDU
#define USIM_ENABLE_CONFIG_APDU 0
#endif

// Clases APDU
#define CLA_STANDARD         0x00
#define CLA_GSM              0xA0
#define CLA_USAT             0x80
#define CLA_CONFIG           0x80

// Instrucciones APDU
#define INS_SELECT_FILE      0xA4
#define INS_READ_BINARY      0xB0
#define INS_UPDATE_BINARY    0xD6
#define INS_AUTHENTICATE     0x88
#define INS_VERIFY_CHV       0x20
#define INS_CHANGE_CHV       0x24
#define INS_DISABLE_CHV      0x26
#define INS_ENABLE_CHV       0x28
#define INS_UNBLOCK_CHV      0x2C
#define INS_GET_RESPONSE     0xC0
#define INS_STATUS           0xF2
#define INS_USAT_DATA_DOWNLOAD 0x81
#define INS_USAT_ENVELOPE    0xC3
#define INS_USAT_FETCH       0x12

// Comandos personalizados
#define INS_WRITE_CONFIG     0xD0
#define INS_READ_CONFIG      0xD1
#define INS_XOR_AUTH         0xA0
#define INS_RESET_SIM        0xE0

// Tipos de datos configuración
#define DATA_TYPE_IMSI       0x01
#define DATA_TYPE_KEY        0x02
#define DATA_TYPE_OPC        0x03
#define DATA_TYPE_PIN        0x04
#define DATA_TYPE_STATUS     0x05

// Estados SW1SW2
#define SW_OK                0x9000
#define SW_WRONG_LENGTH      0x6700
#define SW_SECURITY_STATUS_NOT_SATISFIED 0x6982
#define SW_FILE_NOT_FOUND    0x6A82
#define SW_WRONG_PARAMETERS  0x6B00
#define SW_INS_NOT_SUPPORTED 0x6D00
#define SW_CLA_NOT_SUPPORTED 0x6E00
#define SW_COMMAND_NOT_ALLOWED 0x6986
#define SW_AUTHENTICATION_FAILED 0x6300
#define SW_VERIFICATION_FAILED 0x6300
#define SW_MEMORY_PROBLEM    0x9240
#define SW_PIN_BLOCKED       0x6983
#define SW_REMAINING_ATTEMPTS(n) ((uint16_t)(0x63C0 | ((n) & 0x0F)))

// Tipos de archivo
#define FILE_TYPE_MF         0x01
#define FILE_TYPE_DF         0x02
#define FILE_TYPE_EF         0x04

// Condiciones de acceso
#define AC_ALWAYS            0x00
#define AC_NEVER             0xFF
#define AC_CHV1              0x01
#define AC_ADM               0x0F

// Tipos de acceso
#define ACCESS_SELECT        0x01
#define ACCESS_READ          0x02
#define ACCESS_UPDATE        0x04
#define ACCESS_INVALIDATE    0x08

// Tags USAT
#define USAT_TAG_DISPLAY_TEXT    0x21
#define USAT_TAG_GET_INPUT       0x23
#define USAT_TAG_SELECT_ITEM     0x24
#define USAT_TAG_SETUP_MENU      0x25
#define USAT_TAG_SEND_SMS        0x27
#define USAT_RESPONSE_OK         0x00

// Estados USIM
#define USIM_STATE_IDLE          0x00
#define USIM_STATE_SELECTED      0x01
#define USIM_STATE_AUTHENTICATED 0x02
#define USIM_STATE_PIN_VERIFIED  0x04

// Versión
#define USIM_VERSION_MAJOR   2
#define USIM_VERSION_MINOR   0

#endif
