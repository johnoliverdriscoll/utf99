#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <utf99/utf.h>
#include <sio99/sio_buffer_writer.h>

#define CHECK(condition) do {                                               \
  if (!(condition)) {                                                       \
    fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #condition);            \
    exit(EXIT_FAILURE);                                                     \
  }                                                                         \
} while (0)

#define COUNT(array) (sizeof(array) / sizeof((array)[0]))

typedef struct Output {
  uint8_t bytes[64];
  size_t count;
  size_t calls;
  size_t fail_at;
  int error;
} Output;

static int
write_byte(
  void* user,
  uint8_t byte
) {
  Output* output = user;
  output->calls++;
  if (output->calls == output->fail_at) {
    return output->error;
  }
  CHECK(output->count < sizeof(output->bytes));
  output->bytes[output->count++] = byte;
  return 0;
}

typedef struct Character {
  uint32_t point;
  uint8_t bytes[4];
  size_t count;
} Character;

static const Character characters[] = {
  {0, {0}, 1},
  {0x7f, {0x7f}, 1},
  {0x80, {0xc2, 0x80}, 2},
  {0xe9, {0xc3, 0xa9}, 2},
  {0x7ff, {0xdf, 0xbf}, 2},
  {0x800, {0xe0, 0xa0, 0x80}, 3},
  {0xd7ff, {0xed, 0x9f, 0xbf}, 3},
  {0xe000, {0xee, 0x80, 0x80}, 3},
  {0xfdd0, {0xef, 0xb7, 0x90}, 3},
  {0xfeff, {0xef, 0xbb, 0xbf}, 3},
  {0xffff, {0xef, 0xbf, 0xbf}, 3},
  {0x10000, {0xf0, 0x90, 0x80, 0x80}, 4},
  {0x1f600, {0xf0, 0x9f, 0x98, 0x80}, 4},
  {0x10ffff, {0xf4, 0x8f, 0xbf, 0xbf}, 4},
};

static int
write_character(
  UtfDecoder* decoder,
  const Character* character,
  unsigned encoding
) {
  int result = UTF_OK;
  if (encoding == 8) {
    for (size_t i = 0; i < character->count; i++) {
      result = utf_decoder_write_utf8(decoder, character->bytes[i]);
      if (result != UTF_OK) {
        return result;
      }
    }
    return result;
  }
  if (encoding == 16) {
    if (character->point <= 0xffff) {
      return utf_decoder_write_utf16(decoder, (uint16_t) character->point);
    }
    uint32_t offset = character->point - 0x10000;
    result = utf_decoder_write_utf16(
      decoder,
      (uint16_t) (0xd800 + (offset >> 10))
    );
    if (result != UTF_OK) {
      return result;
    }
    return utf_decoder_write_utf16(
      decoder,
      (uint16_t) (0xdc00 + (offset & 0x3ff))
    );
  }
  return utf_decoder_write_utf32(decoder, character->point);
}

static void
test_characters(
  void
) {
  for (unsigned encoding = 8; encoding <= 32; encoding *= 2) {
    for (size_t i = 0; i < COUNT(characters); i++) {
      Output output = {0};
      SioWriter writer = {.user = &output, .write = write_byte};
      UtfDecoder decoder;
      utf_decoder_init(&decoder, &writer);
      CHECK(write_character(&decoder, &characters[i], encoding) == UTF_OK);
      CHECK(utf_decoder_finish(&decoder) == UTF_OK);
      CHECK(output.count == characters[i].count);
      CHECK(memcmp(output.bytes, characters[i].bytes, output.count) == 0);
      utf_decoder_init(&decoder, NULL);
      CHECK(write_character(&decoder, &characters[i], encoding) == UTF_OK);
      CHECK(utf_decoder_finish(&decoder) == UTF_OK);
    }
  }
}

static void
test_invalid_utf8(
  void
) {
  const uint8_t invalid[][4] = {
    {0x80}, {0xbf}, {0xc0}, {0xc1}, {0xf5}, {0xf8}, {0xff},
    {0xc2, 0x7f}, {0xc2, 0xc0}, {0xe1, 0x80, 0x41},
    {0xf1, 0x80, 0x80, 0xc2}, {0xe0, 0x9f}, {0xed, 0xa0},
    {0xed, 0xbf}, {0xf0, 0x8f}, {0xf4, 0x90},
  };
  for (unsigned validate_only = 0; validate_only < 2; validate_only++) {
    for (size_t i = 0; i < COUNT(invalid); i++) {
      Output output = {0};
      SioWriter writer = {.user = &output, .write = write_byte};
      UtfDecoder decoder;
      utf_decoder_init(&decoder, validate_only ? NULL : &writer);
      int result = UTF_OK;
      for (size_t j = 0; j < 4 && result == UTF_OK; j++) {
        result = utf_decoder_write_utf8(&decoder, invalid[i][j]);
      }
      CHECK(result == UTF_ERROR_INVALID);
      CHECK(decoder.state == UTF_DECODER_STATE_FAILED);
      CHECK(output.count == 0);
    }
  }
}

