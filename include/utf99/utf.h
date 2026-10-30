#ifndef UTF_H
#define UTF_H

#include <stdint.h>
#include <sio99/sio.h>

/** Results returned by decoder operations. */
enum {

  /** The unit was accepted or the input is complete. */
  UTF_OK,

  /** Invalid encoding, interrupted character, or incomplete final input. */
  UTF_ERROR_INVALID,

  /** The destination returned a result other than `SIO_OK`. */
  UTF_ERROR_WRITE,
};

/** Allocation-free incremental UTF decoder with optional UTF-8 output. */
typedef struct UtfDecoder UtfDecoder;

/**
 * Initializes or resets the decoder. `out_decoder` must be non-`NULL`.
 * Borrows `writer` until reinitialization. No allocation or deinitialization
 * is required.
 * A `NULL` writer validates input without producing output. A non-`NULL` writer
 * must have a write callback. The destination receives UTF-8 bytes.
 *
 * Each write accepts one code unit. Input encodings may change only between
 * complete characters. UTF-16 and UTF-32 units are numeric values, independent
 * of byte order. Invalid input returns `UTF_ERROR_INVALID`. Every nonzero
 * destination result becomes `UTF_ERROR_WRITE`. The destination must retain
 * its own detailed error if the caller needs it. Output has no added
 * terminator. NUL, byte order marks, and Unicode noncharacters are preserved.
 *
 * Any error leaves the decoder failed. Reinitialize before writing or finishing
 * again. Using a failed decoder is programmer error. Output already written is
 * not rolled back, including a partial character on destination failure.
 * The writer must not reenter or modify the decoder during a callback.
 */
void utf_decoder_init(
  UtfDecoder* out_decoder,
  SioWriter* writer
);

/**
 * Accepts one UTF-8 byte. Incomplete characters remain pending without output.
 * Rejects invalid prefixes, overlong sequences, and non-scalar values.
 * `decoder` must be non-`NULL`, initialized, and not failed. Returns `UTF_OK`,
 * `UTF_ERROR_INVALID`, or `UTF_ERROR_WRITE`.
 */
int utf_decoder_write_utf8(
  UtfDecoder* decoder,
  uint8_t unit
);

/**
 * Accepts one UTF-16 code unit. A high surrogate remains pending until a low
 * surrogate completes it. An unpaired low surrogate is invalid.
 * `decoder` must be non-`NULL`, initialized, and not failed. Returns `UTF_OK`,
 * `UTF_ERROR_INVALID`, or `UTF_ERROR_WRITE`.
 */
int utf_decoder_write_utf16(
  UtfDecoder* decoder,
  uint16_t unit
);

/**
 * Accepts one UTF-32 code unit. Rejects surrogates and values above U+10FFFF.
 * `decoder` must be non-`NULL`, initialized, and not failed. Returns `UTF_OK`,
 * `UTF_ERROR_INVALID`, or `UTF_ERROR_WRITE`.
 */
int utf_decoder_write_utf32(
  UtfDecoder* decoder,
  uint32_t unit
);

/**
 * Checks that no incomplete character remains. Does not flush or close the
 * destination. `decoder` must be non-`NULL`, initialized, and not failed.
 * Success leaves the decoder ready for more input. Repeated calls are allowed.
 * Incomplete input returns `UTF_ERROR_INVALID` and leaves the decoder failed.
 */
int utf_decoder_finish(
  UtfDecoder* decoder
);

/** Internal decoder states. Callers must not modify decoder state. */
typedef enum UtfDecoderState {
  UTF_DECODER_STATE_READY,
  UTF_DECODER_STATE_UTF8,
  UTF_DECODER_STATE_UTF16,
  UTF_DECODER_STATE_FAILED,
} UtfDecoderState;

/**
 * Internal state exposed for caller allocation.
 * Initialize with `utf_decoder_init`.
 */
struct UtfDecoder {
  SioWriter* writer;
  UtfDecoderState state;
  uint32_t code_point;
  uint32_t minimum_code_point;
  uint8_t remaining;
};

#endif /* UTF_H */
