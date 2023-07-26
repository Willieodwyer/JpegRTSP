#include "JPEGParser.h"

uint8_t JpegParser::read_uint8_t(const uint8_t* buffer, uint32_t total_size, uint32_t& offset)
{
  uint8_t data;

  if (total_size < offset)
    return 0;

  data = *(buffer + offset);
  ++offset;

  return data;
}

uint8_t JpegParser::scan_marker(const uint8_t* buffer, uint32_t total_size, uint32_t& offset)
{
  uint8_t marker = read_uint8_t(buffer, total_size, offset);

  while (marker != JPEG_MARKER && ((offset) < total_size))
  {
    marker = read_uint8_t(buffer, total_size, offset);
  }

  if (offset >= total_size)
  {
    PRINTF("found EOI marker\n");
    return JPEG_MARKER_EOI;
  }
  else
  {
    marker = read_uint8_t(buffer, total_size, offset);
    return marker;
  }
}

uint16_t JpegParser::read_uint16_t(const uint8_t* buffer, uint32_t total_size, uint32_t& offset)
{
  uint16_t data;

  if (total_size < (offset + 1))
    return 0;

  data = ((uint16_t) * (buffer + offset)) << 8;
  ++offset;
  data = data | (*(buffer + offset));
  ++offset;

  return data;
}

