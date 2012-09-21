/******************************************************************************
* glbmp - a library for loading Windows & OS/2 bitmaps for use in OpenGL      *
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


#include <stdio.h>

#ifdef _WIN32
#include <windows.h>
#endif
#include <GL/gl.h>
#include <GL/glut.h>

#include "glbmp.h"


static void DrawBitmap(void);


static GLuint tex = 0;


int main(int argc, char * argv[])
{
   glbmp_t bmp;

   puts("glbmp-test - test utility for glbmp");
   puts("Copyright (C) 2005, 2012 Charles Lindsay <chaz@chazomatic.us>");
   puts("This program is released under the zlib license (see COPYING) and");
   puts("comes with absolutely no warranty whatsoever.");
   puts("");

   if(argc < 2)
   {
      puts("Usage: glbmp-test <bmpfile> [gl_options]");
      puts("glbmp-test loads <bmpfile> and attempts to display it on an OpenGL quad,");
      puts("stretched across the entire window, using GLUT.  If the bitmap looks correct,");
      puts("glbmp works!  You can pass options to GLUT using [gl_options].");
      return 0;
   }

   printf("Loading %s...", argv[1]);
   if(!glbmp_LoadBitmap(argv[1], 0, &bmp))
   {
      puts("error!");
      return 1;
   }
   puts("OK");

   glutInitWindowSize(bmp.width, bmp.height);
   glutInit(&argc, argv);
   glutInitDisplayMode(GLUT_RGB | GLUT_DOUBLE);
   glutCreateWindow(argv[1]);

   glutDisplayFunc(DrawBitmap);

   glMatrixMode(GL_PROJECTION);
   glLoadIdentity();
   glOrtho(-0.5f, 0.5f, -0.5f, 0.5f, 0.1f, 1.0f);

   glMatrixMode(GL_MODELVIEW);
   glLoadIdentity();

   glEnable(GL_TEXTURE_2D);

   glGenTextures(1, &tex);
   glBindTexture(GL_TEXTURE_2D, tex);
   glTexImage2D(GL_TEXTURE_2D, 0, 3, bmp.width, bmp.height, 0, GL_RGB, GL_UNSIGNED_BYTE, bmp.rgb_data);
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

   /*
   {
      FILE * fp = fopen("dump.dat", "wb");
      fwrite(&bmp.width, sizeof(int), 1, fp);
      fwrite(&bmp.height, sizeof(int), 1, fp);
      fwrite(bmp.rgb_data, bmp.width * bmp.height * 3, 1, fp);
      fclose(fp);
   }
   //*/

   glbmp_FreeBitmap(&bmp);

   glutPostRedisplay();
   glutMainLoop();
   return 0;
}

static void DrawBitmap(void)
{
   glMatrixMode(GL_MODELVIEW);
   glPushMatrix();
   glTranslatef(0.0f, 0.0f, -0.5f);

   glBindTexture(GL_TEXTURE_2D, tex);

   glBegin(GL_QUADS);
   glTexCoord2f(0.0f, 0.0f); glVertex3f(-0.5f, -0.5f, 0.0f);
   glTexCoord2f(1.0f, 0.0f); glVertex3f( 0.5f, -0.5f, 0.0f);
   glTexCoord2f(1.0f, 1.0f); glVertex3f( 0.5f,  0.5f, 0.0f);
   glTexCoord2f(0.0f, 1.0f); glVertex3f(-0.5f,  0.5f, 0.0f);
   glEnd();

   glPopMatrix();

   glutSwapBuffers();
}
