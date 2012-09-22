libbmpread
==========

libbmpread is a tiny, fast bitmap (.bmp) image file loader, written from
scratch in portable ANSI C.  Its default behavior is compatible with OpenGL
texture functions, making it ideal for use in simple games.  It handles
uncompressed monochrome, 16- and 256-color, and 24-bit bitmap files of any size
(no RLE support yet).

See `bmpread.h` for thorough documentation of the interface.

Sample Code
-----------

Here's a snippet showing how libbmpread might be used to create an OpenGL
texture from a bitmap file on disk:

    #include <stdio.h>
    #include <GL/gl.h>
    #include "bmpread.h"

    /* Loads the specified bitmap file from disk and copies it into an OpenGL
     * texture.  Returns the GLuint representing the texture.  Calls exit(1) if
     * the bitmap fails to load.
     */
    GLuint LoadTexture(const char * bitmap_file)
    {
        GLuint texture = 0;
        bmpread_t bitmap;

        if(!bmpread(bitmap_file, 0, &bitmap))
        {
            fprintf(stderr, "%s: error loading bitmap file\n", bitmap_file);
            exit(1);
        }

        glGenTextures(1, &texture);
        glBindTexture(GL_TEXTURE_2D, texture);

        glTexImage2D(GL_TEXTURE_2D, 0, 3, bitmap.width, bitmap.height, 0,
                     GL_RGB, GL_UNSIGNED_BYTE, bitmap.rgb_data);

        bmpread_free(&bitmap);

        return texture;
    }

    void SomeInitFunction(void)
    {
        GLuint tex1 = LoadTexture("texture1.bmp");
        // ...
    }

Enjoy!

 -- Charles D. Lindsay, Esq.
