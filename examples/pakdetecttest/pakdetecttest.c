/**
 * @file pakdetecttest.c
 * @author Christopher Bonhage (me@christopherbonhage.com)
 * @brief N64 test ROM for Accessory DETECT
 */

#include <string.h>
#include <stdlib.h>
#include <libdragon.h>

#include "../../src/joybus/joybus_accessory_internal.h"
#include "../../src/joybus/joybus_commands.h"

// From joypad_internal.h
joypad_inputs_t joypad_read_n64_inputs(joypad_port_t port);

void wait_for_b_button(void)
{
    joypad_inputs_t inputs = joypad_read_n64_inputs(JOYPAD_PORT_1);
    while (!inputs.btn.b)
    {
        wait_ms(1);
        inputs = joypad_read_n64_inputs(JOYPAD_PORT_1);
    }
}

const char * format_joybus_accessory_io_status(joybus_accessory_io_status_t status)
{
    switch (status)
    {
        case JOYBUS_ACCESSORY_IO_STATUS_OK:
            return "OK";
        case JOYBUS_ACCESSORY_IO_STATUS_NO_DEVICE:
            return "NO DEVICE";
        case JOYBUS_ACCESSORY_IO_STATUS_NO_PAK:
            return "BAD PAK";
        case JOYBUS_ACCESSORY_IO_STATUS_BAD_CRC:
            return "BAD CRC";
        default:
            return "UNKNOWN";
    }
}

