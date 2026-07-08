/**
 * @file flashramtest.c
 * @brief FlashRAM Write-Read-Verify soak test
 *
 * Detects the FlashRAM chip, then once per second writes a rotating pattern to
 * a fixed region, reads it back, and verifies it byte-for-byte. Results are
 * shown on screen and mirrored to debugf (ISViewer / USB) so the test can be
 * watched on ares or on real hardware via SC64.
 *
 * Each iteration uses a different pattern (seeded by the iteration counter) so
 * a "read returns stale data" bug is caught, not just a bad first write.
 *
 * Build with N64_ROM_SAVETYPE = flashram (see the Makefile).
 */

#include <libdragon.h>
#include <string.h>

#define TEST_OFFSET 0      ///< Byte offset in FlashRAM to exercise.
#define TEST_LEN    1024   ///< Bytes written/verified each cycle (spans 8 pages).

// 16-byte aligned so they are valid PI-DMA buffers for flashram_read/write.
static uint8_t wbuf[TEST_LEN] __attribute__((aligned(16)));
static uint8_t rbuf[TEST_LEN] __attribute__((aligned(16)));

int main(void)
{
    debug_init_isviewer();
    debug_init_usblog();
    timer_init();
    console_init();
    console_set_render_mode(RENDER_AUTOMATIC);

    // Use a longer PI DOM2 latch than the standard default: some FlashRAM parts
    // are unreliable at the wiki's 0x05 latency on real hardware, so keep it in
    // the safe 0x40-0x50 range. The other timings stay at their defaults --
    // page size must remain 0x0F so the driver splits DMAs itself.
    pi_dom_timings_t timings = {
        .latency     = 0x40,
        .pulse_width = 0x0C,
        .page_size   = 0x0F,
        .release     = 0x02,
    };

    flashram_info_t info;
    bool present = flashram_init(&timings, &info);

    if (!present)
    {
        console_clear();
        printf("No FlashRAM detected!\n\n"
               "Build with N64_ROM_SAVETYPE = flashram\n"
               "and run on a flashram-configured target.\n");
        debugf("flashramtest: no FlashRAM detected\n");
        while (1) { /* halt */ }
    }

    debugf("flashramtest: %s  %u KiB  mfr=%04X dev=%04X  %s-indexed\n",
           info.name, (unsigned)(info.total_size / 1024),
           info.manufacturer_id, info.device_id,
           info.layout.unit_bits ? "word" : "byte");

    unsigned iter = 0, passes = 0, fails = 0;

    while (1)
    {
        // Fill the write buffer with an iteration-dependent pattern.
        uint8_t seed = (uint8_t) iter;
        for (int i = 0; i < TEST_LEN; i++)
            wbuf[i] = (uint8_t) (seed + i);

        // Write, read back, verify.
        memset(rbuf, 0, sizeof(rbuf));
        int written = flashram_write(wbuf, TEST_OFFSET, TEST_LEN);
        int read = flashram_read(rbuf, TEST_OFFSET, TEST_LEN);
        int mismatch = -1;
        for (int i = 0; i < TEST_LEN; i++)
        {
            if (wbuf[i] != rbuf[i]) { mismatch = i; break; }
        }
        bool ok = (written == TEST_LEN) && (read == TEST_LEN) && (mismatch < 0);

        iter++;
        if (ok) passes++; else fails++;

        console_clear();
        printf("FlashRAM Write-Read-Verify @ 1 Hz\n");
        printf("--------------------------------\n");
        printf("Chip : %s (%u KiB)\n", info.name, (unsigned)(info.total_size / 1024));
        printf("Range: 0x%X..0x%X\n\n", TEST_OFFSET, TEST_OFFSET + TEST_LEN);
        printf("Iteration: %u\n", iter);
        printf("Pass: %u    Fail: %u\n\n", passes, fails);
        printf("Last cycle: %s\n", ok ? "OK" : "MISMATCH");
        if (written != TEST_LEN)
            printf("  write returned %d\n", written);
        if (mismatch >= 0)
            printf("  byte %d: wrote 0x%02X, read 0x%02X\n",
                   mismatch, wbuf[mismatch], rbuf[mismatch]);

        debugf("flashramtest: iter=%u %s (pass=%u fail=%u)\n",
               iter, ok ? "OK" : "FAIL", passes, fails);
        if (!ok)
            debugf("  written=%d read=%d first-mismatch=%d\n", written, read, mismatch);

        wait_ms(1000);
    }
}
