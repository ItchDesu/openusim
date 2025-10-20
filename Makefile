# Makefile para USIM COS THC20F17BD-V40 con SDCC
PROJECT = usim_cos_thc20f17bd
MCU_MODEL = thc20f17bd

# Herramientas
CC = sdcc
PKGIHX = packihx
OBJCOPY = sdobjcopy
SIZE = size

# Directorios
SRC_DIR = src
INC_DIR = inc
OBJ_DIR = obj
BIN_DIR = bin
CONFIG_DIR = config

# Flags de compilación
CFLAGS = -mmcs51 --model-large --stack-auto --opt-code-size \
         -I$(INC_DIR) -I$(CONFIG_DIR) \
         -DTHC20F17BD -DUSIM_VERSION=200 \
         --std-sdcc11 --fomit-frame-pointer

# Flags de enlazado
# El microcontrolador THC20F17BD dispone de 132 KB de Flash totales, pero el
# núcleo 8051 solamente puede direccionar 64 KB lineales. Ajustamos el tamaño
# de código máximo al rango completo de 64 KB disponible sin banking y ampliamos
# la RAM externa a los 2048 bytes descritos en la hoja de datos.
LDFLAGS = -mmcs51 --model-large --stack-auto --out-fmt-ihx \
          --code-loc 0x0000 --code-size 0x10000 \
          --xram-loc 0x0000 --xram-size 0x0800 \
          --iram-size 0x0100

# Archivos fuente
SRCS = $(SRC_DIR)/main.c \
       $(SRC_DIR)/chip_init.c \
       $(SRC_DIR)/usim_app.c \
       $(SRC_DIR)/usim_files.c \
       $(SRC_DIR)/usim_auth.c \
       $(SRC_DIR)/apdu_handler.c \
       $(SRC_DIR)/usat_handler.c \
       $(SRC_DIR)/config_apdu.c \
       $(CONFIG_DIR)/file_system.c

# Archivos objeto
OBJS = $(patsubst $(SRC_DIR)/%.c,$(OBJ_DIR)/%.rel,$(filter $(SRC_DIR)/%,$(SRCS)))
OBJS += $(patsubst $(CONFIG_DIR)/%.c,$(OBJ_DIR)/%.rel,$(filter $(CONFIG_DIR)/%,$(SRCS)))

# Objetivo principal
all: create_dirs $(BIN_DIR)/$(PROJECT).hex size_info

create_dirs:
	@mkdir -p $(OBJ_DIR) $(BIN_DIR)

$(OBJ_DIR)/%.rel: $(SRC_DIR)/%.c | create_dirs
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJ_DIR)/%.rel: $(CONFIG_DIR)/%.c | create_dirs
	$(CC) $(CFLAGS) -c $< -o $@

$(BIN_DIR)/$(PROJECT).ihx: $(OBJS)
	$(CC) $(LDFLAGS) $(OBJS) -o $@

$(BIN_DIR)/$(PROJECT).hex: $(BIN_DIR)/$(PROJECT).ihx
	$(PKGIHX) $< > $@

size_info: $(BIN_DIR)/$(PROJECT).ihx
	@echo "=== Información de Tamaño ==="
	@python3 scripts/check_size.py $(BIN_DIR)/$(PROJECT).ihx

clean:
	rm -rf $(OBJ_DIR) $(BIN_DIR)
	find . -name "*.asm" -delete
	find . -name "*.lst" -delete
	find . -name "*.rst" -delete
	find . -name "*.sym" -delete
	find . -name "*.mem" -delete

flash: $(BIN_DIR)/$(PROJECT).hex
	@echo "Flasheando tarjeta THC20F17BD-V40..."
	chipsea_isp_tool -p /dev/ttyUSB0 -b 115200 -f $(BIN_DIR)/$(PROJECT).hex

configure:
	@echo "Configurando SIM con datos Open5GS..."
	@python3 scripts/configure_sim.py

deploy: clean all flash configure
	@echo "✅ Despliegue completo realizado"

monitor:
	@echo "Iniciando monitor serial..."
	picocom -b 115200 /dev/ttyUSB0

debug: CFLAGS += -DDEBUG --debug
debug: all

release: CFLAGS += -DNDEBUG --opt-code-speed
release: all

check-syntax:
	@echo "=== Verificando sintaxis ==="
	@mkdir -p $(OBJ_DIR)/syntax_check
	@status=0; \
	for file in $(SRCS); do \
	base=$$(basename $$file .c); \
echo "📝 Verificando: $$file"; \
	if $(CC) $(CFLAGS) -c $$file -o $(OBJ_DIR)/syntax_check/$$base.rel; then \
	echo "✅ OK"; \
	else \
	echo "❌ ERROR"; \
	status=1; \
	fi; \
	done; \
	rm -f $(OBJ_DIR)/syntax_check/*; \
	rmdir $(OBJ_DIR)/syntax_check 2>/dev/null || true; \
	exit $$status

minimal: create_dirs $(OBJ_DIR)/main.rel $(OBJ_DIR)/chip_init.rel \
	         $(OBJ_DIR)/usim_app.rel $(OBJ_DIR)/usim_files.rel
	@echo "✅ Compilación mínima completada"

step1:
	@echo "🔨 Paso 1: Compilando main.c..."
	$(CC) $(CFLAGS) -c $(SRC_DIR)/main.c -o $(OBJ_DIR)/main.rel

step2:
	@echo "🔨 Paso 2: Compilando chip_init.c..."
	$(CC) $(CFLAGS) -c $(SRC_DIR)/chip_init.c -o $(OBJ_DIR)/chip_init.rel

step3:
	@echo "🔨 Paso 3: Compilando usim_app.c..."
	$(CC) $(CFLAGS) -c $(SRC_DIR)/usim_app.c -o $(OBJ_DIR)/usim_app.rel

step4:
	@echo "🔨 Paso 4: Compilando usim_files.c..."
	$(CC) $(CFLAGS) -c $(SRC_DIR)/usim_files.c -o $(OBJ_DIR)/usim_files.rel

step5:
	@echo "🔨 Paso 5: Enlazando..."
	$(CC) $(LDFLAGS) $(OBJ_DIR)/main.rel $(OBJ_DIR)/chip_init.rel \
	$(OBJ_DIR)/usim_app.rel $(OBJ_DIR)/usim_files.rel -o $(BIN_DIR)/$(PROJECT).ihx
	$(PKGIHX) $(BIN_DIR)/$(PROJECT).ihx > $(BIN_DIR)/$(PROJECT).hex

.PHONY: all create_dirs clean flash configure deploy monitor debug release \
	        minimal step1 step2 step3 step4 step5 size_info check-syntax