void joybus_build_cmd(
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

void draw_joybus_buffers(
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

void test_accessory_read(joypad_port_t port, bool valid_checksum)
{
    console_clear();
    wait_ms(100);
    printf("Performing Accessory Read on Port %d (%s Address Checksum)\n\n",
           port + 1,
           valid_checksum ? "Valid" : "Invalid");

    uint8_t input[JOYBUS_BLOCK_SIZE] = {0};
    uint8_t output[JOYBUS_BLOCK_SIZE] = {0};
    uint16_t addr = 0x0001;
    joybus_cmd_n64_accessory_read_port_t cmd = { .send = {
        .command = JOYBUS_COMMAND_ID_N64_ACCESSORY_READ,
        .addr_checksum = valid_checksum ? joybus_accessory_calculate_addr_checksum(addr) : addr,
    } };

    joybus_build_cmd(port, sizeof(cmd.send), sizeof(cmd.recv), &cmd.send, input);

    // Populate random data in the input recv buffer to see if it's overwritten
    int recv_start = port + JOYBUS_COMMAND_METADATA_SIZE + sizeof(cmd.send);
    for (int i = 0; i < JOYBUS_ACCESSORY_DATA_SIZE; i++) cmd.recv.data[i] = rand() & 0xFF;
    memcpy(&input[recv_start], &cmd.recv, sizeof(cmd.recv));

    // Run the Joybus command
    uint32_t start = get_ticks();
    joybus_exec(input, output);
    uint32_t end = get_ticks();

    // Copy recv_data from the output buffer
    memcpy(&cmd.recv, &output[recv_start], sizeof(cmd.recv));

    // Validate the data CRC
    joybus_accessory_io_status_t status = joybus_accessory_compare_data_crc(cmd.recv.data, cmd.recv.data_crc);

    printf("Execution time: %u microseconds\n\n", TIMER_MICROS(end - start));

    draw_joybus_buffers(input, output);

    printf("\n\nAccessory Read Data (32 bytes):\n");
    printf("00 01 02 03 04 05 06 07 08 09 0A 0B 0C 0D 0E 0F\n");
    printf("------------------------------------------------\n");

    // First row (bytes 0-15)
    for (int i = 0; i < 16; i++)
    {
        printf("%02X ", cmd.recv.data[i]);
    }
    printf("  Data CRC: %02X\n", cmd.recv.data_crc);

    // Second row (bytes 16-31)
    for (int i = 16; i < 32; i++)
    {
        printf("%02X ", cmd.recv.data[i]);
    }

    printf("  Status: %s\n", format_joybus_accessory_io_status(status));

    printf("\n\n");
    printf("Press B to continue...");
    console_render();
    wait_for_b_button();
}

void test_accessory_write(joypad_port_t port, bool valid_checksum)
{
    console_clear();
    wait_ms(100);
    printf("Performing Accessory Write on Port %d (%s Address Checksum)\n\n",
           port + 1,
           valid_checksum ? "Valid" : "Invalid");

    uint8_t input[JOYBUS_BLOCK_SIZE] = {0};
    uint8_t output[JOYBUS_BLOCK_SIZE] = {0};
    uint16_t addr = 0x0001;
    joybus_cmd_n64_accessory_write_port_t cmd = { .send = {
        .command = JOYBUS_COMMAND_ID_N64_ACCESSORY_WRITE,
        .addr_checksum = valid_checksum ? joybus_accessory_calculate_addr_checksum(addr) : addr,
    } };
    // Generate random data to write
    for (int i = 0; i < JOYBUS_ACCESSORY_DATA_SIZE; i++) cmd.send.data[i] = rand() & 0xFF;

    joybus_build_cmd(port, sizeof(cmd.send), sizeof(cmd.recv), &cmd.send, input);
    uint32_t start = get_ticks();
    joybus_exec(input, output);
    uint32_t end = get_ticks();

    // Copy recv_data from the output buffer
    int recv_start = port + JOYBUS_COMMAND_METADATA_SIZE + sizeof(cmd.send);
    memcpy(&cmd.recv, &output[recv_start], sizeof(cmd.recv));

    // Calculate expected CRC for the data we sent
    uint8_t expected_crc = joybus_accessory_calculate_data_crc(cmd.send.data);

    // Validate the data CRC
    joybus_accessory_io_status_t status = joybus_accessory_compare_data_crc(cmd.send.data, cmd.recv.data_crc);

    printf("Execution time: %u microseconds\n\n", TIMER_MICROS(end - start));

    draw_joybus_buffers(input, output);

    printf("\n\nAccessory Write Data (32 bytes):\n");
    printf("00 01 02 03 04 05 06 07 08 09 0A 0B 0C 0D 0E 0F\n");
    printf("------------------------------------------------");
    printf("  Calc CRC: %02X\n", expected_crc);

    // First row (bytes 0-15)
    for (int i = 0; i < 16; i++)
    {
        printf("%02X ", cmd.send.data[i]);
    }
    printf("  Recv CRC: %02X\n", cmd.recv.data_crc);

    // Second row (bytes 16-31)
    for (int i = 16; i < 32; i++)
    {
        printf("%02X ", cmd.send.data[i]);
    }
    printf("  Status: %s\n", format_joybus_accessory_io_status(status));

    printf("\n\n");
    printf("Press B to continue...");
    console_render();
    wait_for_b_button();
}

void test_identify(joypad_port_t port)
{
    console_clear();
    wait_ms(100);
    printf("Performing Identify on Port %d\n\n", port + 1);

    uint8_t input[JOYBUS_BLOCK_SIZE] = {0};
    uint8_t output[JOYBUS_BLOCK_SIZE] = {0};
    joybus_cmd_identify_port_t cmd = { .send = {
        .command = JOYBUS_COMMAND_ID_IDENTIFY,
    } };

    joybus_build_cmd(port, sizeof(cmd.send), sizeof(cmd.recv), &cmd.send, input);
    uint32_t start = get_ticks();
    joybus_exec(input, output);
    uint32_t end = get_ticks();

    // Copy recv_data from the output buffer
    int recv_start = port + JOYBUS_COMMAND_METADATA_SIZE + sizeof(cmd.send);
    memcpy(&cmd.recv, &output[recv_start], sizeof(cmd.recv));

    printf("Execution time: %u microseconds\n\n", TIMER_MICROS(end - start));

    draw_joybus_buffers(input, output);

    printf("\n\nIdentify Response:\n");
    printf("--------------------------------\n");
    printf("Identifier: 0x%04X  ", cmd.recv.identifier);
    printf("Status: 0x%02X\n", cmd.recv.status);

    // Decode accessory status (bits 0-1)
    uint8_t accessory_status = cmd.recv.status & JOYBUS_IDENTIFY_STATUS_ACCESSORY_MASK;
    printf("\nAccessory Status (bits 0-1): ");
    switch (accessory_status)
    {
        case JOYBUS_IDENTIFY_STATUS_ACCESSORY_UNSUPPORTED:
            printf("0x00 - Unsupported/Absent\n");
            break;
        case JOYBUS_IDENTIFY_STATUS_ACCESSORY_PRESENT:
            printf("0x01 - Present\n");
            break;
        case JOYBUS_IDENTIFY_STATUS_ACCESSORY_ABSENT:
            printf("0x02 - Absent\n");
            break;
        case JOYBUS_IDENTIFY_STATUS_ACCESSORY_CHANGED:
            printf("0x03 - Changed\n");
            break;
    }

    // Decode other status bits
    if (cmd.recv.status & JOYBUS_IDENTIFY_STATUS_COMMAND_CHECKSUM_ERROR)
    {
        printf("Bit 2: 1 - Command Checksum Error\n");
    }

    printf("\n\n");
    printf("Press B to continue...");
    console_render();
    wait_for_b_button();
}

int main(void)
{
    debug_init_isviewer();
    debug_init_usblog();
    console_init();
    console_set_render_mode(RENDER_MANUAL);
    console_set_debug(false);

    while (1)
    {
        console_clear();

        printf("\n");
        printf("LibDragon Pak DETECT Test\n\n");
        printf("Button  | Joybus Command\n");
        printf("------- | --------------\n");
        printf("A       | Identify\n");
        printf("C-Down  | Accessory Read (Valid Address Checksum)\n");
        printf("C-Left  | Accessory Read (Invalid Address Checksum)\n");
        printf("C-Right | Accessory Write (Valid Address Checksum)\n");
        printf("C-Up    | Accessory Write (Invalid Address Checksum)\n");
        printf("\n");

        joypad_inputs_t inputs = joypad_read_n64_inputs(JOYPAD_PORT_1);

        if (inputs.btn.a)
        {
            test_identify(JOYPAD_PORT_1);
        }
        else if (inputs.btn.c_down)
        {
            test_accessory_read(JOYPAD_PORT_1, true);
        }
        else if (inputs.btn.c_left)
        {
            test_accessory_read(JOYPAD_PORT_1, false);
        }
        else if (inputs.btn.c_right)
        {
            test_accessory_write(JOYPAD_PORT_1, true);
        }
        else if (inputs.btn.c_up)
        {
            test_accessory_write(JOYPAD_PORT_1, false);
        }

        console_render();
    }
}
