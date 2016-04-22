/******************************************************************************
* libbmpread - tiny, fast bitmap (.bmp) image file loader                     *
*              <https://github.com/chazomaticus/libbmpread>                   *
* Copyright (C) 2005, 2012 Charles Lindsay <chaz@chazomatic.us>               *
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
 * version 1.1+git
 * 2012-09-29
 */


#include "bmpread.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* If your compiler doesn't come with stdint.h, which is technically a C99   */
/* feature, see <http://stackoverflow.com/q/126279>.  There are 3rd party    */
/* solutions to this problem, which you should be able to find with a little */
/* searching.  Alternately, just define the following types yourself:        */
/* uint8_t, uint16_t, uint32_t, and int32_t.                                 */
#include <stdint.h>


/* Reads up to 4 little-endian bytes from fp and stores the result in the
 * uint32_t pointed to by dest in the host's byte order.  Returns 0 on EOF or
 * nonzero on success.
 */
static int _bmp_ReadLittleBytes(uint32_t * dest, int bytes, FILE * fp)
{
    int shift = 0;

    *dest = 0;

    while(bytes--)
    {
        int byte;
        if((byte = fgetc(fp)) == EOF) return 0;

        *dest += (uint32_t)byte << shift;
        shift += 8; /* assume CHAR_BIT of 8, because other code here does */
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

typedef struct _bmp_header /* bitmap file header */
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

typedef struct _bmp_info /* immediately follows header; describes image */
{
    uint32_t info_size;   /* size of info struct (is >sizeof(_bmp_info)) */
    int32_t  width;       /* width of image */
    int32_t  height;      /* height (< 0 means right-side up) */
    uint16_t planes;      /* planes (should be 1) */
    uint16_t bits;        /* number of bits (1, 4, 8, 16, 24, or 32) */
    uint32_t compression; /* 0 = none, 1 = 8-bit RLE, 2 = 4-bit RLE */
    /* there are other fields in the actual file info, but we don't need 'em */

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

typedef struct _bmp_palette_entry /* a single color in the palette */
{
    uint8_t blue;   /* blue comes first, for some reason */
    uint8_t green;  /* green component */
    uint8_t red;    /* red comes last */
    uint8_t unused; /* one unused byte.  Great. */

} _bmp_palette_entry;

/* Reads the given number of colors from fp into the palette array.  Returns 0
 * on EOF or nonzero on success.
 */
static int _bmp_ReadPalette(_bmp_palette_entry * palette, int colors,
                            FILE * fp)
{
    /* This is probably the least efficient way to go about this.  But, it's
     * the easiest, without going into implementation-defined behavior or
     * allocating a chunk of memory.  The hope is that the compiler would
     * optimize a bunch of this away into something like a nice long memcpy
     * (from the stdio buffer, I presume).
     *
     * If this is too slow, you could still avoid implementation-defined
     * behavior by allocating space for a buffer (either heap or stack if you
     * don't want to malloc) and doing one long fread, then copying bytes
     * manually.
     */
    int i;
    for(i = 0; i < colors; i++)
    {
        if(!_bmp_ReadUint8(&palette[i].blue,   fp)) return 0;
        if(!_bmp_ReadUint8(&palette[i].green,  fp)) return 0;
        if(!_bmp_ReadUint8(&palette[i].red,    fp)) return 0;
        if(!_bmp_ReadUint8(&palette[i].unused, fp)) return 0;
    }
    return 1;
}

typedef struct _bmp_read_context /* data passed around between read ops */
{
    FILE               * fp;            /* file pointer */
    _bmp_header          header;        /* file header */
    _bmp_info            info;          /* file info */
    int                  lines;         /* how many scan lines (abs(height)) */
    int                  file_line_len; /* how many bytes each scan line is */
    int                  rgb_line_len;  /* bytes in each output line */
    _bmp_palette_entry * palette;       /* palette */
    uint8_t            * file_data;     /* a line of data in the file */
    uint8_t            * rgb_data;      /* rgb data output buffer */

} _bmp_read_context;


static int _bmp_Validate(_bmp_read_context * p_ctx, int flags);
static int _bmp_InitDecode(_bmp_read_context * p_ctx);
static int _bmp_Decode(_bmp_read_context * p_ctx, int flags);
static void _bmp_FreeContext(_bmp_read_context * p_ctx, int leave_rgb_data);


/* see header for details */
int bmpread(const char * bmp_file, int flags, bmpread_t * p_bmp_out)
{
    int success = 0;

    _bmp_read_context ctx;
    memset(&ctx, 0, sizeof(_bmp_read_context));

    do
    {
        if(!bmp_file)  break;
        if(!p_bmp_out) break;
        memset(p_bmp_out, 0, sizeof(bmpread_t));

        if(!(ctx.fp = fopen(bmp_file, "rb"))) break;
        if(!_bmp_Validate(&ctx, flags))       break;
        if(!_bmp_InitDecode(&ctx))            break;
        if(!_bmp_Decode(&ctx, flags))         break;

        p_bmp_out->width = (int)ctx.info.width;
        p_bmp_out->height = ctx.lines;
        p_bmp_out->rgb_data = ctx.rgb_data;

        success = 1;
    } while(0);

    _bmp_FreeContext(&ctx, success);

    return success;
}

/* see header for details */
void bmpread_free(bmpread_t * p_bmp)
{
    if(p_bmp)
    {
        if(p_bmp->rgb_data)
            free(p_bmp->rgb_data);

        memset(p_bmp, 0, sizeof(bmpread_t));
    }
}

/* _bmp_IsPowerOf2
 *
 * Returns whether an integer is a power of 2.
 */
static int _bmp_IsPowerOf2(int x)
{
    int bit;

    if(x < 0)
        x = -x;

    for(bit = 1; bit; bit <<= 1)
    {
        if(x & bit)
            return !(x & ~bit); /* return nonzero if no other bits are set */
    }

    /* if it didn't find a bit, x was 0, which isn't a power of 2 */
    return 0;
}

/* _bmp_GetLineLength
 *
 * Returns the DWORD-aligned byte-length of a scan line.  For instance, for
 * 24-bit data 3 pixels wide, it would return 12 (3 pixels * 3 bytes each = 9
 * bytes, then padded to the next DWORD).  bpp is BITS per pixel, not bytes.
 */
static int _bmp_GetLineLength(int width, int bpp)
{
    int bits;     /* number of bits in a line */
    int pad_bits; /* number of padding bits to make bits divisible by 32 */

    bits = width * bpp;
    pad_bits = 32 - (bits & 0x1f); /* bits & 0x1f == bits % 32, but faster */

    /* if it's not already dword aligned, add pad_bits to it */
    if(pad_bits < 32)
        bits += pad_bits;

    return bits/8; /* convert to bytes */
}

/* _bmp_Validate
 *
 * Reads and validates the bitmap header metadata from the context's file
 * object.  Assumes the file pointer is at the start of the file.  Returns 1 if
 * ok or 0 if error or invalid file.
 */
static int _bmp_Validate(_bmp_read_context * p_ctx, int flags)
{
    int success = 0;

    do
    {
        if(!_bmp_ReadHeader(&p_ctx->header, p_ctx->fp)) break;
        if(!_bmp_ReadInfo(  &p_ctx->info,   p_ctx->fp)) break;

        /* some basic validation */
        if(p_ctx->header.magic[0] != 0x42 /* 'B' */) break;
        if(p_ctx->header.magic[1] != 0x4d /* 'M' */) break;

        if(p_ctx->info.width <= 0 || p_ctx->info.height == 0) break;
        /* no compression supported yet (TODO: RLE) */
        if(p_ctx->info.compression > 0)                       break;
        if(p_ctx->info.bits != 1 && p_ctx->info.bits != 4 &&
           p_ctx->info.bits != 8 && p_ctx->info.bits != 24)   break;

        p_ctx->lines = ((p_ctx->info.height < 0) ?
                        (int)-p_ctx->info.height :
                        (int) p_ctx->info.height);

        p_ctx->file_line_len = _bmp_GetLineLength(p_ctx->info.width,
                                                  p_ctx->info.bits);

        p_ctx->rgb_line_len = ((flags & BMPREAD_BYTE_ALIGN) ?
                               (int)p_ctx->info.width * 3 :
                               _bmp_GetLineLength(p_ctx->info.width, 24));

        if(!(flags & BMPREAD_ANY_SIZE))
        {
            if(!_bmp_IsPowerOf2((int)p_ctx->info.width)) break;
            if(!_bmp_IsPowerOf2(p_ctx->lines))           break;
        }

        /* handle palettes */
        if(p_ctx->info.bits <= 8)
        {
            unsigned int colors = 1 << p_ctx->info.bits;

            if(!(p_ctx->palette = (_bmp_palette_entry *)
                 malloc(colors * sizeof(_bmp_palette_entry))))  break;

            if(fseek(p_ctx->fp,
                     p_ctx->info.info_size - sizeof(_bmp_info),
                     SEEK_CUR))                                 break;
            if(!_bmp_ReadPalette(p_ctx->palette,
                                 (int)colors, p_ctx->fp))       break;
        }

        success = 1;
    } while(0);

    return success;
}

/* _bmp_InitDecode
 *
 * Allocates memory for the decode operation and sets up the file pointer.
 * Returns 1 if success, 0 if failure.
 */
static int _bmp_InitDecode(_bmp_read_context * p_ctx)
{
    return (   (p_ctx->file_data = (uint8_t *)malloc(p_ctx->file_line_len))
            && (p_ctx->rgb_data = (uint8_t *)
                malloc(p_ctx->rgb_line_len * p_ctx->lines))
            && !fseek(p_ctx->fp, p_ctx->header.data_offset, SEEK_SET));
}

/* _bmp_Decode24
 *
 * Decodes 24-bit bitmap data.  Takes a pointer to an output buffer scan line
 * (p_rgb), a pointer to the end of the PIXEL DATA of this scan line
 * (p_rgb_end), a pointer to the source scan line of file data (p_file), and a
 * pointer to a palette (not used in this case).
 */
static void _bmp_Decode24(uint8_t * p_rgb, uint8_t * p_rgb_end,
                          uint8_t * p_file, _bmp_palette_entry * palette)
{
    while(p_rgb < p_rgb_end)
    {
        /* output is RGB, but input is BGR, hence the +/- 2 */
        *p_rgb++ = *(p_file++ + 2);
        *p_rgb++ = *(p_file++    );
        *p_rgb++ = *(p_file++ - 2);
    }

    /* palette is unused; this prevents a pedantic warning */
    (void)palette;
}

/* _bmp_Decode8
 *
 * Same as _bmp_Decode24, but for 8 bit palette data.
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

/* _bmp_Decode4
 *
 * Same as _bmp_Decode24, but for 4 bit palette data.
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

/* _bmp_Decode1
 *
 * Same as _bmp_Decode24, but for monochrome palette data.
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

/* _bmp_Decode
 *
 * Selects an above decoder and runs it for each scan line of the file.
 * Returns 0 if there's an error or 1 if it's gravy.
 */
static int _bmp_Decode(_bmp_read_context * p_ctx, int flags)
{
    void (* decoder)(uint8_t *, uint8_t *, uint8_t *,
                     _bmp_palette_entry *) = NULL;

    uint8_t * p_rgb;      /* pointer to current scan line in output buffer */
    int rgb_inc;          /* incrementor for scan line in outupt buffer */
    uint8_t * p_rgb_end;  /* end marker for output buffer */
    uint8_t * p_line_end; /* pointer to end of current scan line in output */

    switch(p_ctx->info.bits)
    {
        case 24: decoder = _bmp_Decode24; break;
        case 8:  decoder = _bmp_Decode8;  break;
        case 4:  decoder = _bmp_Decode4;  break;
        case 1:  decoder = _bmp_Decode1;  break;
    }

    if(!(p_ctx->info.height < 0) == !(flags & BMPREAD_TOP_DOWN))
    {
        /* keeping scan lines in order */
        p_rgb      = p_ctx->rgb_data;
        rgb_inc    = p_ctx->rgb_line_len;
        p_rgb_end  = p_ctx->rgb_data + (p_ctx->rgb_line_len * p_ctx->lines);
    }
    else
    {
        /* reversing scan lines */
        p_rgb     = p_ctx->rgb_data + (p_ctx->rgb_line_len * (p_ctx->lines-1));
        rgb_inc   = -p_ctx->rgb_line_len;
        p_rgb_end = p_ctx->rgb_data - p_ctx->rgb_line_len;
    }

    p_line_end = p_rgb + p_ctx->info.width * 3;

    if(decoder)
    {
        while(p_rgb != p_rgb_end &&
              fread(p_ctx->file_data, p_ctx->file_line_len, 1, p_ctx->fp) == 1)
        {
            decoder(p_rgb, p_line_end, p_ctx->file_data, p_ctx->palette);
            p_rgb += rgb_inc;
            p_line_end += rgb_inc;
        }
    }

    return (p_rgb == p_rgb_end);
}

/* _bmp_FreeContext
 *
 * Frees resources allocated by various functions along the way.  Only frees
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
