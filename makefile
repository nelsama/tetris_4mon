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
#   make          - Compilar el programa (output/tetris.bin)
#   make prg      - Igual, y generar output/tetris.prg (con cabecera $0800)
#   make run      - Generar el PRG y ejecutarlo en VICE (x64sc)
#   make clean    - Limpiar archivos generados
#   make info     - Ver tamaño del binario
#   make map      - Ver mapa de memoria
#   make help     - Esta ayuda
#
# Variables sobrescribibles:
#   make CC65_HOME=/opt/cc65        (Linux)
#   make CC65_HOME=D:/cc65          (Windows, con barras normales)
#   make VICE_HOME=C:/vice          (para 'make run')
#
# En Windows, make detecta solo si corre bajo Git Bash/MSYS2/Cygwin y usa
# cmd.exe para las recetas. Ya no hace falta pasar SHELL=cmd.exe a mano.
# ============================================================================

# ============================================================
# Detección de Sistema Operativo
# ============================================================
# En Windows, la variable de entorno OS vale "Windows_NT"
# En Linux/Unix, OS no está definida (o vale otra cosa)
# ============================================================
ifeq ($(OS),Windows_NT)
  # ── Windows ─────────────────────────────────────────────
  # Git Bash define SHELL=/usr/bin/bash y make lo hereda del entorno, por lo
  # que las recetas se lanzan con sh y fallan con:
  #   /usr/bin/sh: -c: line 2: syntax error: unexpected end of file from `if'
  # Por eso se elige el shell segun el entorno detectado, en vez de confiar en
  # el valor heredado. Si el usuario pasa SHELL=... en la linea de comandos,
  # ese tiene prioridad (origin = command line).
  uname_s := $(shell uname -s 2>/dev/null)
  ifneq ($(origin SHELL),command line)
    ifneq (,$(or $(MSYSTEM),$(findstring msys,$(uname_s)),$(findstring cygwin,$(uname_s)),$(findstring mingw,$(uname_s))))
      SHELL = cmd.exe
    endif
  endif
  with_cmd := $(if $(filter %cmd.exe,$(SHELL)),1,)
  ifeq ($(with_cmd),1)
    RM_DIR    = rmdir /s /q
    RM_FILE   = del /q
    NULL_DEV  = nul
  else
    RM_DIR    = rm -rf
    RM_FILE   = rm -f
    NULL_DEV  = /dev/null
  endif
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
PROGRAM_PRG = $(OUTPUT_DIR)/tetris.prg
MAP_FILE   = $(OUTPUT_DIR)/tetris.map
LD_CFG     = $(CFG_DIR)/programa.cfg
# Cabecera PRG del C64: 2 bytes little-endian con la direccion de carga ($0800)

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

ifeq ($(OS),Windows_NT)
  PRG_TOOL = powershell -NoProfile -Command "[IO.File]::WriteAllBytes('$(PROGRAM_PRG)', ([byte[]](0x00,0x08) + [IO.File]::ReadAllBytes('$(PROGRAM)')))"
else
  PRG_TOOL = printf '\x00\x08' > $(PROGRAM_PRG) && cat $(PROGRAM) >> $(PROGRAM_PRG)
endif

# ============================================================
# REGLAS PRINCIPALES
# ============================================================
.PHONY: all dirs clean info map prg run help

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
# 'mkdir -p' sin mensajes: funciona en sh y en cmd, y no necesita IFEXIST
# (mkdir en un directorio existente solo imprime aviso, que se descarta).
dirs:
	@mkdir -p $(BUILD_DIR) $(OUTPUT_DIR) 2>$(NULL_DEV) || true

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

# ── Version PRG (con cabecera de direccion de carga) ────
# Genera tetris.prg = $00 $08 + tetris.bin
prg: dirs $(PROGRAM)
	$(PRG_TOOL)

# ── Ejecutar en el emulador VICE (C64) ──────────────────
# Requiere x64sc.exe en el PATH (o indicar VICE_HOME)
VICE_HOME ?=
run: prg
	$(if $(VICE_HOME),$(VICE_HOME)/x64sc,x64sc) -autostart $(PROGRAM_PRG)

# ── Tamaño del binario ──────────────────────────────────
# 'for %%I' solo existe en cmd.exe; with_cmd nos dice si las recetas corren
# bajo cmd (Windows nativo) o bajo sh (Linux / Git Bash / MSYS2).
info:
	@echo ========================================
	@echo Informacion del programa
	@echo ========================================
ifeq ($(with_cmd),1)
	@if exist $(PROGRAM) (for %%I in ($(PROGRAM)) do @echo Tamano: %%~zI bytes) else @echo Error: Programa no compilado
else
	@if [ -f $(PROGRAM) ]; then echo "Tamano: $$(wc -c < $(PROGRAM)) bytes"; else echo "Error: Programa no compilado"; fi
endif

# ── Mapa de memoria ────────────────────────────────────
map:
	@cat $(MAP_FILE) 2>$(NULL_DEV) || echo Error: Mapa no encontrado. Compilar primero.

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
