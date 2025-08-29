/**
 * @file joypadtest.c
 * @author Christopher Bonhage (me@christopherbonhage.com)
 * @brief N64 test ROM for Joypad subsystem
 */

#include <string.h>
#include <libdragon.h>

const char *format_joypad_style(joypad_style_t style)
{
    switch (style)
    {
    case JOYPAD_STYLE_NONE:
        return "None   ";
    case JOYPAD_STYLE_N64:
        return "N64    ";
    case JOYPAD_STYLE_GCN:
        return "GCN    ";
    case JOYPAD_STYLE_MOUSE:
        return "Mouse  ";
    default:
        return "Unknown";
    }
}

const char *format_joypad_accessory_type(joypad_accessory_type_t accessory_type)
{
    switch (accessory_type)
    {
    case JOYPAD_ACCESSORY_TYPE_NONE:
        return "None            ";
    case JOYPAD_ACCESSORY_TYPE_CONTROLLER_PAK:
        return "Controller Pak  ";
    case JOYPAD_ACCESSORY_TYPE_RUMBLE_PAK:
        return "Rumble Pak      ";
    case JOYPAD_ACCESSORY_TYPE_TRANSFER_PAK:
        return "Transfer Pak    ";
    case JOYPAD_ACCESSORY_TYPE_BIO_SENSOR:
        return "Bio Sensor      ";
    case JOYPAD_ACCESSORY_TYPE_SNAP_STATION:
        return "Snap Station    ";
    default:
        return "Unknown         ";
    }
}

const char *format_joypad_rumble(bool supported, bool enabled)
{
    if (!supported) return "Unavailable";
    if (enabled) return "Active";
    return "Idle";
}

void print_joypad_inputs(joypad_inputs_t inputs)
{
    printf(
        "Stick: %+04d,%+04d C-Stick: %+04d,%+04d L-Trig:%03d R-Trig:%03d\n",
        inputs.stick_x, inputs.stick_y,
        inputs.cstick_x, inputs.cstick_y,
        inputs.analog_l, inputs.analog_r
    );
    printf(
        "D-U:%d D-D:%d D-L:%d D-R:%d C-U:%d C-D:%d C-L:%d C-R:%d\n",
        inputs.btn.d_up, inputs.btn.d_down,
        inputs.btn.d_left, inputs.btn.d_right,
        inputs.btn.c_up, inputs.btn.c_down,
        inputs.btn.c_left, inputs.btn.c_right
    );
    printf(
        "A:%d B:%d X:%d Y:%d L:%d R:%d Z:%d Start:%d\n",
        inputs.btn.a, inputs.btn.b,
        inputs.btn.x, inputs.btn.y,
        inputs.btn.l, inputs.btn.r,
        inputs.btn.z, inputs.btn.start
    );
}

void cpak_read_test_mode(joypad_port_t port)
{
    joypad_buttons_t btn;
    uint8_t cpak_buffer[256];
    uint32_t start_ticks, end_ticks;
    int result;

    console_clear();
    printf("\n");
    printf("CPak Read Test Mode\n");
    printf("===================\n\n");

    // Check if P1 has a Controller Pak
    if (joypad_get_accessory_type(port) != JOYPAD_ACCESSORY_TYPE_CONTROLLER_PAK)
    {
        printf("No Controller Pak detected on Port 1!\n");
        printf("\nPress any button to return...\n");
        console_render();

        // Wait for button press
        while (1)
        {
            joypad_poll();
            btn = joypad_get_buttons_pressed(port);
            if (btn.raw) break;
        }
        return;
    }

    printf("Reading first 256 bytes of bank 0...\n\n");
    console_render();

    // Measure the read time
    start_ticks = get_ticks();
    result = cpak_read(port, 0, 0, cpak_buffer, sizeof(cpak_buffer));
    end_ticks = get_ticks();

    if (result >= 0)
    {
        uint32_t elapsed_us = TIMER_MICROS(end_ticks - start_ticks);
        printf("Read completed successfully!\n\n");
        printf("Timing Results:\n");
        printf("  Ticks: %lu\n", end_ticks - start_ticks);
        printf("  Micros:  %lu us\n", elapsed_us);
        printf("  Millis:  %lu ms\n", elapsed_us / 1000);
        printf("  Seconds: %.2f s\n", elapsed_us / 1000000.0);
        printf("  Speed: %.2f KB/s\n", ((float)sizeof(cpak_buffer) / elapsed_us) * 1000000.0 / 1024.0);

        // Show first few bytes of data
        printf("\nFirst 256 bytes of bank 0:\n");
        for (int i = 0; i < 256; i++)
        {
            printf("%02X ", cpak_buffer[i]);
            if ((i + 1) % 16 == 0) printf("\n");
        }
    }
    else
    {
        printf("Read failed with error: %d\n", result);
    }

    printf("\n\nPress any button to return...\n");
    console_render();

    // Wait for button release then press
    while (1)
    {
        joypad_poll();
        btn = joypad_get_buttons_pressed(port);
        if (btn.raw) break;
    }
}

