/*
 *  Copyright (C) 1999 AT&T Laboratories Cambridge.  All Rights Reserved.
 *  Copyright (C) 2002 Tim Jansen. All Rights Reserved.
 *  Copyright (C) 1999-2001 Anders Lindström
 * 
 *  
 *
 *  This is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This software is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this software; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307,
 *  USA.
 *
 * tim@tjansen.de: - removed stuff for krdc 
 *                 - merged with shm.c and misc.c
 *                 - added FillRectangle and Sync methods to draw only on 
 *                   the image
 *                 - added Zoom functionality, based on rotation funcs from
 *                   SGE by Anders Lindström)
 *
 */

/*
 * desktop.c - functions to deal with "desktop" window.
 */

#include <X11/Xlib.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <X11/extensions/XShm.h>
#include <math.h>
#include <limits.h>
#include "vncviewer.h"

static XShmSegmentInfo shminfo;
static Bool caughtShmError = False;
static Bool needShmCleanup = False;

static XShmSegmentInfo zoomshminfo;
static Bool caughtZoomShmError = False;
static Bool needZoomShmCleanup = False;

GC gc;
GC srcGC, dstGC; /* used for debugging copyrect */
Dimension dpyWidth, dpyHeight;

static XImage *image = NULL;
Bool useShm = True;

static Bool zoomActive = False;
static int zoomWidth, zoomHeight;
static XImage *zoomImage = NULL;
static Bool useZoomShm = True;

typedef int Sint32;
typedef short Sint16;
typedef char Sint8;
typedef unsigned int Uint32;
typedef unsigned short Uint16;
typedef unsigned char Uint8;

typedef struct {
  int w, h;
  unsigned int pitch;
  void *pixels;
  int BytesPerPixel;
} Surface;

typedef struct {
        Sint16 x, y;
        Uint16 w, h;
} Rect;


static void CopyDataToScreenRaw(char *buf, int x, int y, int width, int height);
static void CopyBGR233ToScreen(CARD8 *buf, int x, int y, int width,int height);
static void FillRectangleBGR233(CARD8 buf, int x, int y, int width,int height);
static int CheckRectangle(int x, int y, int width, int height);

static void ZoomInit(void);
static void transformZoomSrc(int six, int siy, int siw, int sih,
			     int *dix, int *diy, int *diw, int *dih,
			     int srcW, int dstW, int srcH, int dstH);
static void transformZoomDst(int *six, int *siy, int *siw, int *sih,
			     int dix, int diy, int diw, int dih,
			     int srcW, int dstW, int srcH, int dstH);
static void ZoomSurfaceSrcCoords(int x, int y, int w, int h, 
				 int *dix, int *diy, int *diw, int *dih,
				 Surface * src, Surface * dst);
static void ZoomSurfaceCoords32(int sx, int sy, int sw, int sh,
				int dx, int dy, Surface * src, Surface * dst);
static void sge_transform(Surface *src, Surface *dst, float angle, float xscale, float yscale,
			  Uint16 px, Uint16 py, Uint16 qx, Uint16 qy);


void
DesktopInit(Window win)
{
  XGCValues gcv;

  /* image = CreateShmImage(); */

  if (!image) {
    useShm = False;
    image = XCreateImage(dpy, vis, visdepth, ZPixmap, 0, NULL,
			 si.framebufferWidth, si.framebufferHeight,
			 BitmapPad(dpy), 0);

    image->data = calloc(image->bytes_per_line * image->height, 1);
    if (!image->data) {
      fprintf(stderr,"malloc failed\n");
      exit(1);
    }
  }

  gc = XCreateGC(dpy,win,0,NULL);

  gcv.function = GXxor;
  gcv.foreground = 0x0f0f0f0f;
  srcGC = XCreateGC(dpy,win,GCFunction|GCForeground,&gcv);
  gcv.foreground = 0xf0f0f0f0;
  dstGC = XCreateGC(dpy,win,GCFunction|GCForeground,&gcv);
}

/*
 * DrawScreenRegionX11Thread
 * Never call from any other desktop.c function, only for X11 thread
 */

void
DrawScreenRegionX11Thread(Window win, int x, int y, int width, int height) {
  zoomActive = False;
  zoomWidth = 0;
  zoomHeight = 0;

  if (useShm) 
    XShmPutImage(dpy, win, gc, image, x, y, x, y, width, height, False);
  else
    XPutImage(dpy, win, gc, image, x, y, x, y, width, height);
}

/*
 * CheckRectangle
 */

static int CheckRectangle(int x, int y, int width, int height) {
  if ((x < 0) || (y < 0))
    return 0;

  if (((x+width) > si.framebufferWidth) || ((y+height) > si.framebufferHeight))
    return 0;

  return 1;
}


/*
 * CopyDataToScreen.
 */