static void
test_incomplete_utf8(
  void
) {
  for (size_t i = 0; i < COUNT(characters); i++) {
    for (size_t length = 1; length < characters[i].count; length++) {
      Output output = {0};
      SioWriter writer = {.user = &output, .write = write_byte};
      UtfDecoder decoder;
      utf_decoder_init(&decoder, &writer);
      for (size_t j = 0; j < length; j++) {
        CHECK(
          utf_decoder_write_utf8(&decoder, characters[i].bytes[j]) == UTF_OK
        );
      }
      CHECK(output.count == 0);
      CHECK(utf_decoder_finish(&decoder) == UTF_ERROR_INVALID);
      CHECK(decoder.state == UTF_DECODER_STATE_FAILED);
      utf_decoder_init(&decoder, &writer);
      CHECK(utf_decoder_write_utf8(&decoder, 'x') == UTF_OK);
      CHECK(output.count == 1 && output.bytes[0] == 'x');
    }
  }
}

static void
test_surrogates(
  void
) {
  UtfDecoder decoder;
  for (uint32_t unit = 0xd800; unit <= 0xdfff; unit++) {
    utf_decoder_init(&decoder, NULL);
    CHECK(utf_decoder_write_utf32(&decoder, unit) == UTF_ERROR_INVALID);
    utf_decoder_init(&decoder, NULL);
    int result = utf_decoder_write_utf16(&decoder, (uint16_t) unit);
    if (unit >= 0xdc00) {
      CHECK(result == UTF_ERROR_INVALID);
    } else {
      CHECK(result == UTF_OK);
      CHECK(utf_decoder_finish(&decoder) == UTF_ERROR_INVALID);
    }
    CHECK(decoder.state == UTF_DECODER_STATE_FAILED);
  }
  const uint16_t bad_seconds[] = {0, 0x41, 0xd7ff, 0xd800, 0xdbff, 0xe000};
  for (size_t i = 0; i < COUNT(bad_seconds); i++) {
    utf_decoder_init(&decoder, NULL);
    CHECK(utf_decoder_write_utf16(&decoder, 0xd800) == UTF_OK);
    CHECK(
      utf_decoder_write_utf16(&decoder, bad_seconds[i]) == UTF_ERROR_INVALID
    );
  }
  const uint32_t too_large[] = {0x110000, 0x7fffffff, UINT32_MAX};
  for (size_t i = 0; i < COUNT(too_large); i++) {
    utf_decoder_init(&decoder, NULL);
    CHECK(utf_decoder_write_utf32(&decoder, too_large[i]) == UTF_ERROR_INVALID);
  }
}

static void
test_mixed_input(
  void
) {
  Output output = {0};
  SioWriter writer = {.user = &output, .write = write_byte};
  UtfDecoder decoder;
  utf_decoder_init(&decoder, &writer);
  CHECK(utf_decoder_finish(&decoder) == UTF_OK);
  for (size_t i = 0; i < COUNT(characters); i++) {
    unsigned encoding = 8u << (i % 3);
    size_t start = output.count;
    CHECK(write_character(&decoder, &characters[i], encoding) == UTF_OK);
    CHECK(output.count - start == characters[i].count);
    CHECK(memcmp(
      output.bytes + start,
      characters[i].bytes,
      characters[i].count
    ) == 0);
  }
  CHECK(utf_decoder_finish(&decoder) == UTF_OK);
  CHECK(utf_decoder_finish(&decoder) == UTF_OK);
  CHECK(utf_decoder_write_utf8(&decoder, '!') == UTF_OK);
  CHECK(utf_decoder_finish(&decoder) == UTF_OK);
}

static void
test_interrupted_characters(
  void
) {
  UtfDecoder decoder;
  for (unsigned pending = 8; pending <= 16; pending *= 2) {
    for (unsigned next = 8; next <= 32; next *= 2) {
      if (pending == next) {
        continue;
      }
      utf_decoder_init(&decoder, NULL);
      if (pending == 8) {
        CHECK(utf_decoder_write_utf8(&decoder, 0xc2) == UTF_OK);
      } else {
        CHECK(utf_decoder_write_utf16(&decoder, 0xd800) == UTF_OK);
      }
      CHECK(
        write_character(&decoder, &characters[0], next) == UTF_ERROR_INVALID
      );
      CHECK(decoder.state == UTF_DECODER_STATE_FAILED);
    }
  }
}

