#ifndef JPEGSTREAMER_GSTJPEGPARSER_H
#define JPEGSTREAMER_GSTJPEGPARSER_H

#include <cstdint>
#include <cstdlib>
#include <stdio.h>

#define GST_ROUND_UP_8(num) (((num) + 7) & ~7)
#define G_N_ELEMENTS(arr) (sizeof(arr) / sizeof((arr)[0]))

int print_error(const char* error)
{
  fprintf(stderr, "%s", error);
  return -1;
}

/*
 * RtpJpegHeader:
 * @type_spec: type specific
 * @offset: fragment offset
 * @type: type field
 * @q: quantization table for this frame
 * @width: width of image in 8-pixel multiples
 * @height: height of image in 8-pixel multiples
 *
 * 0                   1                   2                   3
 * 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * | Type-specific |              Fragment Offset                  |
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * |      Type     |       Q       |     Width     |     Height    |
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 */
struct RtpJpegHeader
{
  uint type_spec;
  uint offset;
  uint8_t type;
  uint8_t q;
  uint8_t width;
  uint8_t height;
};

/*
 * RtpQuantHeader
 * @mbz: must be zero
 * @precision: specify size of quantization tables
 * @length: length of quantization data
 *
 * 0                   1                   2                   3
 * 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * |      MBZ      |   Precision   |             Length            |
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * |                    Quantization Table Data                    |
 * |                              ...                              |
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 */
typedef struct
{
  uint8_t mbz;
  uint8_t precision;
  uint16_t length;
} RtpQuantHeader;

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

enum _RtpJpegMarker
{
  JPEG_MARKER = 0xFF,
  JPEG_MARKER_SOI = 0xD8,
  JPEG_MARKER_JFIF = 0xE0,
  JPEG_MARKER_CMT = 0xFE,
  JPEG_MARKER_DQT = 0xDB,
  JPEG_MARKER_SOF = 0xC0,
  JPEG_MARKER_DHT = 0xC4,
  JPEG_MARKER_JPG = 0xC8,
  JPEG_MARKER_SOS = 0xDA,
  JPEG_MARKER_EOI = 0xD9,
  JPEG_MARKER_DRI = 0xDD,
  JPEG_MARKER_APP0 = 0xE0,
  JPEG_MARKER_H264 = 0xE4,      /* APP4 */
  JPEG_MARKER_APP15 = 0xEF,
  JPEG_MARKER_JPG0 = 0xF0,
  JPEG_MARKER_JPG13 = 0xFD
};

typedef struct
{
  uint8_t id;
  uint8_t samp;
  uint8_t qt;
} CompInfo;

struct GstRtpJPEGPay
{
  const uint8_t* payload;

  uint8_t quality;
  uint8_t type;

  int height;
  int width;

  uint8_t quant;
};

static uint8_t parse_mem_inc_offset_guint8 (const uint8_t* buffer, uint32_t total_size,  uint32_t& offset)
{
  uint8_t data;

  if (total_size < offset)
    return 0;

  data = *(buffer + offset);
  ++offset;

  return data;
}

static uint8_t gst_rtp_jpeg_pay_scan_marker(const uint8_t* buffer, uint32_t total_size,  uint32_t& offset)
{
  uint8_t marker = parse_mem_inc_offset_guint8(buffer, total_size, offset);

  while (marker != JPEG_MARKER && ((offset) < total_size))
  {
    marker = parse_mem_inc_offset_guint8(buffer, total_size, offset);
  }

  if (offset >= total_size)
  {
    printf("found EOI marker\n");
    return JPEG_MARKER_EOI;
  }
  else
  {
    marker = parse_mem_inc_offset_guint8(buffer, total_size, offset);
    return marker;
  }
}

/*
 * get uint16 value from current position in mapped memory.
 * the memory offset will be increased with 2.
 */
static uint16_t parse_mem_inc_offset_guint16 (const uint8_t* buffer, uint32_t total_size, uint32_t& offset)
{
  uint16_t data;

  if (total_size < (offset + 1))
    return 0;

  data = ((uint16_t) *(buffer + offset)) << 8;
  ++offset;
  data = data | (*(buffer + offset));
  ++offset;

  return data;
}

