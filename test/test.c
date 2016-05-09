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


#include "bmpread.c"

#include <assert.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>


static const char * const test_data = "./test.data";


static void test_ReadLittleUint32(void)
{
    uint32_t a;
    uint32_t b;
    uint32_t c;
    FILE * fp = fopen(test_data, "rb");

    /* Too bad the assertions won't tell you which value failed.  I suggest
     * re-running the failed binary under gdb.
     */
    assert(_bmp_ReadLittleUint32(&a, fp));
    assert(_bmp_ReadLittleUint32(&b, fp));
    assert(a == UINT32_C(0x04030201));
    assert(b == UINT32_C(0x80706050));

    assert(!_bmp_ReadLittleUint32(&c, fp));

    fclose(fp);
}

static void test_ReadLittleInt32(void)
{
    int32_t a;
    int32_t b;
    int32_t c;
    FILE * fp = fopen(test_data, "rb");

    assert(_bmp_ReadLittleInt32(&a, fp));
    assert(_bmp_ReadLittleInt32(&b, fp));
    assert(a == INT32_C(   67305985));
    assert(b == INT32_C(-2140118960));

    assert(!_bmp_ReadLittleInt32(&c, fp));

    fclose(fp);
}

static void test_ReadLittleUint16(void)
{
    uint16_t a;
    uint16_t b;
    uint16_t c;
    uint16_t d;
    uint16_t e;
    FILE * fp = fopen(test_data, "rb");

    assert(_bmp_ReadLittleUint16(&a, fp));
    assert(_bmp_ReadLittleUint16(&b, fp));
    assert(_bmp_ReadLittleUint16(&c, fp));
    assert(_bmp_ReadLittleUint16(&d, fp));
    assert(a == UINT16_C(0x0201));
    assert(b == UINT16_C(0x0403));
    assert(c == UINT16_C(0x6050));
    assert(d == UINT16_C(0x8070));

    assert(!_bmp_ReadLittleUint16(&e, fp));

    fclose(fp);
}

static void test_ReadUint8(void)
{
    uint8_t a;
    uint8_t b;
    uint8_t c;
    uint8_t d;
    uint8_t e;
    uint8_t f;
    uint8_t g;
    uint8_t h;
    uint8_t i;
    FILE * fp = fopen(test_data, "rb");

    assert(_bmp_ReadUint8(&a, fp));
    assert(_bmp_ReadUint8(&b, fp));
    assert(_bmp_ReadUint8(&c, fp));
    assert(_bmp_ReadUint8(&d, fp));
    assert(_bmp_ReadUint8(&e, fp));
    assert(_bmp_ReadUint8(&f, fp));
    assert(_bmp_ReadUint8(&g, fp));
    assert(_bmp_ReadUint8(&h, fp));
    assert(a == UINT8_C(0x01));
    assert(b == UINT8_C(0x02));
    assert(c == UINT8_C(0x03));
    assert(d == UINT8_C(0x04));
    assert(e == UINT8_C(0x50));
    assert(f == UINT8_C(0x60));
    assert(g == UINT8_C(0x70));
    assert(h == UINT8_C(0x80));

    assert(!_bmp_ReadUint8(&i, fp));

    fclose(fp);
}

static void test_IsPowerOf2(void)
{
    int32_t i;
    int32_t j;

    for(i = 1; i <= INT32_MAX / 2; i *= 2)
    {
        assert(_bmp_IsPowerOf2(i));
        for(j = i + 1; j < i * 2; j++)
            assert(!_bmp_IsPowerOf2(j));
    }

    assert(_bmp_IsPowerOf2(i));
    if(i < INT32_MAX)
    {
        for(j = i + 1; j < INT32_MAX; j++)
            assert(!_bmp_IsPowerOf2(j));
        assert(!_bmp_IsPowerOf2(INT32_MAX));
    }

    assert(!_bmp_IsPowerOf2(0));

    for(i = -1; i >= INT32_MIN / 2; i *= 2)
    {
        assert(_bmp_IsPowerOf2(i));
        for(j = i - 1; j > i * 2; j--)
            assert(!_bmp_IsPowerOf2(j));
    }

    assert(_bmp_IsPowerOf2(i));
    if(i > INT32_MIN)
    {
        for(j = i - 1; j > INT32_MIN; j--)
            assert(!_bmp_IsPowerOf2(j));
        assert(_bmp_IsPowerOf2(INT32_MIN));
    }
}

static void test_GetLineLength(void)
{
    size_t i;
    for(i = 1; i <= 32; i++)
        assert(_bmp_GetLineLength(i, 1) == 4);
    assert(_bmp_GetLineLength(33, 1) == 8);
    /* Etc.  TODO: test near SIZE_MAX. */

    for(i = 1; i <= 8; i++)
        assert(_bmp_GetLineLength(i, 4) == 4);
    assert(_bmp_GetLineLength(9, 4) == 8);

    for(i = 1; i <= 4; i++)
        assert(_bmp_GetLineLength(i, 8) == 4);
    assert(_bmp_GetLineLength(5, 8) == 8);

    assert(_bmp_GetLineLength(1, 24) ==  4);
    assert(_bmp_GetLineLength(2, 24) ==  8);
    assert(_bmp_GetLineLength(3, 24) == 12);
    assert(_bmp_GetLineLength(4, 24) == 12);
    assert(_bmp_GetLineLength(5, 24) == 16);
    assert(_bmp_GetLineLength(6, 24) == 20);
    assert(_bmp_GetLineLength(7, 24) == 24);
    assert(_bmp_GetLineLength(8, 24) == 24);
    assert(_bmp_GetLineLength(9, 24) == 28);
    /* Etc. */
}

int main(int argc, char * argv[])
{
    printf("%s: running tests\n", argv[0]);

#define TEST(x) do                                 \
{                                                  \
    printf("%s: testing _bmp_%s...", argv[0], #x); \
    fflush(stdout);                                \
    test_##x();                                    \
    printf("OK\n");                                \
} while(0)

    TEST(ReadLittleUint32);
    TEST(ReadLittleInt32);
    TEST(ReadLittleUint16);
    TEST(ReadUint8);
    TEST(IsPowerOf2);
    TEST(GetLineLength);

#undef TEST

    printf("%s: all tests passed\n", argv[0]);
    return 0;

    (void)argc; /* Unused. */
}