static void
test_writer_errors(
  void
) {
  const int errors[] = {SIO_ERROR_IO, SIO_ERROR_EOF, SIO_ERROR_INVALID,
    SIO_ERROR_OVERFLOW, SIO_ERROR_NO_MEMORY, -1, 99};
  for (unsigned encoding = 8; encoding <= 32; encoding *= 2) {
    for (size_t i = 0; i < COUNT(characters); i++) {
      for (size_t byte = 1; byte <= characters[i].count; byte++) {
        for (size_t e = 0; e < COUNT(errors); e++) {
          Output output = {.fail_at = byte, .error = errors[e]};
          SioWriter writer = {.user = &output, .write = write_byte};
          UtfDecoder decoder;
          utf_decoder_init(&decoder, &writer);
          CHECK(
            write_character(&decoder, &characters[i], encoding)
              == UTF_ERROR_WRITE
          );
          CHECK(decoder.state == UTF_DECODER_STATE_FAILED);
          CHECK(output.calls == byte);
          CHECK(output.count == byte - 1);
          CHECK(memcmp(output.bytes, characters[i].bytes, output.count) == 0);
          output = (Output) {0};
          utf_decoder_init(&decoder, &writer);
          CHECK(write_character(&decoder, &characters[i], encoding) == UTF_OK);
          CHECK(utf_decoder_finish(&decoder) == UTF_OK);
        }
      }
    }
  }
}

static void
test_sio_output(
  void
) {
  uint8_t bytes[5] = {42, 42, 42, 42, 42};
  SioBufferWriter writer;
  sio_buffer_writer_init(&writer, bytes, 4);
  UtfDecoder decoder;
  utf_decoder_init(&decoder, (SioWriter*) &writer);
  CHECK(utf_decoder_write_utf16(&decoder, 0xd83d) == UTF_OK);
  CHECK(sio_buffer_writer_get_size(&writer) == 0);
  CHECK(utf_decoder_write_utf16(&decoder, 0xde00) == UTF_OK);
  CHECK(utf_decoder_finish(&decoder) == UTF_OK);
  const uint8_t expected[] = {0xf0, 0x9f, 0x98, 0x80, 42};
  CHECK(memcmp(bytes, expected, sizeof(bytes)) == 0);
  CHECK(sio_buffer_writer_get_size(&writer) == 4);
  CHECK(utf_decoder_write_utf8(&decoder, 'x') == UTF_ERROR_WRITE);
  CHECK(decoder.state == UTF_DECODER_STATE_FAILED);
  CHECK(sio_buffer_writer_get_size(&writer) == 4);

  sio_buffer_writer_init(&writer, bytes, 2);
  utf_decoder_init(&decoder, (SioWriter*) &writer);
  CHECK(utf_decoder_write_utf32(&decoder, 0x1f600) == UTF_ERROR_WRITE);
  CHECK(sio_buffer_writer_get_size(&writer) == 2);
  CHECK(bytes[0] == 0xf0 && bytes[1] == 0x9f);
  CHECK(decoder.state == UTF_DECODER_STATE_FAILED);

  sio_buffer_writer_reset(&writer);
  utf_decoder_init(&decoder, (SioWriter*) &writer);
  CHECK(utf_decoder_write_utf8(&decoder, 0xc2) == UTF_OK);
  CHECK(sio_buffer_writer_get_size(&writer) == 0);
  utf_decoder_init(&decoder, NULL);
  CHECK(utf_decoder_finish(&decoder) == UTF_OK);
  CHECK(utf_decoder_write_utf16(&decoder, 0xd800) == UTF_OK);
  CHECK(utf_decoder_finish(&decoder) == UTF_ERROR_INVALID);
  utf_decoder_init(&decoder, NULL);
  CHECK(utf_decoder_write_utf8(&decoder, 0xe0) == UTF_OK);
  CHECK(utf_decoder_finish(&decoder) == UTF_ERROR_INVALID);
}

int
main(
  void
) {
  test_characters();
  test_invalid_utf8();
  test_incomplete_utf8();
  test_surrogates();
  test_mixed_input();
  test_interrupted_characters();
  test_writer_errors();
  test_sio_output();
  puts("8 test groups passed");
  return EXIT_SUCCESS;
}