void
CopyDataToScreen(char *buf, int x, int y, int width, int height)
{
  if (!CheckRectangle(x, y, width, height))
    return;

  if (!appData.useBGR233)
    CopyDataToScreenRaw(buf, x, y, width, height);
  else
    CopyBGR233ToScreen((CARD8 *)buf, x, y, width, height);

  SyncScreenRegion(x, y, width, height);
}

/*
 * CopyDataToScreenRaw.
 */

static void
CopyDataToScreenRaw(char *buf, int x, int y, int width, int height)
{
  int h;
  int widthInBytes = width * myFormat.bitsPerPixel / 8;
  int scrWidthInBytes = image->bytes_per_line;
  
  char *scr = (image->data + y * scrWidthInBytes
	       + x * myFormat.bitsPerPixel / 8);
  
  for (h = 0; h < height; h++) {
    memcpy(scr, buf, widthInBytes);
    buf += widthInBytes;
    scr += scrWidthInBytes;
  }
}

/*
 * CopyBGR233ToScreen.
 */

static void
CopyBGR233ToScreen(CARD8 *buf, int x, int y, int width, int height)
{
  int p, q;
  int xoff = 7 - (x & 7);
  int xcur;
  int fbwb = si.framebufferWidth / 8;
  CARD8 *scr1 = ((CARD8 *)image->data) + y * fbwb + x / 8;
  CARD8 *scrt;
  CARD8 *scr8 = ((CARD8 *)image->data) + y * si.framebufferWidth + x;
  CARD16 *scr16 = ((CARD16 *)image->data) + y * si.framebufferWidth + x;
  CARD32 *scr32 = ((CARD32 *)image->data) + y * si.framebufferWidth + x;

  switch (visbpp) {

    /* thanks to Chris Hooper for single bpp support */

  case 1:
    for (q = 0; q < height; q++) {
      xcur = xoff;
      scrt = scr1;
      for (p = 0; p < width; p++) {
	*scrt = ((*scrt & ~(1 << xcur))
		 | (BGR233ToPixel[*(buf++)] << xcur));

	if (xcur-- == 0) {
	  xcur = 7;
	  scrt++;
	}
      }
      scr1 += fbwb;
    }
    break;

  case 8:
    for (q = 0; q < height; q++) {
      for (p = 0; p < width; p++) {
	*(scr8++) = BGR233ToPixel[*(buf++)];
      }
      scr8 += si.framebufferWidth - width;
    }
    break;

  case 16:
    for (q = 0; q < height; q++) {
      for (p = 0; p < width; p++) {
	*(scr16++) = BGR233ToPixel[*(buf++)];
      }
      scr16 += si.framebufferWidth - width;
    }
    break;

  case 32:
    for (q = 0; q < height; q++) {
      for (p = 0; p < width; p++) {
	*(scr32++) = BGR233ToPixel[*(buf++)];
      }
      scr32 += si.framebufferWidth - width;
    }
    break;
  }
}

/*
 * FillRectangle8.
 */

void
FillRectangle8(CARD8 fg, int x, int y, int width, int height)
{
  if (!CheckRectangle(x, y, width, height))
    return;
  if (!appData.useBGR233) {
    int h;
    int widthInBytes = width * myFormat.bitsPerPixel / 8;
    int scrWidthInBytes = image->bytes_per_line;

    char *scr = (image->data + y * scrWidthInBytes
		 + x * myFormat.bitsPerPixel / 8);

    for (h = 0; h < height; h++) {
      memset(scr, fg, widthInBytes);
      scr += scrWidthInBytes;
    }
  } else {
    FillRectangleBGR233(fg, x, y, width, height);
  }
}

/*
 * FillRectangleBGR233.
 */

static void
FillRectangleBGR233(CARD8 fg, int x, int y, int width, int height)
{
  int p, q;
  int xoff = 7 - (x & 7);
  int xcur;
  int fbwb = si.framebufferWidth / 8;
  CARD8 *scr1 = ((CARD8 *)image->data) + y * fbwb + x / 8;
  CARD8 *scrt;
  CARD8 *scr8 = ((CARD8 *)image->data) + y * si.framebufferWidth + x;
  CARD16 *scr16 = ((CARD16 *)image->data) + y * si.framebufferWidth + x;
  CARD32 *scr32 = ((CARD32 *)image->data) + y * si.framebufferWidth + x;

  unsigned long fg233 = BGR233ToPixel[fg];

  switch (visbpp) {

    /* thanks to Chris Hooper for single bpp support */

  case 1:
    for (q = 0; q < height; q++) {
      xcur = xoff;
      scrt = scr1;
      for (p = 0; p < width; p++) {
	*scrt = ((*scrt & ~(1 << xcur))
		 | (fg233 << xcur));

	if (xcur-- == 0) {
	  xcur = 7;
	  scrt++;
	}
      }
      scr1 += fbwb;
    }
    break;

  case 8:
    for (q = 0; q < height; q++) {
      for (p = 0; p < width; p++) {
	*(scr8++) = fg233;
      }
      scr8 += si.framebufferWidth - width;
    }
    break;

  case 16:
    for (q = 0; q < height; q++) {
      for (p = 0; p < width; p++) {
	*(scr16++) = fg233;
      }
      scr16 += si.framebufferWidth - width;
    }
    break;

  case 32:
    for (q = 0; q < height; q++) {
      for (p = 0; p < width; p++) {
	*(scr32++) = fg233;
      }
      scr32 += si.framebufferWidth - width;
    }
    break;
  }
}

