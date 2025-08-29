/**
 * @file accessoryshorttest.c
 * @author Christopher Bonhage (me@christopherbonhage.com)
 * @brief N64 test ROM for Accessory Short Reads
 */

#include <string.h>
#include <libdragon.h>

#include "../../src/joybus/joybus_accessory_internal.h"
#include "../../src/joybus/joybus_commands.h"

static void wait_for_button_press(joypad_port_t port)
{
    joypad_buttons_t btn;
    while (1)
    {
        joypad_poll();
        btn = joypad_get_buttons_pressed(port);
        if (btn.raw) break;
    }
}

static void draw_joybus_buffers(
    const void *input_buf,
    const void *output_buf
)
{
    const uint8_t *input = (const uint8_t *)input_buf;
    const uint8_t *output = (const uint8_t *)output_buf;

    printf("   Input Buffer                      Output Buffer\n");
    printf("   00 01 02 03 04 05 06 07           00 01 02 03 04 05 06 07\n");
    printf("   ------------------------          ------------------------\n");

    for (size_t row = 0; row < 8; row++)
    {
        printf("%02X:", (unsigned int)(row * 8));

        // Draw input buffer row
        for (size_t col = 0; col < 8; col++)
        {
            size_t idx = row * 8 + col;
            printf(" %02X", input[idx]);
        }

        printf("       %02X:", (unsigned int)(row * 8));

        // Draw output buffer row
        for (size_t col = 0; col < 8; col++)
        {
            size_t idx = row * 8 + col;
            printf(" %02X", output[idx]);
        }

        printf("\n");
    }
}

static void joybus_build_cmd(
    int port,
    size_t send_len,
    size_t recv_len,
    const void *send_data,
    void *input_buf
)
{
    uint8_t *input = (uint8_t *)input_buf;
    // Validate the desired Joybus port offset
    assert((port >= 0) && (port < JOYBUS_PORT_COUNT));
    // Ensure the send_len and recv_len fit in the Joybus operation block
    assert((port + send_len + recv_len) < (JOYBUS_BLOCK_SIZE - 4));
    // Skip commands on ports before the desired port offset
    size_t i = port;
    // Set the command metadata
    input[i++] = send_len;
    input[i++] = recv_len;
    // Copy the send_data into the input buffer
    memcpy(&input[i], send_data, send_len);
    i += send_len + recv_len;
    // Close out the Joybus operation block
    input[i] = 0xFE;
    input[JOYBUS_BLOCK_SIZE - 1] = 0x01;
}

static void accessory_short_read_build_cmd(
    int port,
    uint16_t addr,
    size_t data_len,
    void *input_buf
)
{
    assert(data_len <= JOYBUS_ACCESSORY_DATA_SIZE);
    joybus_cmd_n64_accessory_read_port_t cmd = { .send = {
        .command = JOYBUS_COMMAND_ID_N64_ACCESSORY_READ,
        .addr_checksum = joybus_accessory_calculate_addr_checksum(addr),
    } };
    size_t recv_len = data_len + (data_len == JOYBUS_ACCESSORY_DATA_SIZE ? 1 : 0);
    joybus_build_cmd(port, sizeof(cmd.send), recv_len, &cmd.send, input_buf);
}

static void accessory_short_read_parse_cmd(
    int port,
    size_t data_len,
    const void *output_buf,
    void *data_out,
    uint8_t *data_crc_out
)
{
    assert(data_len <= JOYBUS_ACCESSORY_DATA_SIZE);
    const uint8_t *output = (const uint8_t *)output_buf;
    size_t recv_start = port + JOYBUS_COMMAND_METADATA_SIZE + 3;
    if (data_len > 0) memcpy(data_out, &output[recv_start], data_len);
    if (data_crc_out) *data_crc_out = output[recv_start + data_len];
}