static void gst_rtp_jpeg_pay_skipping_marker(const uint8_t* buffer, uint32_t total_size, uint32_t& offset)
{
  uint skip;

  if ((offset + 1) >= total_size)
  {
    goto wrong_size;
  }
  skip = parse_mem_inc_offset_guint16(buffer, total_size, offset);

  if ((skip - 2 + offset) > total_size)
  {
    goto wrong_size;
  }
  if (skip > 2)
  {
    offset += skip - 2;
  }
  return;

wrong_size:
  fprintf(stderr, "not enough data\n");
}

static bool gst_rtp_jpeg_pay_read_sof(GstRtpJPEGPay* pay,
                                      const uint8_t* buffer,
                                      uint32_t       total_size,
                                      uint32_t&      offset,
                                      CompInfo       info[])
{
  uint sof_size, off;
  uint width, height, infolen;
  CompInfo elem;
  uint i, j;

  off = offset;

  /* we need at least 17 bytes for the SOF */
  if (off + 17 > total_size)
    goto wrong_size;

  sof_size = parse_mem_inc_offset_guint16 (buffer, total_size, offset);
  if (sof_size < 17)
    goto wrong_length;

  /* precision should be 8 */
  if (parse_mem_inc_offset_guint8 (buffer, total_size, offset) != 8)
    goto bad_precision;

  /* read dimensions */
  height = parse_mem_inc_offset_guint16 (buffer, total_size, offset);
  width = parse_mem_inc_offset_guint16 (buffer, total_size, offset);

  printf("got dimensions %ux%u\n", width, height);

  if (height == 0) {
    goto invalid_dimension;
  }
  if (height > 2040) {
    height = 0;
  }
  if (width == 0) {
    goto invalid_dimension;
  }
  if (width > 2040) {
    width = 0;
  }

  if (height == 0 || width == 0) {
    pay->height = 0;
    pay->width = 0;
  } else {
    pay->height = GST_ROUND_UP_8 (height) / 8;
    pay->width = GST_ROUND_UP_8 (width) / 8;
  }

  /* we only support 3 components */
  if (parse_mem_inc_offset_guint8 (buffer, total_size, offset) != 3)
    goto bad_components;

  infolen = 0;
  for (i = 0; i < 3; i++) {
    elem.id = parse_mem_inc_offset_guint8 (buffer, total_size, offset);
    elem.samp = parse_mem_inc_offset_guint8 (buffer, total_size, offset);
    elem.qt = parse_mem_inc_offset_guint8 (buffer, total_size, offset);
    printf("got comp %d, samp %02x, qt %d\n", elem.id, elem.samp, elem.qt);
    /* insertion sort from the last element to the first */
    for (j = infolen; j > 1; j--) {
      if (info[j - 1].id < elem.id)
        break;
      info[j] = info[j - 1];
    }
    info[j] = elem;
    infolen++;
  }

  /* see that the components are supported */
  if (info[0].samp == 0x21)
    pay->type = 0;
  else if (info[0].samp == 0x22)
    pay->type = 1;
  else
    goto invalid_comp;

  if (!(info[1].samp == 0x11))
    goto invalid_comp;

  if (!(info[2].samp == 0x11))
    goto invalid_comp;

  return true;

  /* ERRORS */
wrong_size:
{
  fprintf(stderr, "Wrong size %u (needed %u).", (uint)total_size, off + 17);
  return false;
}
wrong_length:
{
  fprintf(stderr, "Wrong SOF length %u.", sof_size);
  return false;
}
bad_precision:
{
  fprintf(stderr, "Wrong precision, expecting 8.");
  return false;
}
invalid_dimension:
{
  fprintf(stderr, "Wrong dimension, size %ux%u", width, height);
  return false;
}
bad_components:
{
  fprintf(stderr, "Wrong number of components");
  return false;
}
invalid_comp:
{
  fprintf(stderr, "Invalid component");
  return false;
}
}

