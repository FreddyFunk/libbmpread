/******************************************************************************
* libbmpread - tiny, fast bitmap (.bmp) image file loader                     *
*              <https://github.com/chazomaticus/libbmpread>                   *
* Copyright (C) 2005, 2012, 2016 Charles Lindsay <chaz@chazomatic.us>         *
*                                                                             *
*  This software is provided 'as-is', without any express or implied          *
*  warranty.  In no event will the authors be held liable for any damages     *
*  arising from the use of this software.                                     *
*                                                                             *
*  Permission is granted to anyone to use this software for any purpose,      *
*  including commercial applications, and to alter it and redistribute it     *
*  freely, subject to the following restrictions:                             *
*                                                                             *
*  1. The origin of this software must not be misrepresented; you must not    *
*     claim that you wrote the original software. If you use this software    *
*     in a product, an acknowledgment in the product documentation would be   *
*     appreciated but is not required.                                        *
*  2. Altered source versions must be plainly marked as such, and must not be *
*     misrepresented as being the original software.                          *
*  3. This notice may not be removed or altered from any source distribution. *
******************************************************************************/


/* bmpread.c
 * version 2.1
 * 2016-07-18
 */


#include "bmpread.h"

#include <limits.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* If your compiler doesn't come with stdint.h, which is technically a C99
 * feature, see <http://stackoverflow.com/q/126279>.  There are 3rd party
 * solutions to this problem, which you should be able to find with a little
 * searching.  Alternately, just define the following types yourself: uint8_t,
 * uint16_t, uint32_t, and int32_t.
 */
#include <stdint.h>

/* This code makes a number of assumptions about a byte being 8 bits, which is
 * technically not required by the C spec(s).  It's likely that not a whole lot
 * here would need to change if CHAR_BIT != 8, but I haven't taken the time to
 * figure out exactly what those changes would be.
 */
#if CHAR_BIT != 8
#error "libbmpread requires CHAR_BIT == 8"
#endif


/* Default value for alpha when none is present in the file. */
#define BMPREAD_DEFAULT_ALPHA 255

/* I've tried to make every effort to remove the possibility of undefined
 * behavior and prevent related errors where maliciously crafted files could
 * lead to buffer overflows or the like.  To that end, we'll start with some
 * functions that check various operations for behaving as expected.  This one
 * returns nonzero if the two size_ts can be added without wrapping, or 0 if
 * the result would wrap.
 */
static int CanAdd(size_t a, size_t b)
{
    return a <= SIZE_MAX - b;
}

/* Returns nonzero if the two size_ts can be multiplied without wrapping, or 0
 * if the result would wrap.  b must not be 0 (we don't even check here since
 * everything we pass in will have been checked before).
 */
static int CanMultiply(size_t a, size_t b)
{
    return a <= SIZE_MAX / b;
}

/* Returns nonzero if the uint32_t can be converted to a size_t without losing
 * data, which is always the case on 32-bit systems and higher, or 0 if such a
 * conversion would lose data, as could happen on 16-bit systems.
 */
static int CanMakeSizeT(uint32_t x)
{
    /* The preprocessor guard is there to prevent a warning about the condition
     * inside being true by definition on systems where size_t is at least 32
     * bits.  I'm relying on C's integer promotion rules to make this all safe.
     * I *think* it works as intended here (either way, typecasts don't really
     * help clarify things, so I've gone without).
     */
#if UINT32_MAX > SIZE_MAX
    if(x > SIZE_MAX) return 0;
#endif

    (void)x; /* Sometimes unused; this prevents a pedantic warning. */
    return 1;
}

/* Returns nonzero if the uint32_t can be converted to a long without losing
 * data, or 0 if the conversion would lose data.
 */
static int CanMakeLong(uint32_t x)
{
#if UINT32_MAX > LONG_MAX
    if(x > LONG_MAX) return 0;
#endif

    (void)x; /* Sometimes unused. */
    return 1;
}

/* Returns nonzero if the int32_t can be negated properly.  INT32_MIN doesn't
 * work because its positive value isn't representable inside an int32_t (given
 * two's complement).
 */
static int CanNegate(int32_t x)
{
    return x != INT32_MIN;
}

/* Reads up to 4 little-endian bytes from fp and stores the result in the
 * uint32_t pointed to by dest in the host's byte order.  Returns 0 on EOF or
 * nonzero on success.
 */
