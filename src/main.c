/* ==========================================================================
   TETRIS 6502 + SID 6581 - Versión Final Definitiva (C89 Strict)
   
   Características:
   1. RNG 16-bit con entropía SID OSC3: Secuencias únicas por partida/reinicio.
   2. SRS Completo: Giro horario/anti-horario con tablas de kick oficiales.
   3. Piezas Normalizadas: S, Z, T, J, L, I, O alineadas al borde izquierdo en TODAS las rotaciones.
   4. Renderizado Optimizado: Columna 1 absoluta, borrado de línea \033[K, invalidación de buffer.
   5. Audio SID: Jingle intro, SFX de caída/linea/gameover, semilla desde registro de ruido.
   
   Compilador: CC65 | Target: 6502 @ Tang Nano 9K
   ========================================================================== */
#include <stdint.h>
#include "romapi.h"

#define W 10
#define H 20

static const uint8_t COLORS[8] = {0, 96, 93, 95, 92, 91, 94, 97};

/* ==========================================================================
   CHIP SID 6581 ($D400)
   ========================================================================== */
#define SID_BASE 0xD400
#define SID_FREQ_LO_1 (*(volatile uint8_t *)(SID_BASE + 0x00))
#define SID_FREQ_HI_1 (*(volatile uint8_t *)(SID_BASE + 0x01))
#define SID_PW_LO_1   (*(volatile uint8_t *)(SID_BASE + 0x02))
#define SID_PW_HI_1   (*(volatile uint8_t *)(SID_BASE + 0x03))
#define SID_CTRL_1    (*(volatile uint8_t *)(SID_BASE + 0x04))
#define SID_AD_1      (*(volatile uint8_t *)(SID_BASE + 0x05))
#define SID_SR_1      (*(volatile uint8_t *)(SID_BASE + 0x06))

#define SID_FREQ_LO_2 (*(volatile uint8_t *)(SID_BASE + 0x07))
#define SID_FREQ_HI_2 (*(volatile uint8_t *)(SID_BASE + 0x08))
#define SID_PW_LO_2   (*(volatile uint8_t *)(SID_BASE + 0x09))
#define SID_PW_HI_2   (*(volatile uint8_t *)(SID_BASE + 0x0A))
#define SID_CTRL_2    (*(volatile uint8_t *)(SID_BASE + 0x0B))
#define SID_AD_2      (*(volatile uint8_t *)(SID_BASE + 0x0C))
#define SID_SR_2      (*(volatile uint8_t *)(SID_BASE + 0x0D))

#define SID_FREQ_LO_3 (*(volatile uint8_t *)(SID_BASE + 0x0E))
#define SID_FREQ_HI_3 (*(volatile uint8_t *)(SID_BASE + 0x0F))
#define SID_CTRL_3    (*(volatile uint8_t *)(SID_BASE + 0x12))
#define SID_OSC3      (*(volatile uint8_t *)(SID_BASE + 0x1B))
#define SID_VOL       (*(volatile uint8_t *)(SID_BASE + 0x18))

#define N_E3 0x03D0
#define N_G3 0x0490
#define N_C4 0x0620
#define N_D4 0x06E0
#define N_E4 0x07BA
#define N_G4 0x0930
#define N_A4 0x0A30
#define N_C5 0x0C44
#define N_E5 0x0F80

/* ==========================================================================
   VARIABLES GLOBALES (Declaradas ANTES de cualquier función)
   ========================================================================== */
static uint8_t board[H][W];
static uint8_t prev[H][W];
static uint8_t px, py, ptype, prot, game_over;
static uint16_t score, level, lines_cleared;
static uint16_t frame_cnt, drop_delay;
static uint8_t esc_state;
static uint16_t seed;

/* ==========================================================================
   GENERADOR ALEATORIO (16-bit LCG + Entropía SID)
   ========================================================================== */