/*
 * FillRectangle16
 */

void
FillRectangle16(CARD16 fg, int x, int y, int width, int height)
{
  int i, h;
  int scrWidthInBytes = image->bytes_per_line;
  
  char *scr = (image->data + y * scrWidthInBytes
	       + x * myFormat.bitsPerPixel / 8);
  CARD16 *scr16;

  if (!CheckRectangle(x, y, width, height))
    return;

  for (h = 0; h < height; h++) {
    scr16 = (CARD16*) scr;
    for (i = 0; i < width; i++)
      scr16[i] = fg;
    scr += scrWidthInBytes;
  }
}

/*
 * FillRectangle32
 */

void
FillRectangle32(CARD32 fg, int x, int y, int width, int height)
{
  int i, h;
  int scrWidthInBytes = image->bytes_per_line;
  
  char *scr = (image->data + y * scrWidthInBytes
	       + x * myFormat.bitsPerPixel / 8);
  CARD32 *scr32;

  if (!CheckRectangle(x, y, width, height))
    return;

  for (h = 0; h < height; h++) {
    scr32 = (CARD32*) scr;
    for (i = 0; i < width; i++)
      scr32[i] = fg;
    scr += scrWidthInBytes;
  }
}

/*
 * CopyDataFromScreen.
 */

void
CopyDataFromScreen(char *buf, int x, int y, int width, int height)
{
  int widthInBytes = width * myFormat.bitsPerPixel / 8;
  int scrWidthInBytes = image->bytes_per_line;
  char *src = (image->data + y * scrWidthInBytes
	       + x * myFormat.bitsPerPixel / 8);
  int h;

  if (!CheckRectangle(x, y, width, height))
    return;

  for (h = 0; h < height; h++) {
    memcpy(buf, src, widthInBytes);
    src += scrWidthInBytes;
    buf += widthInBytes;
  }
}

/*
 * CopyArea
 */

void
CopyArea(int srcX, int srcY, int width, int height, int x, int y)
{
  int widthInBytes = width * myFormat.bitsPerPixel / 8;
  
  if ((srcY+height < y) || (y+height < srcY) ||
      (srcX+width  < x) || (x+width  < srcX)) {

    int scrWidthInBytes = image->bytes_per_line;
    char *src = (image->data + srcY * scrWidthInBytes
		 + srcX * myFormat.bitsPerPixel / 8);
    char *dst = (image->data + y * scrWidthInBytes
		 + x * myFormat.bitsPerPixel / 8);
    int h;

    if (!CheckRectangle(srcX, srcY, width, height))
      return;
    if (!CheckRectangle(x, y, width, height))
      return;
      
    for (h = 0; h < height; h++) {
      memcpy(dst, src, widthInBytes);
      src += scrWidthInBytes;
      dst += scrWidthInBytes;
    }
  }
  else { 
    char *buf = malloc(widthInBytes*height);
    if (!buf) {
      fprintf(stderr, "Out of memory, CopyArea impossible\n");
      return;
    }
    CopyDataFromScreen(buf, srcX, srcY, width, height);
    CopyDataToScreenRaw(buf, x, y, width, height);
    free(buf);
  }
  SyncScreenRegion(x, y, width, height);
}

void SyncScreenRegion(int x, int y, int width, int height) {
  int dx, dy, dw, dh; 

  if (zoomActive) {
    Surface src, dest;
    src.w = si.framebufferWidth;
    src.h = si.framebufferHeight;
    src.pitch = image->bytes_per_line;
    src.pixels = image->data;
    src.BytesPerPixel = myFormat.bitsPerPixel / 8;
    dest.w = zoomWidth;
    dest.h = zoomHeight;
    dest.pitch = zoomImage->bytes_per_line;
    dest.pixels = zoomImage->data;
    dest.BytesPerPixel = myFormat.bitsPerPixel / 8;
    ZoomSurfaceSrcCoords(x, y, width, height, &dx, &dy, &dw, &dh, &src, &dest);
  }
  else {
    dx = x; dy = y;
    dw = width; dh = height;
  }
  DrawScreenRegion(dx, dy, dw, dh);
}

void ShmSync(void) {
    if (useShm)
      XSync(dpy, False);
}

/*
 * ToplevelInitBeforeRealization sets the title, geometry and other resources
 * on the toplevel window.
 */