static int ReadLittleBytes(uint32_t * dest, int bytes, FILE * fp)
{
    uint32_t shift = 0;

    *dest = 0;

    while(bytes--)
    {
        int byte;
        if((byte = fgetc(fp)) == EOF) return 0;

        *dest += (uint32_t)byte << shift;
        shift += 8;
    }

    return 1;
}

/* Reads a little-endian uint32_t from fp and stores the result in *dest in the
 * host's byte order.  Returns 0 on EOF or nonzero on success.
 */
#define ReadLittleUint32(dest, fp) ReadLittleBytes(dest, 4, fp)

/* Reads a little-endian int32_t from fp and stores the result in *dest in the
 * host's byte order.  Returns 0 on EOF or nonzero on success.
 */
static int ReadLittleInt32(int32_t * dest, FILE * fp)
{
    /* I *believe* casting unsigned -> signed is implementation-defined when
     * the unsigned value is out of range for the signed type, which would be
     * the case for any negative number we've just read out of the file into a
     * uint.  This is a portable way to "reinterpret" the bits as signed
     * without running into undefined/implementation-defined behavior.  I
     * think.
     */
    union int32_signedness_swap
    {
        uint32_t uint32;
        int32_t  int32;

    } t;

    if(!ReadLittleBytes(&t.uint32, 4, fp)) return 0;
    *dest = t.int32;
    return 1;
}

/* Reads a little-endian uint16_t from fp and stores the result in *dest in the
 * host's byte order.  Returns 0 on EOF or nonzero n success.
 */
static int ReadLittleUint16(uint16_t * dest, FILE * fp)
{
    uint32_t t;
    if(!ReadLittleBytes(&t, 2, fp)) return 0;
    *dest = (uint16_t)t;
    return 1;
}

/* Reads a uint8_t from fp and stores the result in *dest.  Returns 0 on EOF or
 * nonzero on success.
 */
static int ReadUint8(uint8_t * dest, FILE * fp)
{
    int byte;
    if((byte = fgetc(fp)) == EOF) return 0;
    *dest = (uint8_t)byte;
    return 1;
}

/* Bitmap file header, including magic bytes.
 */
typedef struct bmp_header
{
    uint8_t  magic[2];    /* Magic bytes 'B' and 'M'. */
    uint32_t file_size;   /* Size of whole file. */
    uint32_t unused;      /* Should be 0. */
    uint32_t data_offset; /* Offset from beginning of file to bitmap data. */

} bmp_header;

/* Reads a bitmap header from fp into header.  Returns 0 on EOF or nonzero on
 * success.
 */
static int ReadHeader(bmp_header * header, FILE * fp)
{
    if(!ReadUint8(&header->magic[0], fp)) return 0;
    if(!ReadUint8(&header->magic[1], fp)) return 0;

    /* If it doesn't look like a bitmap header, don't even bother. */
    if(header->magic[0] != 0x42 /* 'B' */) return 0;
    if(header->magic[1] != 0x4d /* 'M' */) return 0;

    if(!ReadLittleUint32(&header->file_size,   fp)) return 0;
    if(!ReadLittleUint32(&header->unused,      fp)) return 0;
    if(!ReadLittleUint32(&header->data_offset, fp)) return 0;

    return 1;
}

/* How many bytes in the file are occupied by a header, by definition in the
 * spec.  Note that even though our definition logically matches the spec's, C
 * struct padding/packing rules mean it might not be the same as
 * sizeof(bmp_header).
 */
#define BMP_HEADER_SIZE 14

/* Bitmap info: comes immediately after the header and describes the image.
 */
typedef struct bmp_info
{
    uint32_t info_size;   /* Size of info struct (> sizeof(bmp_info)). */
    int32_t  width;       /* Width of image. */
    int32_t  height;      /* Height (< 0 means right-side up). */
    uint16_t planes;      /* Planes (should be 1). */
    uint16_t bits;        /* Number of bits (1, 4, 8, 16, 24, or 32). */
    uint32_t compression; /* See COMPRESSION_* values below. */
    uint32_t unused[5];   /* We don't care about these fields. */
    uint32_t masks[4];    /* Bitmasks for 16- and 32-bit images. */

    /* There can be additional later fields in the actual file info, but we
     * don't need them here.
     */

} bmp_info;