static bool gst_rtp_jpeg_pay_read_dri(GstRtpJPEGPay*          pay,
                                      const uint8_t*          buffer,
                                      uint32_t                total_size,
                                      uint32_t&               offset,
                                      RtpRestartMarkerHeader* dri)
{
  uint dri_size, restart_interval;

  /* we need at least 4 bytes for the DRI */
  if (offset + 4 > total_size)
    goto wrong_size;

  dri_size = parse_mem_inc_offset_guint16 (buffer, total_size, offset);
  if (dri_size < 4)
    goto wrong_length;

  restart_interval = parse_mem_inc_offset_guint16 (buffer, total_size, offset);
  dri->restart_interval = htons (restart_interval);
  dri->restart_count = htons (0xFFFF);

  offset += dri_size - 4;
  if (offset > total_size) {
    goto wrong_size;
  }

  return dri->restart_interval > 0;

wrong_size:
{
  fprintf(stderr, "not enough data for DRI");
  return false;
}
wrong_length:
{
  fprintf(stderr, "DRI size too small (%u)", dri_size);
  /* offset got incremented by two when dri_size was parsed. */
  if (dri_size > 2)
    offset += dri_size - 2;
  return false;
}
}

static void gst_rtp_jpeg_pay_read_quant_table(const uint8_t* buffer,
                                              uint32_t       total_size,
                                              uint32_t&      offset,
                                              RtpQuantTable  tables[])
{
  uint    quant_size, tab_size;
  uint8_t prec;
  uint8_t id;

  if (total_size <= (offset + 1))
    goto too_small;

  quant_size = parse_mem_inc_offset_guint16(buffer, total_size, offset);
  if (quant_size < 2)
    goto small_quant_size;

  /* clamp to available data */
  if (offset + quant_size > total_size)
    quant_size = total_size - offset;

  quant_size -= 2;

  while (quant_size > 0)
  {
    uint8_t data;
    /* not enough to read the id */
    if (offset + 1 > total_size)
      break;

    data = parse_mem_inc_offset_guint8(buffer, total_size, offset);
    id   = data & 0x0f;
    if (id == 15)
      /* invalid id received - corrupt data */
      goto invalid_id;

    prec = (data & 0xf0) >> 4;
    if (prec)
      tab_size = 128;
    else
      tab_size = 64;

    /* there is not enough for the table */
    if (quant_size < tab_size + 1)
      goto no_table;

    printf("read quant table %d, tab_size %d, prec %02x\n", id, tab_size, prec);

    tables[id].size = tab_size;
    tables[id].data = buffer + offset;

    quant_size -= (tab_size + 1);
    offset += tab_size;
    if (offset > total_size)
    {
      goto too_small;
    }
  }
done:
  return;

  /* ERRORS */
too_small : {
  fprintf(stderr, "not enough data\n");
  return;
}
small_quant_size : {
  fprintf(stderr, "quant_size too small (%u < 2)\n", quant_size);
  return;
}
invalid_id : {
  fprintf(stderr, "invalid id\n");
  goto done;
}
no_table : {
  fprintf(stderr, "not enough data for table (%u < %u)\n", quant_size, tab_size + 1);
  goto done;
}
}

