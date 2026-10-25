/**
 * @file samplebuffer_internal.h
 * @brief Sample buffer functions that only the mixer uses
 */
#ifndef __LIBDRAGON_SAMPLEBUFFER_INTERNAL_H
#define __LIBDRAGON_SAMPLEBUFFER_INTERNAL_H

#include "samplebuffer.h"

/**
 * @brief Configure the unit size and append granularity together.
 *
 * The buffer must be empty. When reusing it for another codec, install both
 * values before resizing the ring: the old codec's append granularity need
 * not fit when expressed in the new codec's units. Then connect the new
 * reader with #samplebuffer_set_waveform.
 */
void samplebuffer_set_format(samplebuffer_t *buf, int unit_bytes, int append_units);

#endif
