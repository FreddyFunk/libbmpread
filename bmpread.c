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
 * version 2.0
 * 2016-04-24
 */


#include "bmpread.h"

#include <limits.h>
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


/* Reads up to 4 little-endian bytes from fp and stores the result in the
 * uint32_t pointed to by dest in the host's byte order.  Returns 0 on EOF or
 * nonzero on success.
 */
static int _bmp_ReadLittleBytes(uint32_t * dest, int bytes, FILE * fp)
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
#define _bmp_ReadLittleUint32(dest, fp) _bmp_ReadLittleBytes(dest, 4, fp)

/* Reads a little-endian int32_t from fp and stores the result in *dest in the
 * host's byte order.  Returns 0 on EOF or nonzero on success.
 */
static int _bmp_ReadLittleInt32(int32_t * dest, FILE * fp)
{
    /* I *believe* casting unsigned -> signed is implementation-defined when
     * the unsigned value is out of range for the signed type, which would be
     * the case for any negative number we've just read out of the file into a
     * uint.  This is a portable way to "reinterpret" the bits as signed
     * without running into undefined/implementation-defined behavior.  I
     * think.
     */
    union _bmp_int32_signedness_swap
    {
        uint32_t uint32;
        int32_t  int32;

    } t;

    if(!_bmp_ReadLittleBytes(&t.uint32, 4, fp)) return 0;
    *dest = t.int32;
    return 1;
}

/* Reads a little-endian uint16_t from fp and stores the result in *dest in the
 * host's byte order.  Returns 0 on EOF or nonzero n success.
 */
static int _bmp_ReadLittleUint16(uint16_t * dest, FILE * fp)
{
    uint32_t t;
    if(!_bmp_ReadLittleBytes(&t, 2, fp)) return 0;
    *dest = (uint16_t)t;
    return 1;
}

/* Reads a uint8_t from fp and stores the result in *dest.  Returns 0 on EOF or
 * nonzero on success.
 */
static int _bmp_ReadUint8(uint8_t * dest, FILE * fp)
{
    int byte;
    if((byte = fgetc(fp)) == EOF) return 0;
    *dest = (uint8_t)byte;
    return 1;
}

/* Bitmap file header, including magic bytes.
 */
typedef struct _bmp_header
{
    uint8_t  magic[2];    /* magic bytes 'B' and 'M' */
    uint32_t file_size;   /* size of whole file */
    uint32_t unused;      /* should be 0 */
    uint32_t data_offset; /* offset from beginning of file to bitmap data */

} _bmp_header;

/* Reads a bitmap header from fp into header.  Returns 0 on EOF or nonzero on
 * success.
 */
static int _bmp_ReadHeader(_bmp_header * header, FILE * fp)
{
    if(!_bmp_ReadUint8(       &header->magic[0],    fp)) return 0;
    if(!_bmp_ReadUint8(       &header->magic[1],    fp)) return 0;
    if(!_bmp_ReadLittleUint32(&header->file_size,   fp)) return 0;
    if(!_bmp_ReadLittleUint32(&header->unused,      fp)) return 0;
    if(!_bmp_ReadLittleUint32(&header->data_offset, fp)) return 0;
    return 1;
}

/* Bitmap info struct: comes immediately after header and describes the image.
 */
typedef struct _bmp_info
{
    uint32_t info_size;   /* size of info struct (is >sizeof(_bmp_info)) */
    int32_t  width;       /* width of image */
    int32_t  height;      /* height (< 0 means right-side up) */
    uint16_t planes;      /* planes (should be 1) */
    uint16_t bits;        /* number of bits (1, 4, 8, 16, 24, or 32) */
    uint32_t compression; /* 0 = none, 1 = 8-bit RLE, 2 = 4-bit RLE */

    /* There are other fields in the actual file info, but we don't need 'em.
     */

} _bmp_info;

/* Reads bitmap metadata from fp into info.  Returns 0 on EOF or nonzero on
 * success.
 */
static int _bmp_ReadInfo(_bmp_info * info, FILE * fp)
{
    if(!_bmp_ReadLittleUint32(&info->info_size,   fp)) return 0;
    if(!_bmp_ReadLittleInt32( &info->width,       fp)) return 0;
    if(!_bmp_ReadLittleInt32( &info->height,      fp)) return 0;
    if(!_bmp_ReadLittleUint16(&info->planes,      fp)) return 0;
    if(!_bmp_ReadLittleUint16(&info->bits,        fp)) return 0;
    if(!_bmp_ReadLittleUint32(&info->compression, fp)) return 0;
    return 1;
}