void
ToplevelInit()
{
  dpyWidth = WidthOfScreen(DefaultScreenOfDisplay(dpy));
  dpyHeight = HeightOfScreen(DefaultScreenOfDisplay(dpy));
}

/*
 * Cleanup - perform shm cleanup operations prior to exiting.
 */

void
Cleanup()
{
  if (useShm || useZoomShm)
    ShmCleanup();
}

void
ShmCleanup()
{
  fprintf(stderr,"ShmCleanup called\n");
  if (needShmCleanup) {
    shmdt(shminfo.shmaddr);
    shmctl(shminfo.shmid, IPC_RMID, 0);
    needShmCleanup = False;
  }
  if (needZoomShmCleanup) {
    shmdt(zoomshminfo.shmaddr);
    shmctl(zoomshminfo.shmid, IPC_RMID, 0);
    needZoomShmCleanup = False;
  }
}

static int
ShmCreationXErrorHandler(Display* _dpy, XErrorEvent *_e)
{
  caughtShmError = True;
  return 0;
}

XImage *
CreateShmImage()
{
  XImage *_image;
  XErrorHandler oldXErrorHandler;

  if (!XShmQueryExtension(dpy))
    return NULL;

  _image = XShmCreateImage(dpy, vis, visdepth, ZPixmap, NULL, &shminfo,
			  si.framebufferWidth, si.framebufferHeight);
  if (!_image) return NULL;

  shminfo.shmid = shmget(IPC_PRIVATE,
			 _image->bytes_per_line * _image->height,
			 IPC_CREAT|0777);

  if (shminfo.shmid == -1) {
    XDestroyImage(_image);
    return NULL;
  }

  shminfo.shmaddr = _image->data = shmat(shminfo.shmid, 0, 0);

  if (shminfo.shmaddr == (char *)-1) {
    XDestroyImage(_image);
    shmctl(shminfo.shmid, IPC_RMID, 0);
    return NULL;
  }

  shminfo.readOnly = True;

  oldXErrorHandler = XSetErrorHandler(ShmCreationXErrorHandler);
  XShmAttach(dpy, &shminfo);
  XSync(dpy, False);
  XSetErrorHandler(oldXErrorHandler);

  if (caughtShmError) {
    XDestroyImage(_image);
    shmdt(shminfo.shmaddr);
    shmctl(shminfo.shmid, IPC_RMID, 0);
    return NULL;
  }

  needShmCleanup = True;

  fprintf(stderr,"Using shared memory PutImage\n");

  return _image;
}

XImage *
CreateShmZoomImage()
{
  XImage *_image;
  XErrorHandler oldXErrorHandler;

  if (!XShmQueryExtension(dpy))
    return NULL;

  _image = XShmCreateImage(dpy, vis, visdepth, ZPixmap, NULL, &zoomshminfo,
			  si.framebufferWidth, si.framebufferHeight);
  if (!_image) return NULL;

  zoomshminfo.shmid = shmget(IPC_PRIVATE,
			     _image->bytes_per_line * _image->height,
			     IPC_CREAT|0777);

  if (zoomshminfo.shmid == -1) {
    XDestroyImage(_image);
    return NULL;
  }

  zoomshminfo.shmaddr = _image->data = shmat(zoomshminfo.shmid, 0, 0);

  if (zoomshminfo.shmaddr == (char *)-1) {
    XDestroyImage(_image);
    shmctl(zoomshminfo.shmid, IPC_RMID, 0);
    return NULL;
  }

  zoomshminfo.readOnly = True;

  oldXErrorHandler = XSetErrorHandler(ShmCreationXErrorHandler);
  XShmAttach(dpy, &zoomshminfo);
  XSync(dpy, False);
  XSetErrorHandler(oldXErrorHandler);

  if (caughtZoomShmError) {
    XDestroyImage(_image);
    shmdt(zoomshminfo.shmaddr);
    shmctl(zoomshminfo.shmid, IPC_RMID, 0);
    return NULL;
  }

  needZoomShmCleanup = True;

  fprintf(stderr,"Using shared memory PutImage\n");

  return _image;
}


/*
 * DrawZoomedScreenRegionX11Thread
 * Never call from any other desktop.c function, only for X11 thread
 */