/* We don't support files in bitmap formats older than Windows 3/NT.  info_size
 * of 40 is defined by the spec as Windows 3/NT format, getting larger in later
 * incarnations.
 */
#define BMP3_INFO_SIZE 40
#define MIN_INFO_SIZE BMP3_INFO_SIZE

/* Values for the compression field.  We only support COMPRESSION_NONE and
 * COMPRESSION_BITFIELDS so far.
 */
#define COMPRESSION_NONE      0
#define COMPRESSION_RLE8      1
#define COMPRESSION_RLE4      2
#define COMPRESSION_BITFIELDS 3

/* Reads bitmap metadata from fp into info.  Returns 0 on EOF or nonzero on
 * success.
 */
static int ReadInfo(bmp_info * info, FILE * fp)
{
    if(!ReadLittleUint32(&info->info_size, fp)) return 0;

    /* Older formats might not have all the fields we require, so this check
     * comes first.
     */
    if(info->info_size < MIN_INFO_SIZE) return 0;

    if(!ReadLittleInt32( &info->width,       fp)) return 0;
    if(!ReadLittleInt32( &info->height,      fp)) return 0;
    if(!ReadLittleUint16(&info->planes,      fp)) return 0;
    if(!ReadLittleUint16(&info->bits,        fp)) return 0;
    if(!ReadLittleUint32(&info->compression, fp)) return 0;
    if(!ReadLittleUint32(&info->unused[0],   fp)) return 0;
    if(!ReadLittleUint32(&info->unused[1],   fp)) return 0;
    if(!ReadLittleUint32(&info->unused[2],   fp)) return 0;
    if(!ReadLittleUint32(&info->unused[3],   fp)) return 0;
    if(!ReadLittleUint32(&info->unused[4],   fp)) return 0;

    /* We don't bother to even try to read bitmasks if they aren't needed,
     * since they won't be present in Windows 3 format bitmap files.
     */
    if(info->compression == COMPRESSION_BITFIELDS)
    {
        /* We're guaranteed at least the R, G, and B bitfields... */
        if(!ReadLittleUint32(&info->masks[0], fp)) return 0;
        if(!ReadLittleUint32(&info->masks[1], fp)) return 0;
        if(!ReadLittleUint32(&info->masks[2], fp)) return 0;

        /* ...However, only bitmap versions *after* Windows 3/NT have an alpha
         * bitmask.
         */
        if(info->info_size > BMP3_INFO_SIZE)
        {
            if(!ReadLittleUint32(&info->masks[3], fp)) return 0;
        }
    }

    return 1;
}

/* A single color entry in the palette, in file order (BGR + one unused byte).
 */
typedef struct bmp_color
{
    uint8_t blue;
    uint8_t green;
    uint8_t red;
    uint8_t unused;

} bmp_color;

/* How many bytes in the file are occupied by a palette entry, by definition in
 * the spec (and again note that it might not be the same as
 * sizeof(bmp_color), even if we match).
 */
#define BMP_COLOR_SIZE 4

/* Reads the given number of colors from fp into the palette array.  Returns 0
 * on EOF or nonzero on success.
 */
static int ReadPalette(bmp_color * palette, size_t colors, FILE * fp)
{
    /* This isn't the guaranteed-fastest way to implement this, but it should
     * perform quite well in practice due to compiler optimization and stdio
     * input buffering.  It's implemented this way because of how simple the
     * code is, while avoiding undefined and implementation-defined behavior or
     * allocating any memory.  If you aren't averse to an extra allocation (or
     * using a chunk of the stack), it might be made faster while still
     * avoiding implementation-defined behavior by reading the entire palette
     * into one big buffer up front, then copying bytes into place.
     */
    size_t i;
    for(i = 0; i < colors; i++)
    {
        uint8_t components[BMP_COLOR_SIZE];
        if(fread(components, 1, sizeof(components), fp) != sizeof(components))
            return 0;

        palette[i].blue   = components[0];
        palette[i].green  = components[1];
        palette[i].red    = components[2];
        palette[i].unused = components[3];
    }
    return 1;
}

/* This struct holds information for a bit field. A bit field is used when the
 * compression value of a bitmap is 3. Only bitmaps with a bit count of 16 or
 * 32 can have a compression of 3 and therefor use a bit field.
 */