/* A single color entry in the palette, in file order (BGR + one unused byte).
 */
typedef struct _bmp_palette_entry
{
    uint8_t blue;
    uint8_t green;
    uint8_t red;
    uint8_t unused;

} _bmp_palette_entry;

/* Reads the given number of colors from fp into the palette array.  Returns 0
 * on EOF or nonzero on success.
 */
static int _bmp_ReadPalette(_bmp_palette_entry * palette, size_t colors,
                            FILE * fp)
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
        uint8_t components[4];
        if(fread(components, 1, sizeof(components), fp) != sizeof(components))
            return 0;

        palette[i].blue   = components[0];
        palette[i].green  = components[1];
        palette[i].red    = components[2];
        palette[i].unused = components[3];
    }
    return 1;
}

/* Context shared between various functions in this library.
 */
typedef struct _bmp_read_context
{
    unsigned int         flags;         /* flags passed to bmpread */
    FILE               * fp;            /* file pointer */
    _bmp_header          header;        /* file header */
    _bmp_info            info;          /* file info */
    int32_t              lines;         /* how many scan lines (abs(height)) */
    size_t               file_line_len; /* how many bytes each scan line is */
    size_t               rgb_line_len;  /* bytes in each output line */
    _bmp_palette_entry * palette;       /* palette */
    uint8_t            * file_data;     /* a line of data in the file */
    uint8_t            * rgb_data;      /* rgb data output buffer */

} _bmp_read_context;

/* Returns whether an integer is a power of 2.
 */
static int _bmp_IsPowerOf2(uint32_t x)
{
    uint32_t bit;

    for(bit = 1; bit; bit <<= 1)
    {
        /* When we find a bit, return whether no other bits are set: */
        if(x & bit)
            return !(x & ~bit);
    }

    /* 0, the only value for x which lands us here, isn't a power of 2. */
    return 0;
}

/* Returns the DWORD-aligned byte-length of a scan line.  For instance, for
 * 24-bit data 3 pixels wide, it would return 12 (3 pixels * 3 bytes each = 9
 * bytes, then padded to the next DWORD).  bpp is BITS per pixel, not bytes.
 */
static size_t _bmp_GetLineLength(size_t width, size_t bpp)
{
    size_t bits;     /* number of bits in a line */
    size_t pad_bits; /* number of padding bits to make bits divisible by 32 */

    bits = width * bpp;
    pad_bits = (32 - (bits & 0x1f)) & 0x1f; /* x & 0x1f == x % 32 */

    /* Check for overflow, in both the above multiplication and the below
     * addition.  It's well defined to do this in any order relative to the
     * operations themselves, so we combine the checks into one if.  bpp has
     * been checked for being nonzero elsewhere, so it's safe to divide by.
     */
    if(width > SIZE_MAX / bpp || SIZE_MAX - bits < pad_bits)
        return 0;

    return (bits + pad_bits) / 8; /* convert to bytes */
}

/* Reads and validates the bitmap header metadata from the context's file
 * object.  Assumes the file pointer is at the start of the file.  Returns 1 if
 * ok or 0 if error or invalid file.
 */
