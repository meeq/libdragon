/**
 * @file accessorytest.c
 * @author Christopher Bonhage (me@christopherbonhage.com)
 * @brief Accessory test ROM
 *
 * This example allows you to read and write raw data from the
 * accessories connected to N64 controllers. All read and write
 * operations are 32-byte blocks, and the address is a 16-bit value.
 *
 * The interface shows the current controller port, accessory address,
 * a buffer of data to write, and a buffer of data read from the accessory.
 */

#include <string.h>
#include <libdragon.h>

#define BLACK 0x000000FF
#define WHITE 0xFFFFFFFF

#define GLYPH_WIDTH  8
#define GLYPH_HEIGHT 8

#define X_LABEL (3 * GLYPH_WIDTH)
#define X_VALUE (320 - (3 * GLYPH_WIDTH))
#define X_RIGHT(str) (X_VALUE - (strlen(str) * GLYPH_WIDTH))

#define Y_HEADER (0 * GLYPH_HEIGHT)
#define Y_TITLE (2 * GLYPH_HEIGHT)
#define Y_PORT (4 * GLYPH_HEIGHT)
#define Y_ADDR (5 * GLYPH_HEIGHT)
#define Y_WRITEBUF_LABEL (7 * GLYPH_HEIGHT)
#define Y_WRITEBUF_DATA_1 (9 * GLYPH_HEIGHT)
#define Y_WRITEBUF_DATA_2 (10 * GLYPH_HEIGHT)
#define Y_WRITEBUF_DATA_3 (11 * GLYPH_HEIGHT)
#define Y_WRITEBUF_DATA_4 (12 * GLYPH_HEIGHT)
#define Y_READBUF_LABEL (14 * GLYPH_HEIGHT)
#define Y_READBUF_DATA_1 (16 * GLYPH_HEIGHT)
#define Y_READBUF_DATA_2 (17 * GLYPH_HEIGHT)
#define Y_READBUF_DATA_3 (18 * GLYPH_HEIGHT)
#define Y_READBUF_DATA_4 (19 * GLYPH_HEIGHT)
#define Y_STATUS (21 * GLYPH_HEIGHT)
#define Y_INSTRUCTIONS_1 (25 * GLYPH_HEIGHT)
#define Y_INSTRUCTIONS_2 (26 * GLYPH_HEIGHT)
#define Y_INSTRUCTIONS_3 (27 * GLYPH_HEIGHT)
#define Y_INSTRUCTIONS_4 (28 * GLYPH_HEIGHT)

typedef enum {
    FOCUS_PORT,
    FOCUS_ADDR,
    FOCUS_WRITEBUF,
} focus_t;

static focus_t focus = FOCUS_PORT;
static size_t focus_addr = 0;
static size_t focus_write = 0;
static joypad_port_t port = JOYPAD_PORT_1;
static uint16_t addr = 0x0000;
static uint8_t write_buf[JOYBUS_ACCESSORY_DATA_SIZE] = {0};
static uint16_t read_addr = 0x0000;
static bool read_data_valid = false;
static uint8_t read_buf[JOYBUS_ACCESSORY_DATA_SIZE] = {0};
static const char *status_msg = NULL;
static int status_timer = 0;

void focus_adjust(int incr) {
    int result = focus + incr;
    if (result > FOCUS_WRITEBUF) result = FOCUS_PORT;
    if (result < FOCUS_PORT) result = FOCUS_WRITEBUF;
    focus = result;
}

void port_adjust(int incr) {
    int result = port + incr;
    if (result > JOYPAD_PORT_4) result = JOYPAD_PORT_1;
    if (result < JOYPAD_PORT_1) result = JOYPAD_PORT_4;
    port = result;
}

void addr_nibble_adjust(int incr)
{
    int shift = (3 - focus_addr) * 4;
    int nibble = (addr >> shift) & 0xF;
    // Only allow the address offset bits to be set
    if (focus_addr < 2) {
        nibble = (nibble + incr) & 0xF;
    } else if (focus_addr == 2) {
        // For the second-to-least-significant nibble, only allow even values
        nibble = (nibble + (incr * 2)) & 0xE;
    }
    // Update the address with the new nibble
    uint16_t mask = ~(0xF << shift);
    addr = (addr & mask) | (nibble << shift);
}

