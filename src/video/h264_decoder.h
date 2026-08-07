/**
 * @file h264_decoder.h
 * @author Giovanni Bajo <giovannibajo@gmail.com>
 */
#ifndef LIBDRAGON_VIDEO_H264_DECODER_H
#define LIBDRAGON_VIDEO_H264_DECODER_H

// Activate N64 specific codepath
#define H264BSD_N64         1
#define H264BSD_N64_INTRA   1     // Intraprediction on RSP
#define H264BSD_N64_CAVLC   0     // CAVLC on RSP

// Disable all code related to concealment (recovering of corrupted data).
// This must be defined before including the h264bsd headers, as it changes
// the layout of mbStorage_t (drops the `decoded` field); every translation
// unit that pulls in the h264 decoder must agree on it, so it lives here.
#define OPTIMIZE_NO_DECODED_FLAG

// Maximum number of macroblocks that the RSP will be able to lag behind the
// CPU, and process in background. This is the depth of the mbLayers ring the
// CPU decodes coefficients into while the RSP consumes earlier slots.
//
// There is now an EXPLICIT per-slot rspq syncpoint (mbLayerSync, see
// h264bsd_slice_data.c) that stalls the CPU before it would reuse a slot the
// RSP hasn't consumed. That makes this a pure performance/memory knob rather
// than a correctness one: it no longer has to over-provision against "the RSP
// is too slow" (which previously required 128 to avoid coefficient-buffer
// corruption, and broke outright once RSPQ_DRAM_LOWPRI_BUFFER_SIZE grew and let
// the CPU lap even a 128-deep ring). A small depth now suffices — it only sets
// how far the CPU may run ahead before it must wait on the syncpoint.
// One macroblock is ~1.5KB, so 8 is ~12KB (down from 128 == ~192KB).
#define NUM_PARALLEL_MACROBLOCKS 8

#include "h264_decoder/h264bsd_decoder.h"
#include "h264_decoder/h264bsd_storage.h"

typedef enum {
	PS_H264,
	PS_H264_NAL,
	PS_H264_MACROB,
	PS_H264_LAYER,
	PS_H264_LAYER_CLEAR,
	PS_H264_LAYER_PRED,
	PS_H264_LAYER_RES,
	PS_H264_LAYER_RES_ENC,
	PS_H264_RESIDUAL_LUMA,
	PS_H264_RESIDUAL_CHROMA,
	PS_H264_INTRAPRED_4X4,
	PS_H264_INTRAPRED_16X16,
	PS_H264_INTERPRED,
	PS_H264_INTERPRED_LUMA,
	PS_H264_INTERPRED_CHROMA,
} H264ProfileSlot;

#endif