static void init_random(void) {
    uint8_t r1, r2, r3;
    /* Configurar Voz 3 para generar ruido real */
    SID_FREQ_LO_3 = 0x15; SID_FREQ_HI_3 = 0x2A; 
    SID_CTRL_3 = 0x80; /* Noise Waveform, Gate Off */
    
    rom_delay_us(40); r1 = SID_OSC3;
    rom_delay_us(40); r2 = SID_OSC3;
    rom_delay_us(40); r3 = SID_OSC3;
    
    SID_CTRL_3 = 0x00; /* Apagar Voz 3 */
    
    /* Mezclar entropía SID con frame_cnt (varía según interacción usuario) */
    seed = ((uint16_t)r1 << 8) | r2;
    seed ^= ((uint16_t)r3 << 8) | frame_cnt;
    if (seed == 0) seed = 0xACE1;
}

static uint8_t prand(void) {
    seed = (seed * 137) + 17;
    return (uint8_t)(seed >> 8);
}

/* ==========================================================================
   AUDIO SID
   ========================================================================== */
static void sid_init(void) {
    SID_VOL = 0x0F;
    SID_AD_1 = 0x0F; SID_SR_1 = 0x01;
    SID_AD_2 = 0x0F; SID_SR_2 = 0x01;
    SID_PW_LO_1 = 0x00; SID_PW_HI_1 = 0x08;
    SID_PW_LO_2 = 0x00; SID_PW_HI_2 = 0x08;
}

static void sid_play_tone(uint16_t f1, uint16_t f2, uint8_t ms) {
    if (f1) { SID_FREQ_LO_1 = (uint8_t)f1; SID_FREQ_HI_1 = (uint8_t)(f1 >> 8); SID_CTRL_1 = 0x41; }
    if (f2) { SID_FREQ_LO_2 = (uint8_t)f2; SID_FREQ_HI_2 = (uint8_t)(f2 >> 8); SID_CTRL_2 = 0x41; }
    rom_delay_ms(ms); SID_CTRL_1 = 0x40; SID_CTRL_2 = 0x40; rom_delay_ms(3);
}

static void sid_play_jingle(void) {
    uint8_t i;
    static const uint16_t v1[] = {N_C4, N_E4, N_G4, N_C5, N_E4, N_C4, 0, 0};
    static const uint16_t v2[] = {N_G3, N_C4, N_E4, N_G4, N_C4, N_G3, 0, 0};
    static const uint8_t  d[]  = {80, 80, 80, 100, 80, 80, 60, 100};
    for (i = 0; i < 8; i++) sid_play_tone(v1[i], v2[i], d[i]);
}

static void sid_sfx(uint8_t type) {
    if (type == 0) sid_play_tone(0x0200, 0x0250, 15);
    else if (type == 1) sid_play_tone(N_C5, N_E5, 40);
    else {
        uint8_t i; static const uint16_t go[] = {N_E4, N_C4, N_G3, N_E3};
        for (i = 0; i < 4; i++) sid_play_tone(go[i], 0, 100);
    }
}
static void sid_off(void) { SID_CTRL_1 = 0x00; SID_CTRL_2 = 0x00; SID_VOL = 0x00; }

/* ==========================================================================
   PIEZAS Y TABLAS SRS
   ========================================================================== */
/* FIX: Todas las rotaciones empiezan en Bit 0 para tocar pared izquierda */
static const uint8_t PIECES[7][4][4] = {
    /* I (Cyan)     */ {{0x00,0x0F,0x00,0x00},{0x01,0x01,0x01,0x01},{0x00,0x0F,0x00,0x00},{0x01,0x01,0x01,0x01}},
    /* O (Amarilla) */ {{0x03,0x03,0x00,0x00},{0x03,0x03,0x00,0x00},{0x03,0x03,0x00,0x00},{0x03,0x03,0x00,0x00}},
    /* T (Magenta)  */ {{0x02,0x07,0x00,0x00},{0x02,0x03,0x02,0x00},{0x07,0x02,0x00,0x00},{0x01,0x03,0x01,0x00}},
    /* S (Verde)    */ {{0x06,0x03,0x00,0x00},{0x01,0x03,0x02,0x00},{0x06,0x03,0x00,0x00},{0x01,0x03,0x02,0x00}},
    /* Z (Roja)     */ {{0x03,0x06,0x00,0x00},{0x02,0x03,0x01,0x00},{0x03,0x06,0x00,0x00},{0x02,0x03,0x01,0x00}},
    /* J (Azul)     */ {{0x04,0x07,0x00,0x00},{0x03,0x02,0x02,0x00},{0x07,0x01,0x00,0x00},{0x01,0x01,0x03,0x00}},
    /* L (Blanca)   */ {{0x01,0x07,0x00,0x00},{0x03,0x02,0x02,0x00},{0x07,0x04,0x00,0x00},{0x01,0x01,0x03,0x00}}
};

