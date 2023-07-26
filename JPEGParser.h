#ifndef JPEGSTREAMER_JPEGPARSER_H
#define JPEGSTREAMER_JPEGPARSER_H

#include <cstdint>
#include <cstdlib>
#include <stdio.h>
#include <vector>

#define ROUND_UP_8(num) (((num) + 7) & ~7)
#define N_ELEMENTS(arr) (sizeof(arr) / sizeof((arr)[0]))

#define DEFAULT_JPEG_QUANT 255

#define DEFAULT_JPEG_QUALITY 255
#define DEFAULT_JPEG_TYPE 1

#define RTP_HEADER_LEN 12

#define PRINTF //

namespace JpegParser
{

  /*
   * RtpRestartMarkerHeader:
   * @restartInterval: number of MCUs that appear between restart markers
   * @restartFirstLastCount: a combination of the first packet mark in the chunk
   *                         last packet mark in the chunk and the position of the
   *                         first restart interval in the current "chunk"
   *
   *    0                   1                   2                   3
   *   0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
   *  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   *  |       Restart Interval        |F|L|       Restart Count       |
   *  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   *
   *  The restart marker header is implemented according to the following
   *  methodology specified in section 3.1.7 of rfc2435.txt.
   *
   *  "If the restart intervals in a frame are not guaranteed to be aligned
   *  with packet boundaries, the F (first) and L (last) bits MUST be set
   *  to 1 and the Restart Count MUST be set to 0x3FFF.  This indicates
   *  that a receiver MUST reassemble the entire frame before decoding it."
   *
   */

  typedef struct
  {
    uint16_t restart_interval;
    uint16_t restart_count;
  } RtpRestartMarkerHeader;

  typedef struct
  {
    uint8_t        size;
    const uint8_t* data;
  } RtpQuantTable;

  enum RtpJpegMarker
  {
    JPEG_MARKER       = 0xFF,
    JPEG_MARKER_SOI   = 0xD8,
    JPEG_MARKER_JFIF  = 0xE0,
    JPEG_MARKER_CMT   = 0xFE,
    JPEG_MARKER_DQT   = 0xDB,
    JPEG_MARKER_SOF   = 0xC0,
    JPEG_MARKER_DHT   = 0xC4,
    JPEG_MARKER_JPG   = 0xC8,
    JPEG_MARKER_SOS   = 0xDA,
    JPEG_MARKER_EOI   = 0xD9,
    JPEG_MARKER_DRI   = 0xDD,
    JPEG_MARKER_APP0  = 0xE0,
    JPEG_MARKER_H264  = 0xE4, /* APP4 */
    JPEG_MARKER_APP15 = 0xEF,
    JPEG_MARKER_JPG0  = 0xF0,
    JPEG_MARKER_JPG13 = 0xFD
  };

  typedef struct
  {
    uint8_t id;
    uint8_t samp;
    uint8_t qt;
  } CompInfo;

  struct RtpJPEGPayload
  {
    RtpJPEGPayload()
    {
      quality   = DEFAULT_JPEG_QUALITY;
      quant     = DEFAULT_JPEG_QUANT;
      type      = DEFAULT_JPEG_TYPE;
      width     = -1;
      height    = -1;
      payload   = nullptr;
      size      = 0;
      timestamp = 0;
    }

    uint8_t* payload;

    uint8_t quality;
    uint8_t type;

    int height;
    int width;

    uint8_t quant;

    uint32_t size;
    uint64_t timestamp;
  };

  uint8_t read_uint8_t(const uint8_t* buffer, uint32_t total_size, uint32_t& offset);

  uint8_t scan_marker(const uint8_t* buffer, uint32_t total_size, uint32_t& offset);

  uint16_t read_uint16_t(const uint8_t* buffer, uint32_t total_size, uint32_t& offset);

  void skip_marker(const uint8_t* buffer, uint32_t total_size, uint32_t& offset);

  bool read_sof(RtpJPEGPayload* pay, const uint8_t* buffer, uint32_t total_size, uint32_t& offset, CompInfo info[]);

  bool read_dri(RtpJPEGPayload*         pay,
                const uint8_t*          buffer,
                uint32_t                total_size,
                uint32_t&               offset,
                RtpRestartMarkerHeader* dri);

  void read_quant_table(const uint8_t* buffer, uint32_t total_size, uint32_t& offset, RtpQuantTable tables[]);

  RtpJPEGPayload print_error(const char* error);

  RtpJPEGPayload handle_buffer(uint8_t*              buffer,
                               uint32_t              total_size,
                               uint64_t              timestamp,
                               std::vector<uint8_t>& quantisation,
                               unsigned&             precision);

} // namespace JpegParser

#endif // JPEGSTREAMER_JPEGPARSER_H