static int _bmp_Validate(_bmp_read_context * p_ctx)
{
    int success = 0;

    do
    {
        if(!_bmp_ReadHeader(&p_ctx->header, p_ctx->fp)) break;
        if(!_bmp_ReadInfo(  &p_ctx->info,   p_ctx->fp)) break;

        /* Some basic validation: */
        if(p_ctx->header.magic[0] != 0x42 /* 'B' */) break;
        if(p_ctx->header.magic[1] != 0x4d /* 'M' */) break;

        /* The INT32_MIN check is necessary to avoid undefined behavior when
         * negating height below.  I'm a bit murky on the spec here, but this
         * may assume two's complement integer storage, which I believe isn't
         * mandated.
         * FIXME: is there a more portable overflow check?
         */
        if(p_ctx->info.width <= 0 ||
           p_ctx->info.height == 0 || p_ctx->info.height == INT32_MIN) break;

        /* No compression supported yet (TODO: handle RLE). */
        if(p_ctx->info.compression > 0)                                break;

        if(p_ctx->info.bits != 1 && p_ctx->info.bits != 4 &&
           p_ctx->info.bits != 8 && p_ctx->info.bits != 24)            break;

        p_ctx->lines = ((p_ctx->info.height < 0) ?
                        -p_ctx->info.height :
                         p_ctx->info.height);

        p_ctx->file_line_len = _bmp_GetLineLength(p_ctx->info.width,
                                                  p_ctx->info.bits);
        if(p_ctx->file_line_len == 0) break;


        /* FIXME: ensure that converting int32_t => size_t here won't overflow,
         * which could be the case for 16-bit platforms.
         */
        p_ctx->rgb_line_len = ((p_ctx->flags & BMPREAD_BYTE_ALIGN) ?
                               (size_t)p_ctx->info.width * 3 :
                               _bmp_GetLineLength(p_ctx->info.width, 24));
        if(p_ctx->rgb_line_len == 0)  break;

        if(!(p_ctx->flags & BMPREAD_ANY_SIZE))
        {
            /* Both of these values have been checked against being negative
             * above, and thus it's safe to pass them on as uint32_t.
             */
            if(!_bmp_IsPowerOf2(p_ctx->info.width)) break;
            if(!_bmp_IsPowerOf2(p_ctx->lines))      break;
        }

        /* Handle palettes: */
        if(p_ctx->info.bits <= 8)
        {
            size_t colors = 1 << (size_t)p_ctx->info.bits;

            if(!(p_ctx->palette = (_bmp_palette_entry *)
                 malloc(colors * sizeof(_bmp_palette_entry))))       break;

            if(fseek(p_ctx->fp,
                     p_ctx->info.info_size - sizeof(_bmp_info),
                     SEEK_CUR))                                      break;
            if(!_bmp_ReadPalette(p_ctx->palette, colors, p_ctx->fp)) break;
        }

        /* Set things up for decoding: */
        if(!(p_ctx->file_data = (uint8_t *)
             malloc(p_ctx->file_line_len)))               break;
        if(!(p_ctx->rgb_data = (uint8_t *)
             malloc(p_ctx->rgb_line_len * p_ctx->lines))) break;

        success = 1;
    } while(0);

    return success;
}

/* Decodes 24-bit bitmap data.  Takes a pointer to an output buffer scan line
 * (p_rgb), a pointer to the end of the PIXEL DATA of this scan line
 * (p_rgb_end), a pointer to the source scan line of file data (p_file), and a
 * pointer to a palette (not used in this case).
 */
static void _bmp_Decode24(uint8_t * p_rgb, uint8_t * p_rgb_end,
                          uint8_t * p_file, _bmp_palette_entry * palette)
{
    while(p_rgb < p_rgb_end)
    {
        /* Output is RGB, but input is BGR, hence the +/- 2. */
        *p_rgb++ = *(p_file++ + 2);
        *p_rgb++ = *(p_file++    );
        *p_rgb++ = *(p_file++ - 2);
    }

    /* palette is unused; this prevents a pedantic warning: */
    (void)palette;
}

/* Same as _bmp_Decode24, but for 8 bit palette data.
 */
static void _bmp_Decode8(uint8_t * p_rgb, uint8_t * p_rgb_end,
                         uint8_t * p_file, _bmp_palette_entry * palette)
{
    while(p_rgb < p_rgb_end)
    {
        *p_rgb++ = palette[*p_file  ].red;
        *p_rgb++ = palette[*p_file  ].green;
        *p_rgb++ = palette[*p_file++].blue;
    }
}

/* Same as _bmp_Decode24, but for 4 bit palette data.
 */
static void _bmp_Decode4(uint8_t * p_rgb, uint8_t * p_rgb_end,
                         uint8_t * p_file, _bmp_palette_entry * palette)
{
    int lookup;

    while(p_rgb < p_rgb_end)
    {
        lookup = (*p_file & 0xf0) >> 4;

        *p_rgb++ = palette[lookup].red;
        *p_rgb++ = palette[lookup].green;
        *p_rgb++ = palette[lookup].blue;

        if(p_rgb < p_rgb_end)
        {
            lookup = *p_file++ & 0x0f;

            *p_rgb++ = palette[lookup].red;
            *p_rgb++ = palette[lookup].green;
            *p_rgb++ = palette[lookup].blue;
        }
    }
}

/* Same as _bmp_Decode24, but for monochrome palette data.
 */
