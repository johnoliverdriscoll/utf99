#include <assert.h>
#include <stddef.h>
#include <utf99/utf.h>

static int
utf_decoder_fail(
  UtfDecoder* decoder,
  int error
) {
  decoder->state = UTF_DECODER_STATE_FAILED;
  return error;
}

static int
utf_decoder_emit(
  UtfDecoder* decoder,
  uint32_t code_point
) {
  if (
    code_point > 0x10ffff
    || (code_point >= 0xd800 && code_point <= 0xdfff)
  ) {
    return utf_decoder_fail(decoder, UTF_ERROR_INVALID);
  }

  uint8_t bytes[4];
  unsigned count;
  if (code_point <= 0x7f) {
    bytes[0] = (uint8_t) code_point;
    count = 1;
  } else if (code_point <= 0x7ff) {
    bytes[0] = (uint8_t) (0xc0 | (code_point >> 6));
    bytes[1] = (uint8_t) (0x80 | (code_point & 0x3f));
    count = 2;
  } else if (code_point <= 0xffff) {
    bytes[0] = (uint8_t) (0xe0 | (code_point >> 12));
    bytes[1] = (uint8_t) (0x80 | ((code_point >> 6) & 0x3f));
    bytes[2] = (uint8_t) (0x80 | (code_point & 0x3f));
    count = 3;
  } else {
    bytes[0] = (uint8_t) (0xf0 | (code_point >> 18));
    bytes[1] = (uint8_t) (0x80 | ((code_point >> 12) & 0x3f));
    bytes[2] = (uint8_t) (0x80 | ((code_point >> 6) & 0x3f));
    bytes[3] = (uint8_t) (0x80 | (code_point & 0x3f));
    count = 4;
  }

  if (decoder->writer != NULL) {
    for (unsigned i = 0; i < count; i++) {
      if (sio_writer_write(decoder->writer, bytes[i]) != SIO_OK) {
        return utf_decoder_fail(decoder, UTF_ERROR_WRITE);
      }
    }
  }
  decoder->state = UTF_DECODER_STATE_READY;
  return UTF_OK;
}

void
utf_decoder_init(
  UtfDecoder* out_decoder,
  SioWriter* writer
) {
  assert(out_decoder != NULL);
  assert(writer == NULL || writer->write != NULL);

  *out_decoder = (UtfDecoder) {
    .writer = writer,
    .state = UTF_DECODER_STATE_READY,
  };
}

int
utf_decoder_write_utf8(
  UtfDecoder* decoder,
  uint8_t unit
) {
  assert(decoder != NULL);
  assert(decoder->state != UTF_DECODER_STATE_FAILED);

  if (decoder->state == UTF_DECODER_STATE_UTF16) {
    return utf_decoder_fail(decoder, UTF_ERROR_INVALID);
  }
  if (decoder->state == UTF_DECODER_STATE_READY) {
    if (unit <= 0x7f) {
      return utf_decoder_emit(decoder, unit);
    }
    if (unit >= 0xc2 && unit <= 0xdf) {
      decoder->code_point = unit & 0x1f;
      decoder->minimum_code_point = 0x80;
      decoder->remaining = 1;
    } else if (unit >= 0xe0 && unit <= 0xef) {
      decoder->code_point = unit & 0x0f;
      decoder->minimum_code_point = 0x800;
      decoder->remaining = 2;
    } else if (unit >= 0xf0 && unit <= 0xf4) {
      decoder->code_point = unit & 0x07;
      decoder->minimum_code_point = 0x10000;
      decoder->remaining = 3;
    } else {
      return utf_decoder_fail(decoder, UTF_ERROR_INVALID);
    }
    decoder->state = UTF_DECODER_STATE_UTF8;
    return UTF_OK;
  }

  if (unit < 0x80 || unit > 0xbf) {
    return utf_decoder_fail(decoder, UTF_ERROR_INVALID);
  }
  // Reject impossible prefixes before accepting more continuation bytes.
  if (decoder->minimum_code_point == 0x800 && decoder->remaining == 2) {
    if (
      (decoder->code_point == 0 && unit < 0xa0)
      || (decoder->code_point == 0x0d && unit > 0x9f)
    ) {
      return utf_decoder_fail(decoder, UTF_ERROR_INVALID);
    }
  }
  if (decoder->minimum_code_point == 0x10000 && decoder->remaining == 3) {
    if (
      (decoder->code_point == 0 && unit < 0x90)
      || (decoder->code_point == 4 && unit > 0x8f)
    ) {
      return utf_decoder_fail(decoder, UTF_ERROR_INVALID);
    }
  }
  decoder->code_point = (decoder->code_point << 6) | (unit & 0x3f);
  decoder->remaining--;
  if (decoder->remaining != 0) {
    return UTF_OK;
  }
  if (decoder->code_point < decoder->minimum_code_point) {
    return utf_decoder_fail(decoder, UTF_ERROR_INVALID);
  }
  return utf_decoder_emit(decoder, decoder->code_point);
}

int
utf_decoder_write_utf16(
  UtfDecoder* decoder,
  uint16_t unit
) {
  assert(decoder != NULL);
  assert(decoder->state != UTF_DECODER_STATE_FAILED);

  if (decoder->state == UTF_DECODER_STATE_UTF8) {
    return utf_decoder_fail(decoder, UTF_ERROR_INVALID);
  }
  if (decoder->state == UTF_DECODER_STATE_UTF16) {
    if (unit < 0xdc00 || unit > 0xdfff) {
      return utf_decoder_fail(decoder, UTF_ERROR_INVALID);
    }
    return utf_decoder_emit(
      decoder,
      decoder->code_point + (unit - 0xdc00) + 0x10000
    );
  }
  if (unit >= 0xd800 && unit <= 0xdbff) {
    decoder->code_point = (uint32_t) (unit - 0xd800) << 10;
    decoder->state = UTF_DECODER_STATE_UTF16;
    return UTF_OK;
  }
  return utf_decoder_emit(decoder, unit);
}

int
utf_decoder_write_utf32(
  UtfDecoder* decoder,
  uint32_t unit
) {
  assert(decoder != NULL);
  assert(decoder->state != UTF_DECODER_STATE_FAILED);

  if (decoder->state != UTF_DECODER_STATE_READY) {
    return utf_decoder_fail(decoder, UTF_ERROR_INVALID);
  }
  return utf_decoder_emit(decoder, unit);
}

int
utf_decoder_finish(
  UtfDecoder* decoder
) {
  assert(decoder != NULL);
  assert(decoder->state != UTF_DECODER_STATE_FAILED);

  if (decoder->state != UTF_DECODER_STATE_READY) {
    return utf_decoder_fail(decoder, UTF_ERROR_INVALID);
  }
  return UTF_OK;
}
