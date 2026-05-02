# ============================================================================
# Makefile - Tetris 6502 + SID 6581 para Monitor ROM
# ============================================================================
# Compatible: Linux, Windows (cmd / PowerShell), MinGW / MSYS2
#
# Requiere CC65 (https://cc65.github.io/)
#   Linux:   sudo apt install cc65   o   compilar desde fuente
#   Windows: descargar de GitHub releases y ajustar CC65_HOME si es necesario
#
# Uso:
#   make          - Compilar el programa
#   make clean    - Limpiar archivos generados
#   make info     - Ver tamaño del binario
#   make map      - Ver mapa de memoria
#   make help     - Esta ayuda
#
# Variables sobrescribibles:
#   make CC65_HOME=/opt/cc65        (Linux)
#   make CC65_HOME=C:\cc65          (Windows)
# ============================================================================

# ============================================================
# Detección de Sistema Operativo
# ============================================================
# En Windows, la variable de entorno OS vale "Windows_NT"
# En Linux/Unix, OS no está definida (o vale otra cosa)
# ============================================================
ifeq ($(OS),Windows_NT)
  # ── Windows (cmd / PowerShell) ──────────────────────────
  RM_DIR    = rmdir /s /q
  RM_FILE   = del /q
  NULL_DEV  = nul
else
  # ── Linux / Unix / MinGW-MSYS ───────────────────────────
  RM_DIR    = rm -rf
  RM_FILE   = rm -f
  NULL_DEV  = /dev/null
endif

# ============================================================
# Herramientas CC65
# ============================================================
# Si CC65_HOME está definido, usar rutas absolutas.
# Si no, asumir que cl65/ca65/ld65 están en el PATH.
# ============================================================
CC65_HOME ?=

ifneq ($(CC65_HOME),)
  CC       = $(CC65_HOME)/bin/cl65
  CA65     = $(CC65_HOME)/bin/ca65
  LD65     = $(CC65_HOME)/bin/ld65
  CC65_LIB = $(CC65_HOME)/lib
else
  CC       = cl65
  CA65     = ca65
  LD65     = ld65
  # Intenta rutas comunes; Windows toma lib de PATH automáticamente
  CC65_LIB = /usr/share/cc65/lib
endif

# ============================================================
# Directorios
# ============================================================
SRC_DIR    = src
INC_DIR    = include
CFG_DIR    = config
BUILD_DIR  = build
OUTPUT_DIR = output

# ============================================================
# Archivos de entrada / salida
# ============================================================
PROGRAM    = $(OUTPUT_DIR)/tetris.bin
MAP_FILE   = $(OUTPUT_DIR)/tetris.map
LD_CFG     = $(CFG_DIR)/programa.cfg

C_SRCS     = $(SRC_DIR)/main.c
ASM_SRCS   = $(SRC_DIR)/startup.s

C_OBJS     = $(BUILD_DIR)/main.o
ASM_OBJS   = $(BUILD_DIR)/startup.o
OBJS       = $(ASM_OBJS) $(C_OBJS)

# ============================================================
# Flags de compilación
# ============================================================
CFLAGS     = -t none -O --cpu 6502 -I $(SRC_DIR) -I $(INC_DIR)
ASFLAGS    = -t none --cpu 6502
LDFLAGS    = -C $(LD_CFG) -m $(MAP_FILE) -L $(CC65_LIB)

# ============================================================
# REGLAS PRINCIPALES
# ============================================================
.PHONY: all dirs clean info map help

all: dirs $(PROGRAM)
	@echo ========================================
	@echo Programa generado: $(PROGRAM)
	@echo ========================================
	@echo Para usar:
	@echo   1. Copiar a SD como tetris.bin
	@echo   2. En el monitor:
	@echo      LOAD tetris.bin 0800
	@echo      R 0800
	@echo ========================================

# ── Crear directorios de salida ─────────────────────────
dirs:
ifeq ($(OS),Windows_NT)
	@if not exist "$(BUILD_DIR)" mkdir "$(BUILD_DIR)"
	@if not exist "$(OUTPUT_DIR)" mkdir "$(OUTPUT_DIR)"
else
	@mkdir -p $(BUILD_DIR) $(OUTPUT_DIR)
endif

# ── Compilar C ──────────────────────────────────────────
$(BUILD_DIR)/main.o: $(C_SRCS)
	$(CC) -c $(CFLAGS) -o $@ $<

# ── Ensamblar ───────────────────────────────────────────
$(BUILD_DIR)/startup.o: $(ASM_SRCS)
	$(CA65) $(ASFLAGS) -o $@ $<

# ── Linkar ──────────────────────────────────────────────
$(PROGRAM): $(OBJS)
	$(LD65) $(LDFLAGS) -o $@ $(OBJS) none.lib

# ============================================================
# UTILIDADES
# ============================================================

# ── Tamaño del binario ──────────────────────────────────
info:
	@echo ========================================
	@echo Informacion del programa
	@echo ========================================
ifeq ($(OS),Windows_NT)
	@if exist $(PROGRAM) (for %%I in ($(PROGRAM)) do @echo Tamano: %%~zI bytes) else @echo Error: Programa no compilado
else
	@if [ -f $(PROGRAM) ]; then echo "Tamano: $$(wc -c < $(PROGRAM)) bytes"; else echo "Error: Programa no compilado"; fi
endif

# ── Mapa de memoria ────────────────────────────────────
map:
ifeq ($(OS),Windows_NT)
	@if exist $(MAP_FILE) (type $(MAP_FILE)) else @echo Error: Mapa no encontrado. Compilar primero.
else
	@if [ -f $(MAP_FILE) ]; then cat $(MAP_FILE); else echo "Error: Mapa no encontrado. Compilar primero."; fi
endif

# ── Limpieza ────────────────────────────────────────────
clean:
	-$(RM_DIR) $(BUILD_DIR)
	-$(RM_DIR) $(OUTPUT_DIR)
	@echo Limpieza completa.

# ── Ayuda ───────────────────────────────────────────────
help:
	@echo ========================================
	@echo TETRIS 6502 + SID 6581 - Makefile
	@echo ========================================
	@echo Targets:
	@echo   make        - Compilar el programa
	@echo   make clean  - Limpiar archivos generados
	@echo   make info   - Ver tamano del binario
	@echo   make map    - Ver mapa de memoria
	@echo   make help   - Esta ayuda
	@echo ========================================
	@echo Variables:
	@echo   CC65_HOME   - Ruta de instalacion de CC65
	@echo   Ejemplo: make CC65_HOME=C:\cc65
	@echo ========================================