static void _bmp_Decode1(uint8_t * p_rgb, uint8_t * p_rgb_end,
                         uint8_t * p_file, _bmp_palette_entry * palette)
{
    int shift;
    int lookup;

    while(p_rgb < p_rgb_end)
    {
        for(shift = 7; shift >= 0 && p_rgb < p_rgb_end; --shift)
        {
            lookup = (*p_file >> shift) & 1;

            *p_rgb++ = palette[lookup].red;
            *p_rgb++ = palette[lookup].green;
            *p_rgb++ = palette[lookup].blue;
        }

        p_file++;
    }
}

/* Selects an above decoder and runs it for each scan line of the file.
 * Returns 0 if there's an error or 1 if it's gravy.
 */
static int _bmp_Decode(_bmp_read_context * p_ctx)
{
    void (* decoder)(uint8_t *, uint8_t *, uint8_t *,
                     _bmp_palette_entry *) = NULL;

    uint8_t * p_rgb;      /* pointer to current scan line in output buffer */
    uint8_t * p_rgb_end;  /* end marker for output buffer */
    uint8_t * p_line_end; /* pointer to end of current scan line in output */
    /* FIXME: this should be ssize_t or something equivalent yet portable: */
    int       rgb_inc;    /* incrementor for scan line in output buffer */

    switch(p_ctx->info.bits)
    {
        case 24: decoder = _bmp_Decode24; break;
        case 8:  decoder = _bmp_Decode8;  break;
        case 4:  decoder = _bmp_Decode4;  break;
        case 1:  decoder = _bmp_Decode1;  break;
    }

    if(!(p_ctx->info.height < 0) == !(p_ctx->flags & BMPREAD_TOP_DOWN))
    {
        /* We're keeping scan lines in order. */
        p_rgb      = p_ctx->rgb_data;
        p_rgb_end  = p_ctx->rgb_data + (p_ctx->rgb_line_len * p_ctx->lines);
        rgb_inc    = (int)p_ctx->rgb_line_len;
    }
    else
    {
        /* We're reversing scan lines. */
        p_rgb     = p_ctx->rgb_data + (p_ctx->rgb_line_len * (p_ctx->lines-1));
        p_rgb_end = p_ctx->rgb_data - p_ctx->rgb_line_len;
        rgb_inc   = -(int)p_ctx->rgb_line_len;
    }

    p_line_end = p_rgb + p_ctx->info.width * 3;

    if(decoder && !fseek(p_ctx->fp, p_ctx->header.data_offset, SEEK_SET))
    {
        while(p_rgb != p_rgb_end &&
              fread(p_ctx->file_data, 1, p_ctx->file_line_len, p_ctx->fp) ==
              p_ctx->file_line_len)
        {
            decoder(p_rgb, p_line_end, p_ctx->file_data, p_ctx->palette);
            p_rgb += rgb_inc;
            p_line_end += rgb_inc;
        }
    }

    return (p_rgb == p_rgb_end);
}

/* Frees resources allocated by various functions along the way.  Only frees
 * rgb_data if !leave_rgb_data (if the bitmap loads successfully, you want the
 * data to remain until THEY free it).
 */
static void _bmp_FreeContext(_bmp_read_context * p_ctx, int leave_rgb_data)
{
    if(p_ctx->fp)
        fclose(p_ctx->fp);
    if(p_ctx->palette)
        free(p_ctx->palette);
    if(p_ctx->file_data)
        free(p_ctx->file_data);

    if(!leave_rgb_data && p_ctx->rgb_data)
        free(p_ctx->rgb_data);
}

int bmpread(const char * bmp_file, unsigned int flags, bmpread_t * p_bmp_out)
{
    int success = 0;

    _bmp_read_context ctx;
    memset(&ctx, 0, sizeof(_bmp_read_context));

    do
    {
        if(!bmp_file)  break;
        if(!p_bmp_out) break;
        memset(p_bmp_out, 0, sizeof(bmpread_t));

        ctx.flags = flags;

        if(!(ctx.fp = fopen(bmp_file, "rb"))) break;
        if(!_bmp_Validate(&ctx))              break;
        if(!_bmp_Decode(&ctx))                break;

        p_bmp_out->width = ctx.info.width;
        p_bmp_out->height = ctx.lines;
        p_bmp_out->rgb_data = ctx.rgb_data;

        success = 1;
    } while(0);

    _bmp_FreeContext(&ctx, success);

    return success;
}

void bmpread_free(bmpread_t * p_bmp)
{
    if(p_bmp)
    {
        if(p_bmp->rgb_data)
            free(p_bmp->rgb_data);

        memset(p_bmp, 0, sizeof(bmpread_t));
    }
}