int main(void)
{
    joypad_style_t style;
    joypad_accessory_type_t accessory_type;
    joypad_inputs_t inputs;
    joypad_buttons_t btn;
    bool rumble_supported, rumble_active;
    int cpak_num_banks[4] = {0, 0, 0, 0};
    int b_hold_time[4] = {0, 0, 0, 0};

    timer_init();
    joypad_init();
    debug_init_isviewer();
    debug_init_usblog();
    console_init();
    console_set_render_mode(RENDER_MANUAL);
    console_set_debug(false);

    while (1)
    {
        console_clear();

        printf("\n");
        printf("LibDragon Joypad Subsystem Test\n");
        printf("Hold A to test rumble motors.\n");
        printf("Hold B to trigger the exception screen.\n");
        printf("Press C-Down on P1 to test CPak read timing.\n");
        printf("\n");

        joypad_poll();

        JOYPAD_PORT_FOREACH (port)
        {
            style = joypad_get_style(port);
            accessory_type = joypad_get_accessory_type(port);
            inputs = joypad_get_inputs(port);
            btn = joypad_get_buttons_pressed(port);

            if (btn.c_down)
            {
                cpak_read_test_mode(port);
                break;
            }

            if (accessory_type == JOYPAD_ACCESSORY_TYPE_RUMBLE_PAK)
            {
                rumble_supported = joypad_get_rumble_supported(port);
                rumble_active = joypad_get_rumble_active(port);
                if (inputs.btn.a && !rumble_active)
                {
                    joypad_set_rumble_active(port, true);
                }
                else if (!inputs.btn.a && rumble_active)
                {
                    joypad_set_rumble_active(port, false);
                }
            }

            if (accessory_type == JOYPAD_ACCESSORY_TYPE_CONTROLLER_PAK)
            {
                if (cpak_num_banks[port] == 0)
                {
                    cpak_num_banks[port] = cpak_probe_banks(port);
                }
            }
            else
            {
                cpak_num_banks[port] = 0; // Reset bank count for non-Controller Paks
            }

            if (inputs.btn.b)
            {
                if (!b_hold_time[port])
                    b_hold_time[port] = get_ticks_ms();
                if (get_ticks_ms() - b_hold_time[port] > 1000)
                {
                    assertf(0, "B button held for one second, exception screen triggered for debugging purposes");
                }
            }
            else
            {
                b_hold_time[port] = 0;
            }

            printf("Port %d ", port + 1);
            printf("Style: %s ", format_joypad_style(style));
            printf("Pak: %s ", format_joypad_accessory_type(accessory_type));
            if (accessory_type == JOYPAD_ACCESSORY_TYPE_RUMBLE_PAK)
            {
                printf("Rumble: %s", format_joypad_rumble(rumble_supported, rumble_active));
            }
            else if (accessory_type == JOYPAD_ACCESSORY_TYPE_CONTROLLER_PAK)
            {
                printf("Banks: %d", cpak_num_banks[port]);
                if (!cpak_supports_bankswitching(port))
                    printf(" (no b/s)");
            }
            printf("\n");
            print_joypad_inputs(inputs);
            printf("\n");
        }

        console_render();
    }
}