static inline int gst_rtp_jpeg_pay_handle_buffer(const uint8_t* basepayload, const uint8_t* buffer, uint32_t total_size, uint64_t timestamp)
{
  GstRtpJPEGPay* pay = new GstRtpJPEGPay();
  RtpJpegHeader jpeg_header;
  RtpQuantHeader quant_header;
  RtpRestartMarkerHeader restart_marker_header;
  RtpQuantTable tables[15] = { {0, NULL}, };
  CompInfo info[3] = { {0,}, };
  uint quant_data_size;
  uint mtu, max_payload_size;
  uint bytes_left;
  uint jpeg_header_size = 0;
  uint offset = 0;
  bool frame_done;
  bool sos_found, sof_found, dqt_found, dri_found;
  int i = 0;
//  GstBufferList *list = NULL;

  pay->payload = basepayload;

  printf(" ---- got buffer size %ld, timestamp %ld\n", total_size, timestamp);

  /* parse the jpeg header for 'start of scan' and read quant tables if needed */
  sos_found = false;
  dqt_found = false;
  sof_found = false;
  dri_found = false;

  while (!sos_found && (offset < total_size)) {
    int marker;

    // printf("checking from offset %u\n", offset);
    marker = gst_rtp_jpeg_pay_scan_marker (buffer, total_size, offset);
    switch (marker) {
    case JPEG_MARKER_JFIF:
    case JPEG_MARKER_CMT:
    case JPEG_MARKER_DHT:
    case JPEG_MARKER_H264:
      printf("skipping marker 0x%02x\n", marker);
      gst_rtp_jpeg_pay_skipping_marker (buffer, total_size, offset);
      break;
    case JPEG_MARKER_SOF:
      printf ("SOF found\n");
      sof_found = true;
      if (!gst_rtp_jpeg_pay_read_sof (pay, buffer, total_size, offset, info))
        print_error("invalid_format");
      break;
    case JPEG_MARKER_DQT:
      printf ("DQT found\n");
      gst_rtp_jpeg_pay_read_quant_table (buffer, total_size, offset, tables);
      dqt_found = true;
      break;
    case JPEG_MARKER_SOS:
      sos_found = true;
      printf("SOS found\n");
      jpeg_header_size = offset;
      /* Do not re-combine into single statement with previous line! */
      jpeg_header_size += parse_mem_inc_offset_guint16 (buffer, total_size, offset);
      break;
    case JPEG_MARKER_EOI:
      fprintf(stderr, "EOI reached before SOS!\n");
      break;
    case JPEG_MARKER_SOI:
      printf("SOI found\n");
      break;
    case JPEG_MARKER_DRI:
      printf("DRI found\n");
      if (gst_rtp_jpeg_pay_read_dri (pay, buffer, total_size, offset, &restart_marker_header))
        dri_found = true;
      break;
    default:
      if (marker == JPEG_MARKER_JPG ||
          (marker >= JPEG_MARKER_JPG0 && marker <= JPEG_MARKER_JPG13) ||
          (marker >= JPEG_MARKER_APP0 && marker <= JPEG_MARKER_APP15)) {
        printf("skipping marker\n");
        gst_rtp_jpeg_pay_skipping_marker (buffer, total_size, offset);
      } else {
        /* no need to do anything, gst_rtp_jpeg_pay_scan_marker will go on */
        fprintf(stderr, "unhandled marker 0x%02x\n", marker);
      }
      break;
    }
  }

  if (!dqt_found || !sof_found)
    return print_error("unsupported_jpeg");
  /* by now we should either have negotiated the width/height or the SOF header
   * should have filled us in */
  if (pay->width < 0 || pay->height < 0)
  {
    return print_error("no_dimension");
  }

  printf("header size %u\n", jpeg_header_size);

  offset = 0;

  if (dri_found)
    pay->type += 64;

  /* prepare stuff for the jpeg header */

  jpeg_header.type_spec = 0;
  jpeg_header.type = pay->type;
  jpeg_header.q = pay->quant;
  jpeg_header.width = pay->width;
  jpeg_header.height = pay->height;
  /* collect the quant headers sizes */
  quant_header.mbz = 0;
  quant_header.precision = 0;
  quant_header.length = 0;
  quant_data_size = 0;

  if (pay->quant > 127) {
    /* for the Y and U component, look up the quant table and its size. quant
     * tables for U and V should be the same */
    for (i = 0; i < 2; i++) {
      uint qsize;
      uint qt;

      qt = info[i].qt;
      if (qt >= G_N_ELEMENTS (tables))
        print_error("invalid_quant");

      qsize = tables[qt].size;
      if (qsize == 0)
        print_error("invalid_quant");

      quant_header.precision |= (qsize == 64 ? 0 : (1 << i));
      quant_data_size += qsize;
    }
    quant_header.length = htons (quant_data_size);
    quant_data_size += sizeof (quant_header);
  }

  int x = 0;
//  GST_LOG_OBJECT (pay, "quant_data size %u", quant_data_size);
//
//  bytes_left =
//      sizeof (jpeg_header) + quant_data_size + total_size -
//      jpeg_header_size;
//
//  if (dri_found)
//    bytes_left += sizeof (restart_marker_header);
//
//  max_payload_size = mtu - (RTP_HEADER_LEN + sizeof (jpeg_header));
//  list = gst_buffer_list_new_sized ((bytes_left / max_payload_size) + 1);
//
//  frame_done = false;
//  do {
//    GstBuffer *outbuf;
//    uint8 *payload;
//    uint payload_size;
//    uint header_size;
//    GstBuffer *paybuf;
//    GstRTPBuffer rtp = { NULL };
//    uint rtp_header_size = gst_rtp_buffer_calc_header_len (0);
//
//    *//* The available room is the packet MTU, minus the RTP header length. *//*
//    payload_size =
//        (bytes_left < (mtu - rtp_header_size) ? bytes_left :
//                                              (mtu - rtp_header_size));
//
//    header_size = sizeof (jpeg_header) + quant_data_size;
//    if (dri_found)
//      header_size += sizeof (restart_marker_header);
//
//    outbuf =
//        gst_rtp_base_payload_allocate_output_buffer (basepayload, header_size,
//                                                    0, 0);
//
//    gst_rtp_buffer_map (outbuf, GST_MAP_WRITE, &rtp);
//
//    if (payload_size == bytes_left) {
//      GST_LOG_OBJECT (pay, "last packet of frame");
//      frame_done = true;
//      gst_rtp_buffer_set_marker (&rtp, 1);
//    }
//
//    payload = gst_rtp_buffer_get_payload (&rtp);
//
//    /* update offset */
//#if (G_BYTE_ORDER == G_LITTLE_ENDIAN)
//    jpeg_header.offset = ((offset & 0x0000FF) << 16) |
//                         ((offset & 0xFF0000) >> 16) | (offset & 0x00FF00);
//#else
//    jpeg_header.offset = offset;
//#endif
//    memcpy (payload, &jpeg_header, sizeof (jpeg_header));
//    payload += sizeof (jpeg_header);
//    payload_size -= sizeof (jpeg_header);
//
//    if (dri_found) {
//      memcpy (payload, &restart_marker_header, sizeof (restart_marker_header));
//      payload += sizeof (restart_marker_header);
//      payload_size -= sizeof (restart_marker_header);
//    }
//
//    /* only send quant table with first packet */
//    if (G_UNLIKELY (quant_data_size > 0)) {
//      memcpy (payload, &quant_header, sizeof (quant_header));
//      payload += sizeof (quant_header);
//
//      /* copy the quant tables for luma and chrominance */
//      for (i = 0; i < 2; i++) {
//        uint qsize;
//        uint qt;
//
//        qt = info[i].qt;
//        qsize = tables[qt].size;
//        memcpy (payload, tables[qt].data, qsize);
//
//        GST_LOG_OBJECT (pay, "component %d using quant %d, size %d", i, qt,
//                       qsize);
//
//        payload += qsize;
//      }
//      payload_size -= quant_data_size;
//      bytes_left -= quant_data_size;
//      quant_data_size = 0;
//    }
//    GST_LOG_OBJECT (pay, "sending payload size %d", payload_size);
//    gst_rtp_buffer_unmap (&rtp);
//
//    *//* create a new buf to hold the payload *//*
//    paybuf = gst_buffer_copy_region (buffer, GST_BUFFER_COPY_ALL,
//                                    jpeg_header_size + offset, payload_size);
//
//    *//* join memory parts *//*
//    gst_rtp_copy_video_meta (pay, outbuf, paybuf);
//    outbuf = gst_buffer_append (outbuf, paybuf);
//
//    GST_BUFFER_PTS (outbuf) = timestamp;
//
//    if (discont) {
//      GST_BUFFER_FLAG_SET (outbuf, GST_BUFFER_FLAG_DISCONT);
//      *//* Only the first outputted buffer has the DISCONT flag *//*
//      discont = false;
//    }
//
//    *//* and add to list *//*
//    gst_buffer_list_insert (list, -1, outbuf);
//
//    bytes_left -= payload_size;
//    offset += payload_size;
//  }
//  while (!frame_done);
//  /* push the whole buffer list at once */
//  ret = gst_rtp_base_payload_push_list (basepayload, list);
//
//  gst_buffer_memory_unmap (&memory);
//  gst_buffer_unref (buffer);
//
//  return ret;
//
//  /* ERRORS */
//unsupported_jpeg:
//{
//  GST_ELEMENT_WARNING (pay, STREAM, FORMAT, ("Unsupported JPEG"), (NULL));
//  gst_buffer_memory_unmap (&memory);
//  gst_buffer_unref (buffer);
//  return GST_FLOW_OK;
//}
//no_dimension:
//{
//  GST_ELEMENT_WARNING (pay, STREAM, FORMAT, ("No size given"), (NULL));
//  gst_buffer_memory_unmap (&memory);
//  gst_buffer_unref (buffer);
//  return GST_FLOW_OK;
//}
//invalid_quant:
//{
//  GST_ELEMENT_WARNING (pay, STREAM, FORMAT, ("Invalid quant tables"), (NULL));
//  gst_buffer_memory_unmap (&memory);
//  gst_buffer_unref (buffer);
//  return GST_FLOW_OK;
//}

}

#endif // JPEGSTREAMER_GSTJPEGPARSER_H