/* SRS Clockwise Kicks */
static const int8_t KICKS_JLSTZ_CW[4][5][2] = {
    {{0,0}, {-1,0}, {-1,1}, {0,-2}, {-1,-2}}, {{0,0}, {1,0}, {1,-1}, {0,2}, {1,2}},
    {{0,0}, {1,0}, {1,1}, {0,-2}, {1,-2}}, {{0,0}, {-1,0}, {-1,-1}, {0,2}, {-1,2}}
};
static const int8_t KICKS_I_CW[4][5][2] = {
    {{0,0}, {-2,0}, {1,0}, {-2,-1}, {1,2}}, {{0,0}, {-1,0}, {2,0}, {-1,2}, {2,-1}},
    {{0,0}, {2,0}, {-1,0}, {2,1}, {-1,-2}}, {{0,0}, {1,0}, {-2,0}, {1,-2}, {-2,1}}
};
/* SRS Counter-Clockwise Kicks */
static const int8_t KICKS_JLSTZ_CCW[4][5][2] = {
    {{0,0}, {1,0}, {1,-1}, {0,2}, {1,2}}, {{0,0}, {-1,0}, {-1,1}, {0,-2}, {-1,-2}},
    {{0,0}, {-1,0}, {-1,-1}, {0,2}, {-1,2}}, {{0,0}, {1,0}, {1,1}, {0,-2}, {1,-2}}
};
static const int8_t KICKS_I_CCW[4][5][2] = {
    {{0,0}, {2,0}, {-1,0}, {2,1}, {-1,-2}}, {{0,0}, {1,0}, {-2,0}, {1,-2}, {-2,1}},
    {{0,0}, {-2,0}, {1,0}, {-2,-1}, {1,2}}, {{0,0}, {-1,0}, {2,0}, {-1,2}, {2,-1}}
};

/* ==========================================================================
   UTILIDADES UART / ANSI
   ========================================================================== */
static void uart_ansi_cursor(uint8_t y, uint8_t x) {
    uint8_t t;
    rom_uart_putc('\033'); rom_uart_putc('[');
    t = y / 10; if (t) rom_uart_putc('0' + t);
    rom_uart_putc('0' + (y % 10));
    rom_uart_putc(';');
    t = x / 10; if (t) rom_uart_putc('0' + t);
    rom_uart_putc('0' + (x % 10));
    rom_uart_putc('H');
}
static void uart_set_color(uint8_t id) {
    uint8_t c;
    if (!id) { rom_uart_puts("\033[0m"); return; }
    c = COLORS[id];
    rom_uart_putc('\033'); rom_uart_putc('[');
    rom_uart_putc('0' + (c / 10)); rom_uart_putc('0' + (c % 10));
    rom_uart_putc('m');
}
static void uart_draw_line(uint8_t y) {
    uint8_t x, val;
    uart_ansi_cursor(y + 1, 1); /* Columna 1 = Borde Izquierdo Absoluto */
    for (x = 0; x < W; x++) {
        val = prev[y][x];
        uart_set_color(val);
        if (val) rom_uart_puts("[]"); else rom_uart_puts("  ");
    }
    rom_uart_puts("\033[K");
}

/* ==========================================================================
   LÓGICA DEL JUEGO
   ========================================================================== */
