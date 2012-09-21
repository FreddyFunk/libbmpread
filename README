glbmp
=====

glbmp is an extremely portable, simple, robust, and lightweight replacement for
the auxDIBImageLoad and related device-independent bitmap (.bmp) image loader
functions of glaux, but it goes far beyond just that. At the basic level, it's
a flexible bitmap loader and decoder that can be used anywhere you need to get
the color data of a .bmp file.

Why create glbmp when it's just duplicating existing functionality?
  - glaux.h and glaux.lib (or libglaux.a) no longer exist on most systems: a
    new solution is needed.
So, then why not use an existing image loading library?
  - There aren't very many full-featured and portable image libraries that are
    lightweight enough to be useful (glbmp is two files: one .c and one .h that
    can be easily dropped into any existing project).
But why bitmaps?
  - Bitmaps are fairly common in simple games (that is, games where a drop-in
    image loader is called for) and easy to work with.


Features
--------
 - Handles uncompressed monochrome, 16 color, 256 color, and 24-bit bitmap
   files of any size.
 - Flexible output formats: dword or byte aligned scan-lines, last line first
   (upside-down) or linear.
 - Default behavior is compatible with OpenGL's texture functions.
 - Extremely lightweight, yet powerful and portable.
 - Written in 100% ANSI C, tested (and working!) on Win32, Linux, and MacOS X.
 - zlib licensed, so it can be used for pretty much anything on earth.


More Info
---------
For a quick tutorial on how to use glbmp, see glbmp-tutorial.html in this
distribution.

Visit the glbmp webpage at http://chaoslizard.sourceforge.net/glbmp/ for
updates, news, etc.

Email me at charles [at] chaoslizard [dot] org with questions, bug reports,
etc.

To see glbmp in action download my cross-platform Asteroids clone from
http://chaoslizard.sourceforge.net/asteroid/ -- all textures (all one of 'em)
are loaded using glbmp.


Changes
-------
v1.1 (2005 Mar. 15):
 - Added byte-swapping code for big-endian machines (glbmp will now work on
   Macs!).  Thanks to Micah Tomblin for testing this for me.

v1.0 (2005 Mar. 14):
 - Initial version, supporting 1-, 4-, 8- and 24-bit uncompressed files.


License
-------
Copyright (C) 2005 Charles Lindsay

This software is provided 'as-is', without any express or implied
warranty.  In no event will the authors be held liable for any damages
arising from the use of this software.

Permission is granted to anyone to use this software for any purpose,
including commercial applications, and to alter it and redistribute it
freely, subject to the following restrictions:

1. The origin of this software must not be misrepresented; you must not
   claim that you wrote the original software. If you use this software
   in a product, an acknowledgment in the product documentation would be
   appreciated but is not required.
2. Altered source versions must be plainly marked as such, and must not be
   misrepresented as being the original software.
3. This notice may not be removed or altered from any source distribution.


Word
----
Thanks for downloading.  Enjoy!

 -- Charles D. Lindsay, Esq.