void
DrawZoomedScreenRegionX11Thread(Window win, int zwidth, int zheight, 
				int x, int y, int width, int height) {
  if (zwidth > si.framebufferWidth)
    zwidth = si.framebufferWidth;
  if (zheight > si.framebufferHeight)
    zheight = si.framebufferHeight;
  
  if (!zoomActive) {
    ZoomInit();
    zoomActive = True;
  }
  
  if ((zoomWidth != zwidth) || (zoomHeight != zheight)) {
    Surface src, dest;

    zoomWidth = zwidth;
    zoomHeight = zheight;
  
    src.w = si.framebufferWidth;
    src.h = si.framebufferHeight;
    src.pitch = image->bytes_per_line;
    src.pixels = image->data;
    src.BytesPerPixel = myFormat.bitsPerPixel / 8;
    dest.w = zwidth;
    dest.h = zheight;
    dest.pitch = zoomImage->bytes_per_line;
    dest.pixels = zoomImage->data;
    dest.BytesPerPixel = myFormat.bitsPerPixel / 8;
    sge_transform(&src, &dest, 0, 
		  (float)dest.w/(float)src.w, (float)dest.h/(float)src.h,
		  0, 0, 0, 0);

    if (useZoomShm) 
      XShmPutImage(dpy, win, gc, zoomImage, 0, 0, 0, 0, zwidth, zheight, False);
    else
      XPutImage(dpy, win, gc, zoomImage, 0, 0, 0, 0, zwidth, zheight);
    return;
  }

  if (useZoomShm) 
    XShmPutImage(dpy, win, gc, zoomImage, x, y, x, y, width, height, False);
  else
    XPutImage(dpy, win, gc, zoomImage, x, y, x, y, width, height);
}


static void
ZoomInit()
{
  if (zoomImage)
    return;

   zoomImage = CreateShmZoomImage(); 

  if (!zoomImage) {
    useZoomShm = False;
    zoomImage = XCreateImage(dpy, vis, visdepth, ZPixmap, 0, NULL,
			     si.framebufferWidth, si.framebufferHeight,
			     BitmapPad(dpy), 0);

    zoomImage->data = calloc(zoomImage->bytes_per_line * zoomImage->height, 1);
    if (!zoomImage->data) {
      fprintf(stderr,"malloc failed\n");
      exit(1);
    }
  }
}

static void transformZoomSrc(int six, int siy, int siw, int sih,
			     int *dix, int *diy, int *diw, int *dih,
			     int srcW, int dstW, int srcH, int dstH) {
  double sx, sy, sw, sh;
  double dx, dy, dw, dh;
  double wq, hq;

  sx = six; sy = siy;
  sw = siw; sh = sih;

  wq = ((double)dstW) / (double) srcW;
  hq = ((double)dstH) / (double) srcH;

  dx = sx * wq;
  dy = sy * hq;
  dw = sw * wq;
  dh = sh * hq;
  
  *dix = dx;
  *diy = dy;
  *diw = dw+(dx-(int)dx)+0.5;
  *dih = dh+(dy-(int)dy)+0.5;
}

static void transformZoomDst(int *six, int *siy, int *siw, int *sih,
			     int dix, int diy, int diw, int dih,
			     int srcW, int dstW, int srcH, int dstH) {
  double sx, sy, sw, sh;
  double dx, dy, dw, dh;
  double wq, hq;

  dx = dix; dy = diy;
  dw = diw; dh = dih;

  wq = ((double)dstW) / (double) srcW;
  hq = ((double)dstH) / (double) srcH;

  sx = dx / wq;
  sy = dy / hq;
  sw = dw / wq;
  sh = dh / hq;
  
  *six = sx;
  *siy = sy;
  *siw = sw+(sx-(int)sx)+0.5;
  *sih = sh+(sy-(int)sy)+0.5;
}


static void ZoomSurfaceSrcCoords(int six, int siy, int siw, int sih,
				 int *dix, int *diy, int *diw, int *dih,
				 Surface * src, Surface * dst)
{
  int dx, dy, dw, dh;
  int sx, sy, sw, sh;

  transformZoomSrc(six, siy, siw, sih,
		   &dx, &dy, &dw, &dh,
		   src->w, dst->w, src->h, dst->h);
  dx-=2;
  dy-=2;
  dw+=4;
  dh+=4;

  if (dx < 0)
    dx = 0;
  if (dy < 0)
    dy = 0;
  if (dx+dw > dst->w)
    dw = dst->w - dx;
  if (dy+dh > dst->h)
    dh = dst->h - dy;
  
  transformZoomDst(&sx, &sy, &sw, &sh,
		   dx, dy, dw, dh,
		   src->w, dst->w, src->h, dst->h);

  if (sx+sw > src->w)
    sw = src->w - sx;
  if (sy+sh > src->h)
    sh = src->h - sy;

  ZoomSurfaceCoords32(sx, sy, sw, sh, dx, dy, src, dst);

  *dix = dx;
  *diy = dy;
  *diw = dw;
  *dih = dh;
}

static void ZoomSurfaceCoords32(int sx, int sy, int sw, int sh,
				int dx, int dy, 
				Surface * src, Surface * dst)
{
  Surface s2;

  s2 = *src;
  s2.pixels = ((char*)s2.pixels) + (sx * s2.BytesPerPixel) + (sy * src->pitch);
  s2.w = sw;
  s2.h = sh;
  sge_transform(&s2, dst, 0, 
		(float)dst->w/(float)src->w, (float)dst->h/(float)src->h,
		0, 0, dx, dy);
}