static uint8_t can_place(uint8_t t, uint8_t r, uint8_t x, uint8_t y) {
    uint8_t ry, rx, row, by, bx;
    if (x >= W || y >= H) return 0;
    for (ry = 0; ry < 4; ry++) {
        row = PIECES[t][r][ry];
        if (!row) continue;
        by = y + ry;
        if (by >= H) return 0;
        for (rx = 0; rx < 4; rx++) {
            if ((row >> rx) & 1) {
                bx = x + rx;
                if (bx >= W || board[by][bx]) return 0;
            }
        }
    }
    return 1;
}

static void lock_piece(void) {
    uint8_t ry, rx, row, by;
    for (ry = 0; ry < 4; ry++) {
        row = PIECES[ptype][prot][ry];
        if (!row) continue;
        by = py + ry;
        for (rx = 0; rx < 4; rx++) if ((row >> rx) & 1) board[by][px + rx] = ptype + 1;
    }
    sid_sfx(0);
}

static uint8_t clear_lines(void) {
    uint8_t lines = 0, y, x, full, yy, xx;
    for (y = H - 1; y != 0xFF; y--) {
        full = 1;
        for (x = 0; x < W; x++) if (!board[y][x]) { full = 0; break; }
        if (full) {
            lines++;
            for (yy = y; yy > 0; yy--) for (xx = 0; xx < W; xx++) board[yy][xx] = board[yy - 1][xx];
            for (xx = 0; xx < W; xx++) board[0][xx] = 0;
        }
    }
    if (lines) {
        lines_cleared += lines;
        score += (uint16_t)(lines * 100 * level);
        level = (lines_cleared / 10) + 1;
        drop_delay = 25 / level;
        if (drop_delay < 4) drop_delay = 4;
        sid_sfx(1);
        /* Invalidar buffer para forzar redibujo completo */
        for (y = 0; y < H; y++) for (x = 0; x < W; x++) prev[y][x] = 0xFF;
    }
    return lines;
}

static uint8_t spawn(void) {
    ptype = prand() % 7; prot = 0; px = 3; py = 0;
    return can_place(ptype, prot, px, py);
}

static void try_rotate(int8_t dir) {
    uint8_t nr = (dir > 0) ? (prot + 1) & 3 : (prot + 3) & 3;
    const int8_t (*kicks)[5][2];
    uint8_t t, is_i = (ptype == 0);
    kicks = (dir > 0) ? (is_i ? KICKS_I_CW : KICKS_JLSTZ_CW) 
                      : (is_i ? KICKS_I_CCW : KICKS_JLSTZ_CCW);
    for (t = 0; t < 5; t++) {
        int8_t dx = kicks[prot][t][0], dy = kicks[prot][t][1];
        if (can_place(ptype, nr, (uint8_t)(px + dx), (uint8_t)(py + dy))) {
            prot = nr; px += dx; py += dy; return;
        }
    }
}

static void hard_drop(void) {
    while (can_place(ptype, prot, px, py + 1)) py++;
    lock_piece();
}

static uint8_t get_key(void) {
    char c;
    if (!rom_uart_rx_ready()) return 0;
    c = rom_uart_getc();
    if (esc_state == 0) { if (c == 0x1B) { esc_state = 1; return 0; } return (uint8_t)c; }
    if (esc_state == 1) { if (c == '[') { esc_state = 2; return 0; } esc_state = 0; return 0x1B; }
    esc_state = 0;
    if (c == 'A') return 1; if (c == 'B') return 2; if (c == 'C') return 3; if (c == 'D') return 4;
    return 0;
}

/* ==========================================================================
   MAIN
   ========================================================================== */