typedef struct bit_field_info {
    uint8_t  bit_shift;        /* The shift to apply to get the value. */
    uint8_t  bit_count;        /* The amount of bits that are set. */
    uint32_t bit_mask;         /* The mask of the used bits. */
    float    value_multiplier; /* A multiplier to normalize values. */

} bit_field_info;

#define BMP_BIT_FIELD_SIZE 4

/* Creates the bit_field_info(bit_field) based on the bit_mask(bit_field_mask).
 * Returns 0 on failure or nonzero on success.
 */
static int CreateBitField(bit_field_info * bit_field, uint32_t bit_field_mask)
{
    uint8_t bit_nr;

    if(!bit_field) /* Idiot test for my self. */
        return 0;

    bit_field->bit_count = 0;
    bit_field->bit_mask = bit_field_mask;

    if(bit_field_mask == 0)
    {
        bit_field->bit_shift = 0;
        /* bit_field->bit_mask is already 0. */
        bit_field->value_multiplier = 0;
        return 1;
    }

    bit_field->bit_shift = 0xff; /* Invalid/impossible value. */
    for(bit_nr = 0; bit_nr < BMP_BIT_FIELD_SIZE * 8; bit_nr++)
    {
        uint8_t bit_value = (bit_field_mask >> bit_nr) & 0x01;

        if(bit_value == 1)
        {
            if(bit_field->bit_shift == 0xff)
                bit_field->bit_shift = bit_nr;

            /* The bit_nr is equal to (mask_bit_count + bit_shift) if the bit
             * is contiguous.
             */
            if(bit_nr != (bit_field->bit_count + bit_field->bit_shift))
                return 0;

            bit_field->bit_count++;
        }
    }

    if(bit_field->bit_count == 0 || bit_field->bit_shift == 0xff)
        return 0;

    if(bit_field->bit_count > 31)
        return 0; /* This should never happen... so the question is: how?!? */

    /* -1 because the value space start at 0. */
    bit_field->value_multiplier =
            (float)0xff / (float)((0x01 << bit_field->bit_count) - 1);

    return 1;
}

/* Returns whether a non-negative integer is a power of 2.
 */
static int IsPowerOf2(uint32_t x)
{
    while(x)
    {
        /* When we find a bit, return whether no other bits are set. */
        if(x & 1)
            return !(x & ~UINT32_C(1));
        x = x >> 1;
    }

    /* 0, the only value for x which lands us here, isn't a power of 2. */
    return 0;
}

/* Returns the byte length of a scan line padded as necessary to be divisible
 * by four.  For example, 3 pixels wide at 24 bpp would yield 12 (3 pixels * 3
 * bytes each = 9 bytes, padded by 3 to the next multiple of 4).  bpp is *bits*
 * per pixel, not bytes.  Returns 0 in case of overflow.
 */
static size_t GetLineLength(size_t width, size_t bpp)
{
    size_t bits;     /* Number of bits in a line. */
    size_t pad_bits; /* Number of padding bits to make bits divisible by 32. */

    bits = width * bpp;
    pad_bits = (32 - (bits & 0x1f)) & 0x1f; /* x & 0x1f == x % 32 */

    /* Check for overflow, in both the above multiplication and the below
     * addition.  It's well defined to do this in any order relative to the
     * operations themselves (since size_t is unsigned), so we combine the
     * checks into one if.  bpp has been checked for being nonzero elsewhere.
     */
    if(!CanMultiply(width, bpp) || !CanAdd(bits, pad_bits)) return 0;

    /* Convert to bytes. */
    return (bits + pad_bits) / 8;
}

/* Context shared between the below functions.
 */
typedef struct read_context
{
    unsigned int        flags;             /* Flags passed to bmpread. */
    FILE              * fp;                /* File pointer. */
    bmp_header          header;            /* File header. */
    bmp_info            info;              /* File info. */
    int32_t             lines;         /* How many scan lines (abs(height)). */
    size_t              file_line_len;  /* How many bytes each scan line is. */
    size_t              out_channel_count;/* Color channels (3, or 4=alpha). */
    size_t              out_line_len;      /* Bytes in each output line. */
    bmp_color         * palette;        /* Enough entries for our bit depth. */
    bit_field_info      bit_fields[4];     /* How to decode 16- and 32-bits. */
    uint8_t           * file_data;         /* A line of data in the file. */
    uint8_t           * data_out;          /* RGB(A) data output buffer. */

} read_context;