#define sge_clip_xmin(pnt) 0
#define sge_clip_xmax(pnt) pnt->w
#define sge_clip_ymin(pnt) 0
#define sge_clip_ymax(pnt) pnt->h

/*==================================================================================
// Helper function to sge_transform()
// Returns the bounding box
//==================================================================================
*/
static void _calcRect(Surface *src, Surface *dst, float theta, float xscale, float yscale, 
		      Uint16 px, Uint16 py, Uint16 qx, Uint16 qy, 
		      Sint16 *xmin, Sint16 *ymin, Sint16 *xmax, Sint16 *ymax)
{
	Sint16 x, y, rx, ry;
	int i;
	
	/* Clip to src surface */
	Sint16 sxmin = sge_clip_xmin(src);
	Sint16 sxmax = sge_clip_xmax(src);
	Sint16 symin = sge_clip_ymin(src);
	Sint16 symax = sge_clip_ymax(src);
	Sint16 sx[5];
	Sint16 sy[4];
	
	/* We don't really need fixed-point here
	 * but why not? */
	Sint32 istx = (Sint32)((sin(theta)*xscale) * 8192.0);  /* Inverse transform */
	Sint32 ictx = (Sint32)((cos(theta)*xscale) * 8192.2);
	Sint32 isty = (Sint32)((sin(theta)*yscale) * 8192.0);
	Sint32 icty = (Sint32)((cos(theta)*yscale) * 8192.2);

	sx[0] = sxmin;
	sx[1] = sxmax;
	sx[2] = sxmin;
	sx[3] = sxmax;
	sy[0] = symin;
	sy[1] = symax;
	sy[2] = symax;
	sy[3] = symin;

	/* Calculate the four corner points */
	for(i=0; i<4; i++){
		rx = sx[i] - px;
		ry = sy[i] - py;
		
		x = (Sint16)(((ictx*rx - isty*ry) >> 13) + qx);
		y = (Sint16)(((icty*ry + istx*rx) >> 13) + qy);
		
		
		if(i==0){
			*xmax = *xmin = x;
			*ymax = *ymin = y;
		}else{
			if(x>*xmax)
				*xmax=x;
			else if(x<*xmin)
				*xmin=x;
				
			if(y>*ymax)
				*ymax=y;
			else if(y<*ymin)
				*ymin=y;
		}
	}
	
	/* Better safe than sorry...*/
	*xmin -= 1;
	*ymin -= 1;
	*xmax += 1;
	*ymax += 1;
	
	/* Clip to dst surface */
	if( !dst )
		return;
	if( *xmin < sge_clip_xmin(dst) )
		*xmin = sge_clip_xmin(dst);
	if( *xmax > sge_clip_xmax(dst) )
		*xmax = sge_clip_xmax(dst);
	if( *ymin < sge_clip_ymin(dst) )
		*ymin = sge_clip_ymin(dst);
	if( *ymax > sge_clip_ymax(dst) )
		*ymax = sge_clip_ymax(dst);
}


/*==================================================================================
** Rotate by angle about pivot (px,py) scale by scale and place at
** position (qx,qy). 
** 
** Transformation matrix application (rotated coords "R"):
**
**   / rx \   /  cos(theta)  sin(theta) \  / dx \
**   |    | = |                         |  |    |
**   \ ry /   \ -sin(theta)  cos(theta) /  \ dy /
**
** =>  rx = cos(theta) dx + sin(theta) dy
**     ry = cos(theta) dy - sin(theta) dx 
** but represented as a fixed-point float using integer math
**
** Developed with the help from Terry Hancock (hancock@earthlink.net)
**
**==================================================================================*/
/* First we need some macros to handle different bpp
 *  I'm sorry about this... 
 */
#define TRANSFORM(UintXX, DIV) \
	Sint32 src_pitch=src->pitch/DIV; \
	Sint32 dst_pitch=dst->pitch/DIV; \
	UintXX *src_row = (UintXX *)src->pixels; \
	UintXX *dst_row; \
\
	for (y=ymin; y<ymax; y++){ \
		dy = y - qy; \
\
		sx = (Sint32)(ctdx  + stx*dy + mx);  /* Compute source anchor points */ \
		sy = (Sint32)(cty*dy - stdx  + my); \
\
		/* Calculate pointer to dst surface */ \
		dst_row = (UintXX *)dst->pixels + y*dst_pitch; \
\
		for (x=xmin; x<xmax; x++){ \
			rx=(Sint16)(sx >> 13);  /* Convert from fixed-point */ \
			ry=(Sint16)(sy >> 13); \
\
			/* Make sure the source pixel is actually in the source image. */ \
			if( (rx>=sxmin) && (rx<sxmax) && (ry>=symin) && (ry<symax) ) \
				*(dst_row + x) = *(src_row + ry*src_pitch + rx); \
\
			sx += ctx;  /* Incremental transformations */ \
			sy -= sty; \
		} \
	}
	
	