int main(void) {
    uint8_t y, x, k, cur, line_dirty;
    char c;

    asm("cli");
    sid_init();
    
    /* Inicializar RNG con entropía SID */
    init_random();
    
    drop_delay = 25; frame_cnt = 0; esc_state = 0;
    
    rom_uart_puts("\033[?25l\033[2J\033[H");
    rom_uart_puts("  TETRIS 6502 + SID\r\n\r\n  WASD/Arrows: Move & Rotate\r\n  Space: Hard Drop\r\n  Z: Rotate CCW\r\n  ESC: Exit\r\n\r\n  Press any key...");
    while (!rom_uart_rx_ready()); rom_uart_getc();
    rom_uart_puts("\033[2J\033[H");
    sid_play_jingle();

reset:
    score = 0; level = 1; lines_cleared = 0; drop_delay = 25; game_over = 0;
    frame_cnt = 0;
    for (y = 0; y < H; y++) for (x = 0; x < W; x++) { board[y][x] = 0; prev[y][x] = 0xFF; }
    if (!spawn()) return 0;

    while (!game_over) {
        k = get_key();
        if (k) {
            if (k == 1 || k == 'w' || k == 'W') try_rotate(1);
            else if (k == 'z' || k == 'Z') try_rotate(-1);
            else if (k == 2 || k == 's' || k == 'S') { if (can_place(ptype, prot, px, py + 1)) py++; }
            else if (k == 3 || k == 'd' || k == 'D') { if (can_place(ptype, prot, px + 1, py)) px++; }
            else if (k == 4 || k == 'a' || k == 'A') { if (can_place(ptype, prot, px - 1, py)) px--; }
            else if (k == ' ') hard_drop();
            else if (k == 0x1B) { rom_uart_puts("\033[?25h\033[2J"); return 0; }
        }

        frame_cnt++;
        if (frame_cnt >= drop_delay) {
            frame_cnt = 0;
            if (can_place(ptype, prot, px, py + 1)) py++;
            else { lock_piece(); clear_lines(); if (!spawn()) game_over = 1; }
        }

        /* Renderizado */
        for (y = 0; y < H; y++) {
            line_dirty = 0;
            for (x = 0; x < W; x++) {
                cur = board[y][x];
                if (y >= py && x >= px && y < py + 4 && x < px + 4) {
                    uint8_t ry = y - py, rx = x - px;
                    if ((PIECES[ptype][prot][ry] >> rx) & 1) cur = ptype + 1;
                }
                if (cur != prev[y][x]) { line_dirty = 1; prev[y][x] = cur; }
            }
            if (line_dirty) uart_draw_line(y);
        }

        /* HUD */
        uart_ansi_cursor(1, 26); rom_uart_puts("\033[0mSCORE: ");
        { uint16_t s = score; rom_uart_putc('0' + (s / 10000)); s %= 10000;
          rom_uart_putc('0' + (s / 1000)); s %= 1000; rom_uart_putc('0' + (s / 100)); s %= 100;
          rom_uart_putc('0' + (s / 10)); s %= 10; rom_uart_putc('0' + s); }
        uart_ansi_cursor(2, 26); rom_uart_puts("LINES:");
        { uint16_t l = lines_cleared; rom_uart_putc('0' + (l / 1000)); l %= 1000;
          rom_uart_putc('0' + (l / 100)); l %= 100; rom_uart_putc('0' + (l / 10)); l %= 10;
          rom_uart_putc('0' + l); }
        uart_ansi_cursor(3, 26); rom_uart_puts("LEVEL:");
        rom_uart_putc('0' + level);
        rom_delay_us(50);
    }

    sid_sfx(2);
    uart_ansi_cursor(H / 2, 26); rom_uart_puts("\033[31;1m*** GAME OVER ***\033[0m");
    uart_ansi_cursor(H / 2 + 2, 26); rom_uart_puts("Press [R]estart or [E]xit");
    while (1) {
        if (!rom_uart_rx_ready()) continue;
        c = rom_uart_getc();
        if (c == 'r' || c == 'R') { 
            init_random(); /* Nueva secuencia aleatoria al reiniciar */
            rom_uart_puts("\033[2J\033[H"); 
            goto reset; 
        }
        if (c == 'e' || c == 'E' || c == 0x1B) { sid_off(); rom_uart_puts("\033[?25h\033[2J"); return 0; }
    }
}