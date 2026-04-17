# 🎮 Tetris 6502 + SID 6581

Un clásico juego de Tetris implementado en **C89 estricto** para el procesador **6502** corriendo sobre el monitor para fpga tang nano 9k, con efectos de sonido generados por el chip **SID 6581**. Diseñado para ejecutarse en la FPGA **Tang Nano 9K** con monitor ROM personalizado.

![Tetris Gameplay](screenshots/gameplay.png)

## ✨ Características

- 🎵 **Audio SID 6581**: Música de introducción y efectos de sonido auténticos
- 🎲 **Generación aleatoria real**: Usa el registro de ruido del SID para secuencias únicas
- 🎯 **SRS (Super Rotation System)**: Sistema de rotación oficial de Tetris con wall-kicks
- 🎨 **Colores ANSI brillantes**: Interfaz visual atractiva en terminal
- ⚡ **Optimizado para 6502**: Código eficiente en C89 estricto compatible con CC65
-  **Controles completos**: Movimiento, rotación horaria/antihoraria, hard drop y soft drop

## 🛠️ Especificaciones Técnicas

### Hardware Objetivo
- **CPU**: MOS 6502 @ 1MHz
- **Audio**: SID 6581 ($D400)
- **Plataforma**: Tang Nano 9K FPGA
- **Display**: Terminal ANSI compatible

### Software
- **Compilador**: CC65 (`cl65`)
- **Lenguaje**: C89 estricto + ensamblador inline
- **Dependencias**: `romapi.h` (proporcionada por el monitor ROM)

##  Requisitos

### Para Compilar
- [CC65](https://cc65.github.io/) compilador cruzado para 6502
- Make (en Windows: included con Git o WSL)
- Conexión serial al dispositivo Tang Nano 9K

### Para Ejecutar
- Tang Nano 9K con monitor ROM cargado
- Terminal compatible con ANSI escape codes
- Conexión serial configurada (115200 baudios recomendado)

## 🚀 Instalación y Compilación

1. **Clonar el repositorio**:
```bash
git clone https://github.com/nelsama/tetris_4mon.git
cd tetris-6502-sid