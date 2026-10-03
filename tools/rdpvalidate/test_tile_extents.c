/* Regression tests for the host RDP validator's tile register model. */
#include "../../src/rdpq/rdpq_debug.c"

static unsigned checks, failures;

static void check(bool condition, const char *message)
{
    checks++;
    if (!condition) {
        failures++;
        fprintf(stderr, "FAIL: %s\n", message);
    }
}

static void validate(uint64_t *command)
{
    int errs, warns;
    rdpq_validate(command, RDPQ_VALIDATE_FLAG_NOECHO, &errs, &warns);
    check(errs == 0 && warns == 0, "setup command must validate");
}

static void reset(void)
{
    memset(&rdp, 0, sizeof(rdp));
    memset(&vctx, 0, sizeof(vctx));
    static uint64_t rgb_mode = 0xEF00080000000000ULL;
    validate(&rgb_mode);
}

int main(void)
{
    for (unsigned tidx = 0; tidx < 8; tidx++) {
        uint64_t initial = 0xF500000000000000ULL | (2ULL << 51)
            | (2ULL << 41) | ((uint64_t)tidx << 24);
        reset();
        validate(&initial);
        check(!rdp.tile[tidx].has_extents, "SET_TILE must not invent extents");
        float coordinates[] = {0, 0};
        validate_use_tile_internal(tidx, 1, 0, coordinates, 1);
        check(vctx.errs == 1, "missing extents must still be diagnosed");

        for (unsigned load = 0; load < 2; load++) {
            reset();
            validate(&initial);
            uint64_t image = 0xFD00000000000000ULL | (2ULL << 51)
                | (7ULL << 32) | 0x100000;
            if (load) validate(&image);
            uint64_t extents = ((load ? 0xF4ULL : 0xF2ULL) << 56)
                | (4ULL << 44) | (8ULL << 32) | ((uint64_t)tidx << 24)
                | (28ULL << 12) | 40;
            validate(&extents);
            uint64_t sync = 0xE800000000000000ULL;
            validate(&sync);
            uint64_t attributes = 0xF500000000000000ULL | (2ULL << 53)
                | (4ULL << 41) | (8ULL << 32) | ((uint64_t)tidx << 24)
                | (3ULL << 20) | (1ULL << 19) | (1ULL << 18)
                | (4ULL << 14) | (1ULL << 9) | (1ULL << 8) | (3ULL << 4);
            validate(&attributes);
            struct tile_s *tile = &rdp.tile[tidx];
            check(tile->has_extents, "SET_TILE must retain existing extents");
            check(tile->s0 == 1 && tile->t0 == 2 && tile->s1 == 7 && tile->t1 == 10,
                  "SET_TILE must retain all four bounds");
            check(tile->last_setsize == &extents && tile->last_setsize_data == extents,
                  "SET_TILE must retain extent diagnostic provenance");
            check(tile->last_settile == &attributes && tile->last_settile_data == attributes,
                  "SET_TILE must update attribute diagnostic provenance");
            check(tile->fmt == 2 && tile->size == 0 && tile->pal == 3,
                  "SET_TILE must update format, size and palette");
            check(tile->tmem_addr == 64 && tile->tmem_pitch == 32,
                  "SET_TILE must update TMEM addressing");
            check(tile->s.clamp && tile->t.clamp && tile->s.mirror && tile->t.mirror
                  && tile->s.mask == 3 && tile->t.mask == 4,
                  "SET_TILE must update clamp, mirror and mask");
        }
    }
    printf("tile extent model: checks=%u failures=%u\n", checks, failures);
    return failures != 0;
}