/* Reads and validates the bitmap header metadata from the context's file
 * object.  Assumes the file pointer is at the start of the file.  Returns 1 if
 * ok or 0 if error or invalid file.
 */
static int Validate(read_context * p_ctx)
{
    int is_supported;

    if(!ReadHeader(&p_ctx->header, p_ctx->fp)) return 0;
    if(!ReadInfo(  &p_ctx->info,   p_ctx->fp)) return 0;

    if(p_ctx->info.width <= 0 || p_ctx->info.height == 0) return 0;

    is_supported = 0;
    switch(p_ctx->info.compression)
    {
        case COMPRESSION_NONE:
            if(p_ctx->info.bits == 1 || p_ctx->info.bits == 4 ||
               p_ctx->info.bits == 8 || p_ctx->info.bits == 24)
                is_supported = 1;
            break;
        case COMPRESSION_BITFIELDS:
            if(p_ctx->info.bits == 16 || p_ctx->info.bits == 32)
                is_supported = 1;
            break;
        default: /* No compression supported yet (TODO: handle RLE). */
            is_supported = 0;
            break;
    }
    if(!is_supported) return 0;

    if(!CanMakeSizeT(p_ctx->info.width)) return 0;
    p_ctx->file_line_len = GetLineLength(p_ctx->info.width,
                                         p_ctx->info.bits);
    if(p_ctx->file_line_len == 0) return 0;

    p_ctx->out_channel_count = ((p_ctx->flags & BMPREAD_ALPHA) ? 4 : 3);
    /* This check happens outside the following if, where it would seem to
     * belong, because we make the same computation again in the future.
     */
    if(!CanMultiply(p_ctx->info.width, p_ctx->out_channel_count)) return 0;

    /* This might be unnecessary when out_channel_count is 4.
     * TODO: think about it.
     */
    if(p_ctx->flags & BMPREAD_BYTE_ALIGN)
    {
        p_ctx->out_line_len =
                (size_t)p_ctx->info.width * p_ctx->out_channel_count;
    }
    else
    {
        p_ctx->out_line_len = GetLineLength(p_ctx->info.width,
                                            p_ctx->out_channel_count * 8);
        if(p_ctx->out_line_len == 0) return 0;
    }

    if(!CanNegate(p_ctx->info.height)) return 0;
    p_ctx->lines = ((p_ctx->info.height < 0) ?
                    -p_ctx->info.height :
                     p_ctx->info.height);

    if(!(p_ctx->flags & BMPREAD_ANY_SIZE))
    {
        /* Both of these values have been checked against being negative
         * above, and thus it's safe to pass them on as uint32_t.
         */
        if(!IsPowerOf2(p_ctx->info.width)) return 0;
        if(!IsPowerOf2(p_ctx->lines))      return 0;
    }

    /* Handle palettes.
    * TODO: check against ColorsUsed (handling 0) and the size of the space
    * between the bitmap header and start of color data.
     */
    if(p_ctx->info.bits <= 8)
    {
        /* I believe C mandates that SIZE_MAX is at least 65535, so this
         * expression and the next are always safe.
         */
        size_t colors = (size_t)1 << p_ctx->info.bits;
        if(!(p_ctx->palette = (bmp_color *)
             malloc(colors * BMP_COLOR_SIZE))) return 0;

        if(!CanMakeLong(p_ctx->info.info_size)) return 0;
#if UINT32_MAX > LONG_MAX
        if((long)p_ctx->info.info_size > LONG_MAX - BMP_HEADER_SIZE) return 0;
#endif
        if(fseek(p_ctx->fp,
                 BMP_HEADER_SIZE + (long)p_ctx->info.info_size,
                 SEEK_SET))                                 return 0;
        if(!ReadPalette(p_ctx->palette, colors, p_ctx->fp)) return 0;
    }
    /* Loading bit field info for decoding. */
    if(p_ctx->info.compression == 3)
    {
        uint8_t  success;
        uint8_t  channel_nr;
        uint32_t bit_mask;
        uint8_t  total_bit_count;
        uint32_t total_bit_mask;

        total_bit_count = 0;
        total_bit_mask = 0;
        success = 1;
        for(channel_nr = 0; channel_nr < 4; channel_nr++)
        {
            bit_mask = p_ctx->info.masks[channel_nr];

            /* Overlapping bit masks are invalid. */
            if((total_bit_mask & bit_mask) ||
               !CreateBitField(&p_ctx->bit_fields[channel_nr], bit_mask))
            {
                success = 0;
                break;
            }

            total_bit_mask  |= bit_mask;
            total_bit_count += p_ctx->bit_fields[channel_nr].bit_count;
        }
        if(!success || total_bit_count > p_ctx->info.bits)
            return 0;
    }

    /* Set things up for decoding. */
    if(!(p_ctx->file_data = (uint8_t *)
         malloc(p_ctx->file_line_len))) return 0;

    if(!CanMakeSizeT(p_ctx->lines))                           return 0;
    if(!CanMultiply( p_ctx->lines, p_ctx->out_line_len))      return 0;
    if(!(p_ctx->data_out = (uint8_t *)
         malloc((size_t)p_ctx->lines * p_ctx->out_line_len))) return 0;

    return 1;
}