static void accessory_short_read_test_mode(joypad_port_t port, uint16_t addr, size_t data_len)
{
    uint8_t data[JOYBUS_ACCESSORY_DATA_SIZE] = {0};
    uint8_t joybus_input[JOYBUS_BLOCK_SIZE] = {0};
    uint8_t joybus_output[JOYBUS_BLOCK_SIZE] = {0};

    console_clear();
    printf("\n");
    printf("Accessory Short Read Test Mode\n");
    printf("==============================\n\n");

    printf("Attempting to read %d bytes starting from 0x%04X on port %d\n\n", data_len, addr, port + 1);
    console_render();

    accessory_short_read_build_cmd(port, addr, data_len, joybus_input);

    int32_t start = get_ticks();
    joybus_exec(joybus_input, joybus_output);
    int32_t end = get_ticks();

    printf("Execution time: %u microseconds\n\n", TIMER_MICROS(end - start));

    // Draw the Joybus buffers
    draw_joybus_buffers(joybus_input, joybus_output);

    // Display the read data
    printf("\n\n");
    printf("Accessory Read Data (%02d bytes):                 \n", data_len);
    printf("00 01 02 03 04 05 06 07 08 09 0A 0B 0C 0D 0E 0F \n");
    printf("------------------------------------------------\n");

    accessory_short_read_parse_cmd(port, data_len, joybus_output, data, NULL);
    for (int i = 0; i < data_len; i++)
    {
        printf("%02X ", data[i]);
        if ((i & 0x0F) == 0x0F) printf("\n");
    }
    if (data_len & 0x0F) printf("\n");

    printf("\nPress any button to return...\n");
    console_render();
    wait_for_button_press(port);
}

static void accessory_short_read_crc_fuzz_mode(joypad_port_t port, size_t data_len)
{
    uint16_t addr = JOYBUS_ACCESSORY_ADDR_LABEL;
    uint8_t data[JOYBUS_ACCESSORY_DATA_SIZE] = {0};
    uint8_t data_crc;
    uint8_t joybus_input[JOYBUS_BLOCK_SIZE] = {0};
    uint8_t joybus_output[JOYBUS_BLOCK_SIZE] = {0};

    console_clear();
    printf("\n");
    printf("Accessory Short Read CRC Fuzz Mode\n");
    printf("==================================\n\n");

    printf("Attempting to fuzz %d bytes starting from 0x0000 on port %d\n\n", data_len, port + 1);
    console_render();

    for (uint8_t i = 0; i < 0xFF; i++)
    {
        data[0] = i;
        for (int j = 1; j < JOYBUS_ACCESSORY_DATA_SIZE; j++) data[j] = j;
        joybus_accessory_write(port, addr, data);
        accessory_short_read_build_cmd(port, addr, data_len, joybus_input);
        joybus_exec(joybus_input, joybus_output);
        accessory_short_read_parse_cmd(port, data_len, joybus_output, data, &data_crc);
        printf("0x%02X: CRC: %02X  ", i, data_crc);
        if (i % 4 == 3) printf("\n");
        console_render();
        if (i % 32 == 31)
        {
            printf("\nPress any button to continue...\n");
            console_render();
            wait_for_button_press(port);
        }
    }

    printf("\nPress any button to return...\n");
    console_render();
    wait_for_button_press(port);
}

int main(void)
{
    joypad_buttons_t btn;

    joypad_init();
    debug_init_isviewer();
    debug_init_usblog();
    console_init();
    console_set_render_mode(RENDER_MANUAL);
    console_set_debug(false);

menu_start:
    console_clear();

    printf("\n");
    printf("LibDragon Short Accessory Test\n");
    printf("Press A to do a short accessory read.\n");
    printf("Press C-Down to do a short accessory read CRC fuzz test.\n");
    printf("\n");

    joypad_poll();

    JOYPAD_PORT_FOREACH (port)
    {
        btn = joypad_get_buttons_pressed(port);
        if (btn.raw)
        {
            if (joypad_get_accessory_type(port) == JOYPAD_ACCESSORY_TYPE_NONE)
            {
                printf("No accessory detected on Port %d!\n", port + 1);
                printf("\nPress any button to return...\n");
                console_render();

                wait_for_button_press(port);
                goto menu_start;
            }
            else if (btn.a)
            {
                accessory_short_read_test_mode(port, 0x0000, 0);
                goto menu_start;
            }
            else if (btn.c_down)
            {
                accessory_short_read_crc_fuzz_mode(port, 0);
                goto menu_start;
            }
        }
    }

    console_render();
    goto menu_start;
}
