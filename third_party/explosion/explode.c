/* This code is placed in the Public Domain by its author, Stuart Caie */

/* macro which obtains the next bit from the input bitstream, from MSB to
 * LSB, and sets the "bit" variable with the result. If 8 bits have been
 * read, fetches another byte from the input bytestream. Equivalent to the
 * following M680x0 code:
 *
 *     add.b   d3,d3
 *     bne.b   gotbit
 *     move.b  -(a3),d3
 *     addx.b  d3,d3
 * gotbit:
 */
#define EXPLODE_GETBIT do {			\
    bit = bit_buffer & 0x80;                    \
    bit_buffer <<= 1;                           \
    if (!bit_buffer) {                          \
      bit2 = bit;                               \
      bit_buffer = *--i;                        \
      bit = bit_buffer & 0x80;                  \
      bit_buffer <<= 1;                         \
      if (bit2) bit_buffer++;                   \
    }                                           \
  } while (0)

static unsigned char explode_literal_base[4] = {
  6, 10, 10, 18
};

static unsigned char explode_literal_extrabits[12] = {
  1, 1, 1, 1,
  2, 3, 3, 4,
  4, 5, 7, 14
};

/**
 * Decompresses a stream of Imploder-compressed data.
 *
 * @param buffer      a buffer that is large enough to contain all the
 *                    decompressed data. On entry, the buffer should
 *                    contain the entire Imploder-compressed stream at
 *                    offset 0. On successful exit, the buffer will
 *                    contain the decompressed data at offset 0. The
 *                    original buffer contents will be overwritten.
 *               
 * @param table       an explosion table, consisting of 8 16-bit
 *                    big-endian "base offset" values and 12 8-bit
 *                    "extra bits" values.
 * @param comp_len    the compressed length of the data
 * @param uncomp_len  the decompressed length of the data
 *
 * @return zero on error, non-zero on success. If successful, the
 * buffer contains the decompressed data.
 */
int explode(unsigned char *buffer,
	    unsigned char *table,
	    unsigned int comp_len,
	    unsigned int uncomp_len)
{
  unsigned char *i  = buffer + comp_len - 5; /* input pointer  */
  unsigned char *o  = buffer + uncomp_len;   /* output pointer */
  unsigned char *match;                      /* match pointer  */
  unsigned char bit_buffer, bit, bit2;
  unsigned int literal_len, match_len, selector, x, y;
  unsigned int match_base[8];

  /* read the 'base' part of the explosion table into native byte order,
   * for speed */
  for (x = 0; x < 8; x++) {
    match_base[x] = (table[x*2] << 8) | table[x*2 + 1];
  }

  /* get initial bit buffer contents, and first literal length */
  if (comp_len & 1) {
    bit_buffer = i[4];
    literal_len = (i[0] << 24) | (i[1] << 16) | (i[2] << 8) | i[3];
  }
  else {
    bit_buffer = i[0];
    literal_len = (i[1] << 24) | (i[2] << 16) | (i[3] << 8) | i[4];
  }

  while (1) {
    /* copy literal run */
    if ((o - buffer) < literal_len) return 0; /* enough space? */
    while (literal_len--) *--o = *--i;

    /* main exit point - after the literal copy */
    if (o <= buffer) break;

    /* static Huffman encoding of the match length and selector: 
     * 
     * 0     -> selector = 0, match_len = 1
     * 10    -> selector = 1, match_len = 2
     * 110   -> selector = 2, match_len = 3
     * 1110  -> selector = 3, match_len = 4
     * 11110 -> selector = 3, match_len = 5 + next three bits (5-12)
     * 11111 -> selector = 3, match_len = (next input byte)-1 (0-254)
     * 
     */
    EXPLODE_GETBIT;
    if (bit) {
      EXPLODE_GETBIT;
      if (bit) {
        EXPLODE_GETBIT;
	if (bit) {
	  selector = 3;
          EXPLODE_GETBIT;
	  if (bit) {
            EXPLODE_GETBIT;
	    if (bit) {
              match_len = *--i;
	      if (match_len == 0) return 0; /* bad input */
	      match_len--;
            }
            else {
              match_len = 0;   EXPLODE_GETBIT; if (bit) match_len++;
              match_len <<= 1; EXPLODE_GETBIT; if (bit) match_len++;
              match_len <<= 1; EXPLODE_GETBIT; if (bit) match_len++;
              match_len += 5;
            }
          }
	  else {
	    match_len = 4;
	  }
        }
	else {
	  selector = 2;
	  match_len = 3;
	}
      }
      else {
	selector = 1;
	match_len = 2;
      }
    }
    else {
      selector = 0;
      match_len = 1;
    }

    /* another Huffman tuple, for deciding the base value (y) and number
     * of extra bits required from the input stream (x) to create the
     * length of the next literal run. Selector is 0-3, as previously
     * obtained.
     *
     * 0  -> base = 0,                      extra = {1,1,1,1}[selector]
     * 10 -> base = 2,                      extra = {2,3,3,4}[selector]
     * 11 -> base = {6,10,10,18}[selector]  extra = {4,5,7,14}[selector]
     */
    y = 0;
    x = selector;
    EXPLODE_GETBIT;
    if (bit) {
      EXPLODE_GETBIT;
      if (bit) {
        y = explode_literal_base[x];
        x += 8;
      }
      else {
        y = 2;
        x += 4;
      }
    }
    x = explode_literal_extrabits[x];

    /* next literal run length: read [x] bits and add [y] */
    literal_len = 0;
    while (x--) {
      literal_len <<= 1;
      EXPLODE_GETBIT;
      if (bit) literal_len++;
    }
    literal_len += y;


    /* another Huffman tuple, for deciding the match distance: _base and
     * _extra are from the explosion table, as passed into the explode
     * function.
     *
     * 0  -> base = 1                        extra = _extra[selector + 0]
     * 10 -> base = 1 + _base[selector + 0]  extra = _extra[selector + 4]
     * 11 -> base = 1 + _base[selector + 4]  extra = _extra[selector + 8]
     */
    match = o + 1;
    x = selector;
    EXPLODE_GETBIT;
    if (bit) {
      EXPLODE_GETBIT;
      if (bit) {
	match += match_base[selector + 4];
	x += 8;
      }
      else {
	match += match_base[selector];
	x += 4;
      }
    }
    x = table[x + 16];

    /* obtain the value of the next [x] extra bits and
     * add it to the match offset */
    y = 0;
    while (x--) {
      y <<= 1; EXPLODE_GETBIT; if (bit) y++;
    }
    match += y;

    /* copy match */
    if ((o - buffer) < match_len) return 0; /* enough space? */
    do { *--o = *--match; } while (match_len--);
  }

  /* return 1 if we used up all input bytes (as we should) */
  return (i == buffer);
}