/* Decodes 32-bit bitmap data.  Takes a pointer to an output buffer scan line
 * (p_out), a pointer to the end of the PIXEL DATA of this scan line
 * (p_out_end), a pointer to the source scan line of file data (p_file), and a
 * array of bit fields to decode the data.
 */
static void Decode32(uint8_t * p_out,
                     const uint8_t * p_out_end,
                     const uint8_t * p_file,
                     const read_context * p_ctx)
{
    uint32_t * value;

    while(p_out < p_out_end)
    {
        value = (uint32_t*)p_file;

        *p_out++ = (uint8_t)(
                             ((*value & p_ctx->bit_fields[0].bit_mask)
                                     >> p_ctx->bit_fields[0].bit_shift)
                             * p_ctx->bit_fields[0].value_multiplier
                            );
        *p_out++ = (uint8_t)(
                             ((*value & p_ctx->bit_fields[1].bit_mask)
                                     >> p_ctx->bit_fields[1].bit_shift)
                             * p_ctx->bit_fields[1].value_multiplier
                            );
        *p_out++ = (uint8_t)(
                             ((*value & p_ctx->bit_fields[2].bit_mask)
                                     >> p_ctx->bit_fields[2].bit_shift)
                             * p_ctx->bit_fields[2].value_multiplier
                            );

        if(p_ctx->out_channel_count == 4)
        {
            /* Alpha. */
            if(p_ctx->bit_fields[3].bit_mask)
            {
                *p_out++ = (uint8_t)(
                                     ((*value & p_ctx->bit_fields[3].bit_mask)
                                             >> p_ctx->bit_fields[3].bit_shift)
                                     * p_ctx->bit_fields[3].value_multiplier
                                    );
            }
            else
                *p_out++ = BMPREAD_DEFAULT_ALPHA;
        }

        p_file += 4;
    }
}

/* Decodes 24-bit bitmap data.  Takes a pointer to an output buffer scan line
 * (p_out), a pointer to the end of the PIXEL DATA of this scan line
 * (p_out_end), a pointer to the source scan line of file data (p_file), and a
 * pointer to a palette (not used in this case).
 */
static void Decode24(uint8_t * p_out,
                     const uint8_t * p_out_end,
                     const uint8_t * p_file,
                     const read_context * p_ctx)
{
    while(p_out < p_out_end)
    {
        *p_out++ = *(p_file + 2);
        *p_out++ = *(p_file + 1);
        *p_out++ = *(p_file    );

        if(p_ctx->out_channel_count == 4)
            *p_out++ = BMPREAD_DEFAULT_ALPHA;

        p_file += 3;
    }

    (void)p_ctx; /* Unused. */
}