/* Interpolated transform */
#define TRANSFORM_AA(UintXX, DIV) \
	Sint32 src_pitch=src->pitch/DIV; \
	Sint32 dst_pitch=dst->pitch/DIV; \
	UintXX *src_row = (UintXX *)src->pixels; \
	UintXX *dst_row; \
	UintXX c1, c2, c3, c4;\
	Uint32 R, G, B, A=0; \
	UintXX Rmask = myFormat.redMax << myFormat.redShift;\
        UintXX Gmask = myFormat.greenMax << myFormat.greenShift;\
        UintXX Bmask = myFormat.blueMax << myFormat.blueShift;\
        UintXX Amask = 0;\
	Uint32 wx, wy;\
	Uint32 p1, p2, p3, p4;\
\
	/* 
	*  Interpolation:
	*  We calculate the distances from our point to the four nearest pixels, d1..d4.
	*  d(a,b) = sqrt(a²+b²) ~= 0.707(a+b)  (Pythagoras (Taylor) expanded around (0.5;0.5))
	*  
	*    1  wx 2
	*     *-|-*  (+ = our point at (x,y))
	*     | | |  (* = the four nearest pixels)
	*  wy --+ |  wx = float(x) - int(x)
	*     |   |  wy = float(y) - int(y)
	*     *---*
	*    3     4
	*  d1 = d(wx,wy)  d2 = d(1-wx,wy)  d3 = d(wx,1-wy)  d4 = d(1-wx,1-wy)
	*  We now want to weight each pixels importance - it's vicinity to our point:
	*  w1=d4  w2=d3  w3=d2  w4=d1  (Yes it works... just think a bit about it)
	*
	*  If the pixels have the colors c1..c4 then our point should have the color
	*  c = (w1*c1 + w2*c2 + w3*c3 + w4*c4)/(w1+w2+w3+w4)   (the weighted average)
	*  but  w1+w2+w3+w4 = 4*0.707  so we might as well write it as
	*  c = p1*c1 + p2*c2 + p3*c3 + p4*c4  where  p1..p4 = (w1..w4)/(4*0.707)
	*
	*  But p1..p4 are fixed point so we can just divide the fixed point constant!
	*  8192/(4*0.71) = 2897  and we can skip 0.71 too (the division will cancel it everywhere)
	*  8192/4 = 2048
	*
	*  020102: I changed the fixed-point representation for the variables in the weighted average
	*          to 24.7 to avoid problems with 32bit colors. Everything else is still 18.13. This
	*          does however not solve the problem with 32bit RGBA colors... 
	*/\
\
	Sint32 one = 2048>>6;   /* 1 in Fixed-point */ \
	Sint32 two = 2*2048>>6; /* 2 in Fixed-point */ \
\
	for (y=ymin; y<ymax; y++){ \
		dy = y - qy; \
\
		sx = (Sint32)(ctdx  + stx*dy + mx);  /* Compute source anchor points */ \
		sy = (Sint32)(cty*dy - stdx  + my); \
\
		/* Calculate pointer to dst surface */ \
		dst_row = (UintXX *)dst->pixels + y*dst_pitch; \
\
		for (x=xmin; x<xmax; x++){ \
			rx=(Sint16)(sx >> 13);  /* Convert from fixed-point */ \
			ry=(Sint16)(sy >> 13); \
\
			/* Make sure the source pixel is actually in the source image. */ \
			if( (rx>=sxmin) && (rx+1<sxmax) && (ry>=symin) && (ry+1<symax) ){ \
				wx = (sx & 0x00001FFF) >>8;  /* (float(x) - int(x)) / 4 */ \
				wy = (sy & 0x00001FFF) >>8;\
\
				p4 = wx+wy;\
				p3 = one-wx+wy;\
				p2 = wx+one-wy;\
				p1 = two-wx-wy;\
\
				c1 = *(src_row + ry*src_pitch + rx);\
				c2 = *(src_row + ry*src_pitch + rx+1);\
				c3 = *(src_row + (ry+1)*src_pitch + rx);\
				c4 = *(src_row + (ry+1)*src_pitch + rx+1);\
\
				/* Calculate the average */\
				R = ((p1*(c1 & Rmask) + p2*(c2 & Rmask) + p3*(c3 & Rmask) + p4*(c4 & Rmask))>>7) & Rmask;\
				G = ((p1*(c1 & Gmask) + p2*(c2 & Gmask) + p3*(c3 & Gmask) + p4*(c4 & Gmask))>>7) & Gmask;\
				B = ((p1*(c1 & Bmask) + p2*(c2 & Bmask) + p3*(c3 & Bmask) + p4*(c4 & Bmask))>>7) & Bmask;\
				if(Amask)\
					A = ((p1*(c1 & Amask) + p2*(c2 & Amask) + p3*(c3 & Amask) + p4*(c4 & Amask))>>7) & Amask;\
				\
				*(dst_row + x) = R | G | B | A;\
			} \
			sx += ctx;  /* Incremental transformations */ \
			sy -= sty; \
		} \
	} 

