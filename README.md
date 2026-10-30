# utf99

Allocation-free, incremental UTF decoding and validation for C99. Accepts
UTF-8 bytes, numeric UTF-16 code units, and numeric UTF-32 code units. Output
is UTF-8 written through an optional `SioWriter`. A `NULL` writer validates
without producing output.

## Building

```sh
git clone --recurse-submodules https://github.com/johnoliverdriscoll/utf99.git
cmake -S utf99 -B utf99/build
cmake --build utf99/build
ctest --test-dir utf99/build --output-on-failure
```

See [BUILDING.md](BUILDING.md) for installation and build options.

## Usage

```c
#include <stddef.h>
#include <utf99/utf.h>
#include <sio99/sio_buffer_writer.h>

int encode_smile(void) {
  uint8_t bytes[4];
  SioBufferWriter writer;
  sio_buffer_writer_init(&writer, bytes, sizeof(bytes));

  UtfDecoder decoder;
  utf_decoder_init(&decoder, (SioWriter*) &writer);
  int result = utf_decoder_write_utf32(&decoder, 0x1f600);
  if (result != UTF_OK) {
    return result;
  }
  return utf_decoder_finish(&decoder);
}
```

Call `utf_decoder_finish` at the end of input to reject an unfinished UTF-8
sequence or unmatched UTF-16 surrogate. Input encodings may be mixed between
complete characters. Output has no added terminator. Destination errors
become `UTF_ERROR_WRITE` and may leave partial output. Reinitialize the decoder
after any error before writing or finishing again.

See the public header for complete interface contracts.