void write_nibble_adjust(int incr)
{
    int byte_idx = focus_write / 2;
    int is_high_nibble = (focus_write % 2) == 0;
    int shift = is_high_nibble ? 4 : 0;
    int nibble = (write_buf[byte_idx] >> shift) & 0xF;
    nibble = (nibble + incr) & 0xF;
    uint8_t mask = ~(0xF << shift);
    write_buf[byte_idx] = (write_buf[byte_idx] & mask) | (nibble << shift);
}

void write_nibble_fill(void) {
    uint8_t fill_byte = write_buf[focus_write / 2];
    memset(write_buf, fill_byte, sizeof(write_buf));
    status_msg = "Buffer filled";
    status_timer = 60;
}

void accessory_read(void) {
    read_addr = addr;
    int result = joybus_accessory_read(port, addr, read_buf);
    read_data_valid = false;
    if (!joypad_is_connected(port)) {
        status_msg = "Error: No controller in port";
    } else if (result == JOYBUS_ACCESSORY_IO_STATUS_OK) {
        read_data_valid = true;
        status_msg = "Read successful";
    } else if (result == JOYBUS_ACCESSORY_IO_STATUS_NO_PAK) {
        status_msg = "Error: No accessory in controller";
    } else if (result == JOYBUS_ACCESSORY_IO_STATUS_BAD_CRC) {
        status_msg = "Error: Bad read response CRC";
    } else {
        status_msg = "Error: Read failed";
    }
    status_timer = 240;
}

void accessory_write(void) {
    int result = joybus_accessory_write(port, addr, write_buf);
    if (!joypad_is_connected(port)) {
        status_msg = "Error: No controller in port";
    } else if (result == JOYBUS_ACCESSORY_IO_STATUS_OK) {
        accessory_read(); // Read back to verify
        status_msg = "Write successful";
    } else if (result == JOYBUS_ACCESSORY_IO_STATUS_NO_PAK) {
        status_msg = "Error: No accessory in controller";
    } else if (result == JOYBUS_ACCESSORY_IO_STATUS_BAD_CRC) {
        status_msg = "Error: Bad write response CRC";
    } else {
        status_msg = "Error: Write failed";
    }
    status_timer = 240;
}

void draw_hex_with_highlight(display_context_t disp, int x, int y, const char *str, int highlight_pos) {
    for (int i = 0; str[i]; i++) {
        if (i == highlight_pos) {
            graphics_set_color(BLACK, WHITE);
        } else {
            graphics_set_color(WHITE, BLACK);
        }
        char ch[2] = {str[i], 0};
        graphics_draw_text(disp, x + i * GLYPH_WIDTH, y, ch);
    }
    graphics_set_color(WHITE, BLACK);
}