void sge_transform(Surface *src, Surface *dst, float angle, float xscale, float yscale ,Uint16 px, Uint16 py, Uint16 qx, Uint16 qy)
{
	Sint32 dy, sx, sy;
	Sint16 x, y, rx, ry;
	Rect r;
	float theta = (float)(angle*M_PI/180.0);  /* Convert to radians.  */

	Sint32 stx, ctx, sty, cty, mx, my;
	Sint16 xmin, xmax, ymin, ymax;
	Sint16 sxmin, sxmax, symin, symax;
	Sint32 dx, ctdx, stdx;


	/* Here we use 18.13 fixed point integer math
	// Sint32 should have 31 usable bits and one for sign
	// 2^13 = 8192
	*/

	/* Check scales */
	Sint32 maxint = (Sint32)(pow(2, sizeof(Sint32)*8 - 1 - 13));  /* 2^(31-13) */

	r.x = r.y = r.w = r.h = 0;
	
	if( xscale == 0 || yscale == 0)
		return;
		
	if( 8192.0/xscale > maxint )
		xscale =  (float)(8192.0/maxint);
	else if( 8192.0/xscale < -maxint )
		xscale =  (float)(-8192.0/maxint);	
		
	if( 8192.0/yscale > maxint )
		yscale =  (float)(8192.0/maxint);
	else if( 8192.0/yscale < -maxint )
		yscale =  (float)(-8192.0/maxint);


	/* Fixed-point equivalents */
	stx = (Sint32)((sin(theta)/xscale) * 8192.0);
	ctx = (Sint32)((cos(theta)/xscale) * 8192.0);
	sty = (Sint32)((sin(theta)/yscale) * 8192.0);
	cty = (Sint32)((cos(theta)/yscale) * 8192.0);
	mx = (Sint32)(px*8192.0); 
	my = (Sint32)(py*8192.0);

	/* Compute a bounding rectangle */
	xmin=0; xmax=dst->w; ymin=0; ymax=dst->h;
	_calcRect(src, dst, theta, xscale, yscale, 
		  px, py, qx, qy, &xmin, &ymin, &xmax, &ymax);	

	/* Clip to src surface */
	sxmin = sge_clip_xmin(src);
	sxmax = sge_clip_xmax(src);
	symin = sge_clip_ymin(src);
	symax = sge_clip_ymax(src);

	/* Some terms in the transform are constant */
	dx = xmin - qx;
	ctdx = ctx*dx;
	stdx = sty*dx;
	
	/* Use the correct bpp */
	if( src->BytesPerPixel == dst->BytesPerPixel){
		switch( src->BytesPerPixel ){
			case 1: { /* Assuming 8-bpp */
				TRANSFORM(Uint8, 1)
			}
			break;
			case 2: { /* Probably 15-bpp or 16-bpp */
				TRANSFORM_AA(Uint16, 2)
			}
			break;
			case 4: { /* Probably 32-bpp */
				TRANSFORM_AA(Uint32, 4)
			}
			break;
		}
	}
}

void freeDesktopResources() {
  Cleanup();
  if (image) {
    if (!useShm)
      free(image->data);
    XDestroyImage(image);
  }
  if (zoomImage) {
    if(!useZoomShm)
      free(zoomImage->data);
    XDestroyImage(zoomImage);
  }

  XFreeGC(dpy, srcGC);
  XFreeGC(dpy, dstGC);
}


/*
 * ColorRectangle32
 * Only used for debugging / visualizing output
 */
/*
static void
ColorRectangle32(XImage *img, CARD32 fg, int x, int y, int width, int height)
{
  int i, h;
  int scrWidthInBytes = img->bytes_per_line;
  char *scr;
  CARD32 *scr32;

  if ((!img) || (!img->data))
    return;

  scr = (img->data + y * scrWidthInBytes + x * 4);
    
  if (!CheckRectangle(x, y, width, height))
    return;

  for (h = 0; h < height; h++) {
    scr32 = (CARD32*) scr;
    for (i = 0; i < width; i++) {
      CARD32 n = 0;
      CARD32 p = scr32[i];
      if (0xff & fg)
	n |= (((    0xff & p)+(    0xff & fg)) >> 2) & 0xff;
      else
	n |= (0xff & p);
      if (0xff00 & fg)
	n |= (((  0xff00 & p)+(  0xff00 & fg)) >> 2) & 0xff00;
      else
	n |= (0xff00 & p);
      if (0xff0000 & fg)
	n |= (((0xff0000 & p)+(0xff0000 & fg)) >> 2) & 0xff0000;
      else
	n |= (0xff0000 & p);
      scr32[i] = n;
    }
    scr += scrWidthInBytes;
  }
}
*/