void JpegParser::skip_marker(const uint8_t* buffer, uint32_t total_size, uint32_t& offset)
{
  uint skip;

  if ((offset + 1) >= total_size)
  {
    goto wrong_size;
  }
  skip = read_uint16_t(buffer, total_size, offset);

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

bool JpegParser::read_sof(RtpJPEGPayload* pay,
                          const uint8_t*  buffer,
                          uint32_t        total_size,
                          uint32_t&       offset,
                          CompInfo*       info)
{
  uint     sof_size, off;
  uint     width, height, infolen;
  CompInfo elem;
  uint     i, j;

  off = offset;

  /* we need at least 17 bytes for the SOF */
  if (off + 17 > total_size)
    goto wrong_size;

  sof_size = read_uint16_t(buffer, total_size, offset);
  if (sof_size < 17)
    goto wrong_length;

  /* precision should be 8 */
  if (read_uint8_t(buffer, total_size, offset) != 8)
    goto bad_precision;

  /* read dimensions */
  height = read_uint16_t(buffer, total_size, offset);
  width  = read_uint16_t(buffer, total_size, offset);

  PRINTF("got dimensions %ux%u\n", width, height);

  if (height == 0)
  {
    goto invalid_dimension;
  }
  if (height > 2040)
  {
    height = 0;
  }
  if (width == 0)
  {
    goto invalid_dimension;
  }
  if (width > 2040)
  {
    width = 0;
  }

  if (height == 0 || width == 0)
  {
    pay->height = 0;
    pay->width  = 0;
  }
  else
  {
    pay->height = ROUND_UP_8(height) / 8;
    pay->width  = ROUND_UP_8(width) / 8;
  }

  /* we only support 3 components */
  if (read_uint8_t(buffer, total_size, offset) != 3)
    goto bad_components;

  infolen = 0;
  for (i = 0; i < 3; i++)
  {
    elem.id   = read_uint8_t(buffer, total_size, offset);
    elem.samp = read_uint8_t(buffer, total_size, offset);
    elem.qt   = read_uint8_t(buffer, total_size, offset);
    PRINTF("got comp %d, samp %02x, qt %d\n", elem.id, elem.samp, elem.qt);
    /* insertion sort from the last element to the first */
    for (j = infolen; j > 1; j--)
    {
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
wrong_size : {
  fprintf(stderr, "Wrong size %u (needed %u).", (uint)total_size, off + 17);
  return false;
}
wrong_length : {
  fprintf(stderr, "Wrong SOF length %u.", sof_size);
  return false;
}
bad_precision : {
  fprintf(stderr, "Wrong precision, expecting 8.");
  return false;
}
invalid_dimension : {
  fprintf(stderr, "Wrong dimension, size %ux%u", width, height);
  return false;
}
bad_components : {
  fprintf(stderr, "Wrong number of components");
  return false;
}
invalid_comp : {
  fprintf(stderr, "Invalid component");
  return false;
}
}

bool JpegParser::read_dri(RtpJPEGPayload*         pay,
                          const uint8_t*          buffer,
                          uint32_t                total_size,
                          uint32_t&               offset,
                          RtpRestartMarkerHeader* dri)
{
  uint dri_size, restart_interval;

  /* we need at least 4 bytes for the DRI */
  if (offset + 4 > total_size)
    goto wrong_size;

  dri_size = read_uint16_t(buffer, total_size, offset);
  if (dri_size < 4)
    goto wrong_length;

  restart_interval      = read_uint16_t(buffer, total_size, offset);
  dri->restart_interval = restart_interval;
  dri->restart_count    = 0xFFFF;

  offset += dri_size - 4;
  if (offset > total_size)
  {
    goto wrong_size;
  }

  return dri->restart_interval > 0;

wrong_size : {
  fprintf(stderr, "not enough data for DRI");
  return false;
}
wrong_length : {
  fprintf(stderr, "DRI size too small (%u)", dri_size);
  /* offset got incremented by two when dri_size was parsed. */
  if (dri_size > 2)
    offset += dri_size - 2;
  return false;
}
}

void JpegParser::read_quant_table(const uint8_t* buffer, uint32_t total_size, uint32_t& offset, RtpQuantTable* tables)
{
  uint    quant_size, tab_size;
  uint8_t prec;
  uint8_t id;

  if (total_size <= (offset + 1))
    goto too_small;

  quant_size = read_uint16_t(buffer, total_size, offset);
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

    data = read_uint8_t(buffer, total_size, offset);
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

    PRINTF("read quant table %d, tab_size %d, prec %02x\n", id, tab_size, prec);

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

JpegParser::RtpJPEGPayload JpegParser::print_error(const char* error)
{
  fprintf(stderr, "%s", error);
  return RtpJPEGPayload();
}

JpegParser::RtpJPEGPayload JpegParser::handle_buffer(uint8_t*              buffer,
                                                     uint32_t              total_size,
                                                     uint64_t              timestamp,
                                                     std::vector<uint8_t>& quantisation,
                                                     unsigned int&         precision)
{
  RtpJPEGPayload         pay;
  RtpRestartMarkerHeader restart_marker_header;

  RtpQuantTable tables[15] = {
      {0, NULL},
  };
  CompInfo info[3] = {
      {
          0,
      },
  };

  uint quant_data_size;
  uint jpeg_header_size = 0;
  uint offset           = 0;
  bool sos_found, sof_found, dqt_found, dri_found;
  int  i = 0;

  PRINTF(" ---- got buffer size %ld, timestamp %ld\n", total_size, timestamp);

  /* parse the jpeg header for 'start of scan' and read quant tables if needed */
  sos_found = false;
  dqt_found = false;
  sof_found = false;
  dri_found = false;

  while (!sos_found && (offset < total_size))
  {
    int marker;

    // PRINTF("checking from offset %u\n", offset);
    marker = scan_marker(buffer, total_size, offset);
    switch (marker)
    {
    case JPEG_MARKER_JFIF:
    case JPEG_MARKER_CMT:
    case JPEG_MARKER_DHT:
    case JPEG_MARKER_H264:
      PRINTF("skipping marker 0x%02x\n", marker);
      skip_marker(buffer, total_size, offset);
      break;
    case JPEG_MARKER_SOF:
      PRINTF("SOF found\n");
      sof_found = true;
      if (!read_sof(&pay, buffer, total_size, offset, info))
        print_error("invalid_format");
      break;
    case JPEG_MARKER_DQT:
      PRINTF("DQT found\n");
      read_quant_table(buffer, total_size, offset, tables);
      dqt_found = true;
      break;
    case JPEG_MARKER_SOS:
      sos_found = true;
      PRINTF("SOS found\n");
      jpeg_header_size = offset;
      /* Do not re-combine into single statement with previous line! */
      jpeg_header_size += read_uint16_t(buffer, total_size, offset);
      break;
    case JPEG_MARKER_EOI:
      fprintf(stderr, "EOI reached before SOS!\n");
      break;
    case JPEG_MARKER_SOI:
      PRINTF("SOI found\n");
      break;
    case JPEG_MARKER_DRI:
      PRINTF("DRI found\n");
      if (read_dri(&pay, buffer, total_size, offset, &restart_marker_header))
        dri_found = true;
      break;
    default:
      if (marker == JPEG_MARKER_JPG || (marker >= JPEG_MARKER_JPG0 && marker <= JPEG_MARKER_JPG13) ||
          (marker >= JPEG_MARKER_APP0 && marker <= JPEG_MARKER_APP15))
      {
        PRINTF("skipping marker\n");
        skip_marker(buffer, total_size, offset);
      }
      else
      {
        /* no need to do anything, scan_marker will go on */
        fprintf(stderr, "unhandled marker 0x%02x\n", marker);
      }
      break;
    }
  }

  if (!dqt_found || !sof_found)
    return print_error("unsupported_jpeg");
  /* by now we should either have negotiated the width/height or the SOF header
   * should have filled us in */
  if (pay.width < 0 || pay.height < 0)
  {
    return print_error("no_dimension");
  }

  PRINTF("header size %u\n", jpeg_header_size);

  offset = 0;

  if (dri_found)
    pay.type += 64;

  quant_data_size = 0;

  if (pay.quant > 127)
  {
    /* for the Y and U component, look up the quant table and its size. quant
     * tables for U and V should be the same */
    for (i = 0; i < 2; i++)
    {
      uint qsize;
      uint qt;

      qt = info[i].qt;
      if (qt >= N_ELEMENTS(tables))
        print_error("invalid_quant");

      qsize = tables[qt].size;
      if (qsize == 0)
        print_error("invalid_quant");

      precision |= (qsize == 64 ? 0 : (1 << i));
      quant_data_size += qsize;
    }
  }

  PRINTF("quant_data size %u\n", quant_data_size);
  quantisation.reserve(quant_data_size);

  /* copy the quant tables for luma and chrominance */
  for (i = 0; i < 2; i++)
  {
    uint qsize;
    uint qt;

    qt = info[i].qt;
    quantisation.insert(quantisation.end(), tables[qt].data, tables[qt].data + tables[qt].size);

    PRINTF("component %d using quant %d, size %d", i, qt, qsize);
  }

  pay.payload = buffer + jpeg_header_size;
  pay.size    = total_size - jpeg_header_size;

  pay.timestamp = timestamp;

  return pay;
}