/* Decodes 16-bit bitmap data.  Takes a pointer to an output buffer scan line
* (p_out), a pointer to the end of the PIXEL DATA of this scan line
* (p_out_end), a pointer to the source scan line of file data (p_file), and a
*  array of bit fields to decode the data.
*/
static void Decode16(uint8_t * p_out,
                     const uint8_t * p_out_end,
                     const uint8_t * p_file,
                     const read_context * p_ctx)
{
    uint32_t * value;
    while(p_out < p_out_end)
    {
        value = (uint32_t*)p_file;

        *p_out++ = (uint8_t)(
                             ((*value & p_ctx->bit_fields[0].bit_mask)
                                     >> p_ctx->bit_fields[0].bit_shift)
                             * p_ctx->bit_fields[0].value_multiplier
                            );
        *p_out++ = (uint8_t)(
                             ((*value & p_ctx->bit_fields[1].bit_mask)
                                     >> p_ctx->bit_fields[1].bit_shift)
                             * p_ctx->bit_fields[1].value_multiplier
                            );
        *p_out++ = (uint8_t)(
                             ((*value & p_ctx->bit_fields[2].bit_mask)
                                     >> p_ctx->bit_fields[2].bit_shift)
                             * p_ctx->bit_fields[2].value_multiplier
                            );

        if(p_ctx->out_channel_count == 4)
        {
            /* Alpha. */
            if(p_ctx->bit_fields[3].bit_mask)
            {
                *p_out++ = (uint8_t)(
                                     ((*value & p_ctx->bit_fields[3].bit_mask)
                                             >> p_ctx->bit_fields[3].bit_shift)
                                     * p_ctx->bit_fields[3].value_multiplier
                                    );
            }
            else
                *p_out++ = BMPREAD_DEFAULT_ALPHA;
        }

        p_file += 2;
    }
}

/* Same as Decode24, but for 8 bit palette data.
 */
static void Decode8(uint8_t * p_out,
                    const uint8_t * p_out_end,
                    const uint8_t * p_file,
                    const read_context * p_ctx)
{
    while(p_out < p_out_end) {
        *p_out++ = p_ctx->palette[*p_file].red;
        *p_out++ = p_ctx->palette[*p_file].green;
        *p_out++ = p_ctx->palette[*p_file].blue;

        if(p_ctx->out_channel_count == 4)
            *p_out++ = BMPREAD_DEFAULT_ALPHA;

        p_file++;
    }
}

/* Same as Decode24, but for 4 bit palette data.
 */
static void Decode4(uint8_t * p_out,
                    const uint8_t * p_out_end,
                    const uint8_t * p_file,
                    const read_context * p_ctx)
{
    unsigned int lookup;

    while(p_out < p_out_end)
    {
        lookup = (*p_file & 0xf0U) >> 4;

        *p_out++ = p_ctx->palette[lookup].red;
        *p_out++ = p_ctx->palette[lookup].green;
        *p_out++ = p_ctx->palette[lookup].blue;

        if(p_ctx->out_channel_count == 4)
            *p_out++ = BMPREAD_DEFAULT_ALPHA;

        if(p_out < p_out_end)
        {
            lookup = *p_file++ & 0x0fU;

            *p_out++ = p_ctx->palette[lookup].red;
            *p_out++ = p_ctx->palette[lookup].green;
            *p_out++ = p_ctx->palette[lookup].blue;

            if(p_ctx->out_channel_count == 4)
                *p_out++ = BMPREAD_DEFAULT_ALPHA;
        }
    }
}

/* Same as Decode24, but for monochrome palette data.
 */
static void Decode1(uint8_t * p_out,
                    const uint8_t * p_out_end,
                    const uint8_t * p_file,
                    const read_context * p_ctx)
{
    unsigned int bit;
    unsigned int lookup;

    while(p_out < p_out_end)
    {
        for(bit = 0; bit < 8 && p_out < p_out_end; bit++)
        {
            lookup = (*p_file >> (7 - bit)) & 1;

            *p_out++ = p_ctx->palette[lookup].red;
            *p_out++ = p_ctx->palette[lookup].green;
            *p_out++ = p_ctx->palette[lookup].blue;

            if(p_ctx->out_channel_count == 4)
                *p_out++ = BMPREAD_DEFAULT_ALPHA;
        }

        p_file++;
    }
}

/* Selects an above decoder and runs it for each scan line of the file.
 * Returns 0 if there's an error or 1 if it's gravy.
 */