int main(void) {
    display_context_t disp;
    joypad_buttons_t p1_pressed;
    int p1_stick_y_pressed;
    int p1_stick_x_pressed;
    int nibble_pos;
    char port_str[sizeof("1")];
    char addr_str[sizeof("0x0000")];
    char hex_line[sizeof("00 00 00 00 00 00 00 00")];

    timer_init();
    display_init(RESOLUTION_320x240, DEPTH_32_BPP, 2, GAMMA_NONE, ANTIALIAS_RESAMPLE);
    debug_init_isviewer();
    debug_init_usblog();
    debugf("Accessory Test ROM started\n");

    joypad_init();

    while (1) {
        disp = display_get();
        joypad_poll();
        p1_pressed = joypad_get_buttons_pressed(JOYPAD_PORT_1);
        p1_stick_y_pressed = joypad_get_axis_pressed(JOYPAD_PORT_1, JOYPAD_AXIS_STICK_Y);
        p1_stick_x_pressed = joypad_get_axis_pressed(JOYPAD_PORT_1, JOYPAD_AXIS_STICK_X);

        graphics_fill_screen(disp, BLACK);

        // Draw Labels
        graphics_set_color(WHITE, BLACK);
        graphics_draw_text(disp, X_LABEL, Y_HEADER, "LibDragon Accessory Test");
        graphics_draw_text(disp, X_LABEL, Y_PORT, "Controller Port:");
        graphics_draw_text(disp, X_LABEL, Y_ADDR, "Accessory Address:");
        graphics_draw_text(disp, X_LABEL, Y_WRITEBUF_LABEL, "Write Buffer:");
        graphics_draw_text(disp, X_LABEL, Y_READBUF_LABEL, "Read Buffer:");

        // Draw instructions
        graphics_draw_text(disp, X_LABEL, Y_INSTRUCTIONS_1, "A: Accessory Read B: Accessory Write");
        graphics_draw_text(disp, X_LABEL, Y_INSTRUCTIONS_2, "Stick/D-Pad Up/Down: Move Cursor");
        if (focus == FOCUS_PORT) {
            graphics_draw_text(disp, X_LABEL, Y_INSTRUCTIONS_3, "C-Up/Down: Change Controller Port");
        } else if (focus == FOCUS_ADDR) {
            graphics_draw_text(disp, X_LABEL, Y_INSTRUCTIONS_3, "Left/Right: Move Cursor");
            graphics_draw_text(disp, X_LABEL, Y_INSTRUCTIONS_4, "C-Up/Down: Change Value");
        } else if (focus == FOCUS_WRITEBUF) {
            graphics_draw_text(disp, X_LABEL, Y_INSTRUCTIONS_3, "Left/Right: Move Cursor");
            graphics_draw_text(disp, X_LABEL, Y_INSTRUCTIONS_4, "C-Up/Down: Change Value  R: Fill");
        }

        // Draw Port Field
        sprintf(port_str, "%1d", port + 1);
        if (focus == FOCUS_PORT) {
            graphics_set_color(BLACK, WHITE);
        } else {
            graphics_set_color(WHITE, BLACK);
        }
        graphics_draw_text(disp, X_RIGHT(port_str), Y_PORT, port_str);
        graphics_set_color(WHITE, BLACK);

        // Draw Address Field
        sprintf(addr_str, "0x%04X", addr);
        if (focus == FOCUS_ADDR) {
            draw_hex_with_highlight(disp, X_RIGHT(addr_str), Y_ADDR, addr_str, 2 + focus_addr);
        } else {
            graphics_draw_text(disp, X_RIGHT(addr_str), Y_ADDR, addr_str);
        }

        // Draw Write Buffer Data Line 1
        sprintf(hex_line, "%02X %02X %02X %02X %02X %02X %02X %02X",
            write_buf[0], write_buf[1], write_buf[2], write_buf[3],
            write_buf[4], write_buf[5], write_buf[6], write_buf[7]);
        if (focus == FOCUS_WRITEBUF && focus_write < 16) {
            nibble_pos = (focus_write / 2) * 3 + (focus_write % 2);
            draw_hex_with_highlight(disp, X_LABEL, Y_WRITEBUF_DATA_1, hex_line, nibble_pos);
        } else {
            graphics_draw_text(disp, X_LABEL, Y_WRITEBUF_DATA_1, hex_line);
        }

        // Draw Write Buffer Data Line 2
        sprintf(hex_line, "%02X %02X %02X %02X %02X %02X %02X %02X",
            write_buf[8], write_buf[9], write_buf[10], write_buf[11],
            write_buf[12], write_buf[13], write_buf[14], write_buf[15]);
        if (focus == FOCUS_WRITEBUF && focus_write >= 16 && focus_write < 32) {
            nibble_pos = ((focus_write - 16) / 2) * 3 + ((focus_write - 16) % 2);
            draw_hex_with_highlight(disp, X_LABEL, Y_WRITEBUF_DATA_2, hex_line, nibble_pos);
        } else {
            graphics_draw_text(disp, X_LABEL, Y_WRITEBUF_DATA_2, hex_line);
        }

        // Draw Write Buffer Data Line 3
        sprintf(hex_line, "%02X %02X %02X %02X %02X %02X %02X %02X",
            write_buf[16], write_buf[17], write_buf[18], write_buf[19],
            write_buf[20], write_buf[21], write_buf[22], write_buf[23]);
        if (focus == FOCUS_WRITEBUF && focus_write >= 32 && focus_write < 48) {
            nibble_pos = ((focus_write - 32) / 2) * 3 + ((focus_write - 32) % 2);
            draw_hex_with_highlight(disp, X_LABEL, Y_WRITEBUF_DATA_3, hex_line, nibble_pos);
        } else {
            graphics_draw_text(disp, X_LABEL, Y_WRITEBUF_DATA_3, hex_line);
        }

        // Draw Write Buffer Data Line 4
        sprintf(hex_line, "%02X %02X %02X %02X %02X %02X %02X %02X",
            write_buf[24], write_buf[25], write_buf[26], write_buf[27],
            write_buf[28], write_buf[29], write_buf[30], write_buf[31]);
        if (focus == FOCUS_WRITEBUF && focus_write >= 48) {
            nibble_pos = ((focus_write - 48) / 2) * 3 + ((focus_write - 48) % 2);
            draw_hex_with_highlight(disp, X_LABEL, Y_WRITEBUF_DATA_4, hex_line, nibble_pos);
        } else {
            graphics_draw_text(disp, X_LABEL, Y_WRITEBUF_DATA_4, hex_line);
        }

        // Draw Read Buffer Data
        graphics_set_color(WHITE, BLACK);
        if (read_data_valid) {
            sprintf(addr_str, "0x%04X", read_addr);
            graphics_draw_text(disp, X_RIGHT(addr_str), Y_READBUF_LABEL, addr_str);
            sprintf(hex_line, "%02X %02X %02X %02X %02X %02X %02X %02X",
                read_buf[0], read_buf[1], read_buf[2], read_buf[3],
                read_buf[4], read_buf[5], read_buf[6], read_buf[7]);
            graphics_draw_text(disp, X_LABEL, Y_READBUF_DATA_1, hex_line);
            sprintf(hex_line, "%02X %02X %02X %02X %02X %02X %02X %02X",
                read_buf[8], read_buf[9], read_buf[10], read_buf[11],
                read_buf[12], read_buf[13], read_buf[14], read_buf[15]);
            graphics_draw_text(disp, X_LABEL, Y_READBUF_DATA_2, hex_line);
            sprintf(hex_line, "%02X %02X %02X %02X %02X %02X %02X %02X",
                read_buf[16], read_buf[17], read_buf[18], read_buf[19],
                read_buf[20], read_buf[21], read_buf[22], read_buf[23]);
            graphics_draw_text(disp, X_LABEL, Y_READBUF_DATA_3, hex_line);
            sprintf(hex_line, "%02X %02X %02X %02X %02X %02X %02X %02X",
                read_buf[24], read_buf[25], read_buf[26], read_buf[27],
                read_buf[28], read_buf[29], read_buf[30], read_buf[31]);
            graphics_draw_text(disp, X_LABEL, Y_READBUF_DATA_4, hex_line);
        } else {
            graphics_draw_text(disp, X_LABEL, Y_READBUF_DATA_1, "<No data>");
        }

        // Draw and update status message
        if (status_msg && status_timer > 0) {
            graphics_draw_text(disp, X_LABEL, Y_STATUS, status_msg);
            status_timer--;
            if (status_timer == 0) status_msg = NULL;
        }

        // Handle input
        if (p1_pressed.d_up || p1_stick_y_pressed > 0) focus_adjust(-1);
        if (p1_pressed.d_down || p1_stick_y_pressed < 0) focus_adjust(+1);
        if (focus == FOCUS_PORT) {
            if (p1_pressed.c_down) port_adjust(-1);
            if (p1_pressed.c_up) port_adjust(+1);
        }
        else if (focus == FOCUS_ADDR) {
            const int FOCUS_ADDR_NIBBLE_MAX = sizeof(addr) * 2 - 2;
            if (p1_pressed.d_left || p1_pressed.c_left || p1_stick_x_pressed < 0) {
                if (focus_addr > 0) focus_addr--;
                else focus_addr = FOCUS_ADDR_NIBBLE_MAX;
            }
            if (p1_pressed.d_right || p1_pressed.c_right || p1_stick_x_pressed > 0) {
                if (focus_addr < FOCUS_ADDR_NIBBLE_MAX) focus_addr++;
                else focus_addr = 0;
            }
            if (p1_pressed.c_down) addr_nibble_adjust(-1);
            if (p1_pressed.c_up) addr_nibble_adjust(+1);
        }
        else if (focus == FOCUS_WRITEBUF) {
            const int FOCUS_WRITE_NIBBLE_MAX = sizeof(write_buf) * 2 - 1;
            if (p1_pressed.d_left || p1_pressed.c_left || p1_stick_x_pressed < 0) {
                if (focus_write > 0) focus_write--;
                else focus_write = FOCUS_WRITE_NIBBLE_MAX;
            }
            if (p1_pressed.d_right || p1_pressed.c_right || p1_stick_x_pressed > 0) {
                if (focus_write < FOCUS_WRITE_NIBBLE_MAX) focus_write++;
                else focus_write = 0;
            }
            if (p1_pressed.c_down) write_nibble_adjust(-1);
            if (p1_pressed.c_up) write_nibble_adjust(+1);
            if (p1_pressed.r) write_nibble_fill();
        }
        if (p1_pressed.a) accessory_read();
        if (p1_pressed.b) accessory_write();

        display_show(disp);
    }
}
