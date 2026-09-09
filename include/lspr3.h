/**
 * @file lspr3.h
 * @author Giovanni Bajo <giovannibajo@gmail.com>
 * @brief Lossy-sprite Level 3: H264I decoder
 *
 * H264I ("H.264 Image") is libdragon's "Lossy-sprite Level 3" container format:
 * lossy compression is useful for large images that need higher storage
 * compression at the cost of some quality loss, such as large backgrounds. The
 * quality factor can be tuned at build-time to achieve the desired size/quality
 * tradeoff. H264I files are produced by `mksprite --lossy=<quality> --compress 3`
 * and always decode to a #FMT_RGBA16 sprite.
 *
 * You must opt-in to support for Level 3 lossy sprites by calling #lspr3_init
 * to register the H264I decoder with #sprite_load before loading H264I files.
 * Once initialized, #sprite_load will recognize H264I-compressed sprites and
 * decode them transparently.
 *
 * H264I is implemented as an H.264 intra-only bitstream (4:2:0 YUV, BT.709
 * full range) plus a small header. At load-time the H.264 slice is decoded
 * and the YUV reconstruction is converted into a #FMT_RGBA16 sprite, so the
 * returned #sprite_t works like a normal sprite.
 *
 * H264I can encode either images with no alpha channel, or images with a
 * 1-bit alpha channel. The alpha channel is stored lossless alongside the
 * H.264 bitstream, and reconstructed at load-time.
 */
#ifndef __LIBDRAGON_LSPR3_H
#define __LIBDRAGON_LSPR3_H

#include <stddef.h>
#include "preview.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct sprite_s sprite_t;

/**
 * @brief Optional parameters for advanced lspr3 decoding.
 *
 * Pass a pointer to one of these into #lspr3_load_buf_ex to override the
 * default decode behaviour used by #sprite_load. Each field defaults to
 * "use the standard behaviour" when zero, so a zero-initialised
 * `(lspr3_load_parms_t){}` is equivalent to passing NULL.
 * @preview
 */
typedef struct lspr3_load_parms_s {
    /**
     * @brief Output horizontal divisor.
     *
     * 0 or 1 = native source width (default).
     * 2 = decode to a half-width sprite. The RDP YUV combiner performs
     * the horizontal downsample during the YUV→RGB conversion using its
     * bilinear filter, so no separate downsample pass runs. Useful when
     * the destination framebuffer is narrower than the encoded source
     * (or memory pressure rules out a full-width copy).
     */
    int output_x_divisor;

    /**
     * @brief Output vertical divisor.
     *
     * 0 or 1 = native source height (default).
     * 2 = decode to a half-height sprite. As with @ref output_x_divisor
     * the RDP bilinear filter handles the downsample during the YUV→RGB
     * blit. The two divisors are independent — set both to 2 for a
     * quarter-area sprite, or only one for an anisotropic downscale.
     */
    int output_y_divisor;

    /**
     * @brief Decode in horizontal bands to cap the transient YUV footprint.
     *
     * When > 0, the frame is reconstructed in bands of this many MB-rows into a
     * small windowed scratch buffer (`band_rows`+2 MB-rows) and each band is
     * converted into the output sprite as it completes, so the peak transient
     * YUV is ~`(band_rows+2)/mb_height` of the full-frame YUV instead of the
     * whole frame. Output is bit-identical to the full-frame path: intra
     * prediction's cross-band top-neighbours are carried, and each band decodes
     * one row ahead so its bottom-row chroma upsample has its real neighbour (no
     * seam at band boundaries). Costs a few % more decode time (extra per-band
     * RSP/RDP syncs), so it is intended for callers trading a little speed for a
     * smaller transient working set.
     *
     * 0 (default) = decode the whole frame at once (fastest; needs the full
     * frame's worth of transient YUV).
     */
    int band_rows;
} lspr3_load_parms_t;

/**
 * @brief Decode an H264I-encoded sprite from memory, with options.
 *
 * Lower-level decode entry point exposed for advanced callers. The
 * #sprite_load path uses #lspr3_load_buf_ex with NULL @p parms, matching
 * the default behaviour (native source dimensions, full-frame decode).
 *
 * The output sprite is allocated with #memalign and owns its pixel buffer, so
 * it must be released with #sprite_free.
 * @preview
 *
 * @param encoded_buf  Pointer to the H264I-encoded sprite payload.
 * @param encoded_sz   Size of @p encoded_buf in bytes.
 * @param parms        Optional decode parameters; pass NULL for defaults.
 * @return The decoded sprite, or aborts via #assertf on failure.
 */
LIBDRAGON_PREVIEW_API
sprite_t *lspr3_load_buf_ex(const void *encoded_buf, int encoded_sz,
                            const lspr3_load_parms_t *parms);

/**
 * @brief Register the H264I (Lossy-sprite Level 3) decoder with the sprite loader.
 * @preview
 *
 * After registering, #sprite_load recognizes the `H264` magic and decodes
 * matching files transparently. Until then, loading an H264I file via
 * #sprite_load fails with an assertion.
 * 
 * Refcounted; calling this increments the refcount and will require the
 * same number of #lspr3_close calls to fully unregister.
 */
LIBDRAGON_PREVIEW_API
void lspr3_init(void);

/**
 * @brief Unregister the H264I (Lossy-sprite Level 3) decoder.
 * @preview
 *
 * Refcounted; calling this decrements the refcount and once it reaches zero,
 * #sprite_load no longer recognizes H264I files and attempts to load them will
 * fail with an assertion.
 */
LIBDRAGON_PREVIEW_API
void lspr3_close(void);

#ifdef __cplusplus
}
#endif

#endif