static int Decode(read_context * p_ctx)
{
    void (* decoder)(uint8_t *, const uint8_t *, const uint8_t *,
                     const read_context *);

    uint8_t * p_out;      /* Pointer to current scan line in output buffer. */
    uint8_t * p_out_end;  /* End marker for output buffer. */
    uint8_t * p_line_end; /* Pointer to end of current scan line in output. */

    /* out_inc is an incrementor for p_out to advance it one scan line.  I'm
     * not exactly sure what the correct type for it would be, perhaps ssize_t,
     * but that's not C standard.  I went with ptrdiff_t because its value
     * will be equivalent to the difference between two pointers, whether it
     * was derived that way or not.
     */
    ptrdiff_t out_inc;

    /* Double check this won't overflow.  Who knows, man. */
#if SIZE_MAX > PTRDIFF_MAX
    if(p_ctx->out_line_len > PTRDIFF_MAX) return 0;
#endif
    out_inc = p_ctx->out_line_len;

    if(!(p_ctx->info.height < 0) == !(p_ctx->flags & BMPREAD_TOP_DOWN))
    {
        /* We're keeping scan lines in order.  These and subsequent operations
         * have all been checked earlier.
         */
        p_out     = p_ctx->data_out;
        p_out_end = p_ctx->data_out +
                    ((size_t)p_ctx->lines * p_ctx->out_line_len);
    }
    else /* We're reversing scan lines. */
    {
        /* TODO: I'm not 100% sure about the legality, purely C spec-wise, of
         * this subtraction.
         */
        p_out_end = p_ctx->data_out - p_ctx->out_line_len;
        p_out     = p_ctx->data_out +
                    (((size_t)p_ctx->lines - 1) * p_ctx->out_line_len);

        /* Always safe, given two's complement, since it was positive. */
        out_inc = -out_inc;
    }

    p_line_end = p_out + (size_t)p_ctx->info.width * p_ctx->out_channel_count;

    switch(p_ctx->info.bits)
    {
        case 32: decoder = Decode32; break;
        case 24: decoder = Decode24; break;
        case 16: decoder = Decode16; break;
        case 8:  decoder = Decode8;  break;
        case 4:  decoder = Decode4;  break;
        case 1:  decoder = Decode1;  break;
        default: return 0;
    }

    if(!CanMakeLong(p_ctx->header.data_offset))               return 0;
    if(fseek(p_ctx->fp, p_ctx->header.data_offset, SEEK_SET)) return 0;

    while(p_out != p_out_end &&
          fread(p_ctx->file_data, 1, p_ctx->file_line_len, p_ctx->fp) ==
          p_ctx->file_line_len)
    {
        decoder(p_out, p_line_end, p_ctx->file_data, p_ctx);

        p_out      += out_inc;
        p_line_end += out_inc;
    }

    return (p_out == p_out_end);
}

/* Frees resources allocated by various functions along the way.  Only frees
 * data_out if !leave_data_out (if the bitmap loads successfully, you want the
 * data to remain until THEY free it).
 */
static void FreeContext(read_context * p_ctx, int leave_data_out)
{
    if(p_ctx->fp)
        fclose(p_ctx->fp);
    if(p_ctx->palette)
        free(p_ctx->palette);
    if(p_ctx->file_data)
        free(p_ctx->file_data);

    if(!leave_data_out && p_ctx->data_out)
        free(p_ctx->data_out);
}

int bmpread(const char * bmp_file, unsigned int flags, bmpread_t * p_bmp_out)
{
    int success = 0;

    read_context ctx;
    memset(&ctx, 0, sizeof(read_context));

    do
    {
        if(!bmp_file)  break;
        if(!p_bmp_out) break;
        memset(p_bmp_out, 0, sizeof(bmpread_t));

        ctx.flags = flags;

        if(!(ctx.fp = fopen(bmp_file, "rb"))) break;
        if(!Validate(&ctx))                   break;
        if(!Decode(&ctx))                     break;

        /* Finally, make sure we can stuff these into ints.  I feel like this
         * is slightly justified by how it keeps the header definition dead
         * simple (including, well, no #includes).  I suppose this could also
         * be done way earlier and maybe save some disk reads, but I like
         * keeping the check with the code it's checking.
         */
#if INT32_MAX > INT_MAX
        if(ctx.info.width > INT_MAX) break;
        if(ctx.lines      > INT_MAX) break;
#endif

        p_bmp_out->width  = ctx.info.width;
        p_bmp_out->height = ctx.lines;
        p_bmp_out->flags  = ctx.flags;
        p_bmp_out->data   = ctx.data_out;

        success = 1;
    } while(0);

    FreeContext(&ctx, success);

    return success;
}

void bmpread_free(bmpread_t * p_bmp)
{
    if(p_bmp)
    {
        if(p_bmp->data)
            free(p_bmp->data);

        memset(p_bmp, 0, sizeof(bmpread_t));
    }
}
