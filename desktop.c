/*
 *  Copyright (C) 1999 AT&T Laboratories Cambridge.  All Rights Reserved.
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
 * 03-05-2002 tim@tjansen.de: removed stuff for krdc, merged with shm.c 
 *                            and misc.c
 *
 */

/*
 * desktop.c - functions to deal with "desktop" window.
 */

#include <X11/Xlib.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <X11/extensions/XShm.h>
#include "vncviewer.h"

static XShmSegmentInfo shminfo;

static Bool caughtShmError = False;
static Bool needShmCleanup = False;

GC gc;
GC srcGC, dstGC; /* used for debugging copyrect */
Window desktopWin;
Cursor dotCursor;
Dimension dpyWidth, dpyHeight;

static XImage *image = NULL;

Bool useShm = True;

static Cursor CreateDotCursor(void);
static void CopyBGR233ToScreen(CARD8 *buf, int x, int y, int width,int height);
static void FillRectangleBGR233(CARD8 buf, int x, int y, int width,int height);
static int CheckRectangle(int x, int y, int width, int height);

void
DesktopInit(Window win)
{
  XGCValues gcv;
  XSetWindowAttributes attr;

/*  image = CreateShmImage();*/

  if (!image) {
    useShm = False;
    image = XCreateImage(dpy, vis, visdepth, ZPixmap, 0, NULL,
			 si.framebufferWidth, si.framebufferHeight,
			 BitmapPad(dpy), 0);

    image->data = malloc(image->bytes_per_line * image->height);
    if (!image->data) {
      fprintf(stderr,"malloc failed\n");
      exit(1);
    }
  }

  desktopWin = win;

  gc = XCreateGC(dpy,desktopWin,0,NULL);

  gcv.function = GXxor;
  gcv.foreground = 0x0f0f0f0f;
  srcGC = XCreateGC(dpy,desktopWin,GCFunction|GCForeground,&gcv);
  gcv.foreground = 0xf0f0f0f0;
  dstGC = XCreateGC(dpy,desktopWin,GCFunction|GCForeground,&gcv);

  dotCursor = CreateDotCursor();
  attr.cursor = dotCursor;

  XChangeWindowAttributes(dpy, desktopWin, CWBackingStore|CWCursor, &attr);
}


/*
 * HandleBasicDesktopEvent - deal with expose and leave events.
 */
/*
static void
HandleBasicDesktopEvent(Widget w, XtPointer ptr, XEvent *ev, Boolean *cont)
{
  int i;

  switch (ev->type) {

  case Expose:
  case GraphicsExpose:
  /// sometimes due to scrollbars being added/removed we get an expose outside
  //    the actual desktop area.  Make sure we don't pass it on to the RFB
  //    server. 

    if (ev->xexpose.x + ev->xexpose.width > si.framebufferWidth) {
      ev->xexpose.width = si.framebufferWidth - ev->xexpose.x;
      if (ev->xexpose.width <= 0) break;
    }

    if (ev->xexpose.y + ev->xexpose.height > si.framebufferHeight) {
      ev->xexpose.height = si.framebufferHeight - ev->xexpose.y;
      if (ev->xexpose.height <= 0) break;
    }

    SendFramebufferUpdateRequest(ev->xexpose.x, ev->xexpose.y,
				 ev->xexpose.width, ev->xexpose.height, False);
    break;

  case LeaveNotify:
    for (i = 0; i < 256; i++) {
      if (modifierPressed[i]) {
	SendKeyEvent(XKeycodeToKeysym(dpy, i, 0), False);
	modifierPressed[i] = False;
      }
    }
    break;
  }
}
*/

/*
 * SendRFBEvent is an action which sends an RFB event.  It can be used in two
 * ways.  Without any parameters it simply sends an RFB event corresponding to
 * the X event which caused it to be called.  With parameters, it generates a
 * "fake" RFB event based on those parameters.  The first parameter is the
 * event type, either "ptr", "keydown", "keyup" or "key" (down&up).  For a
 * "key" event the second parameter is simply a keysym string as understood by
 * XStringToKeysym().  For a "ptr" event, the following three parameters are
 * just X, Y and the button mask (0 for all up, 1 for button1 down, 2 for
 * button2 down, 3 for both, etc).
 */
/*
void
SendRFBEvent(XEvent *ev, String *params, Cardinal *num_params)
{
  KeySym ks;
  char keyname[256];
  int buttonMask, x, y;

  if (appData.fullScreen && ev->type == MotionNotify) {
    if (BumpScroll(ev))
      return;
  }

  if (appData.viewOnly) return;

  if (*num_params != 0) {
    if (strncasecmp(params[0],"key",3) == 0) {
      if (*num_params != 2) {
	fprintf(stderr,
		"Invalid params: SendRFBEvent(key|keydown|keyup,<keysym>)\n");
	return;
      }
      ks = XStringToKeysym(params[1]);
      if (ks == NoSymbol) {
	fprintf(stderr,"Invalid keysym '%s' passed to SendRFBEvent\n",
		params[1]);
	return;
      }
      if (strcasecmp(params[0],"keydown") == 0) {
	SendKeyEvent(ks, 1);
      } else if (strcasecmp(params[0],"keyup") == 0) {
	SendKeyEvent(ks, 0);
      } else if (strcasecmp(params[0],"key") == 0) {
	SendKeyEvent(ks, 1);
	SendKeyEvent(ks, 0);
      } else {
	fprintf(stderr,"Invalid event '%s' passed to SendRFBEvent\n",
		params[0]);
	return;
      }
    } else if (strcasecmp(params[0],"ptr") == 0) {
      if (*num_params == 4) {
	x = atoi(params[1]);
	y = atoi(params[2]);
	buttonMask = atoi(params[3]);
	SendPointerEvent(x, y, buttonMask);
      } else if (*num_params == 2) {
	switch (ev->type) {
	case ButtonPress:
	case ButtonRelease:
	  x = ev->xbutton.x;
	  y = ev->xbutton.y;
	  break;
	case KeyPress:
	case KeyRelease:
	  x = ev->xkey.x;
	  y = ev->xkey.y;
	  break;
	default:
	  fprintf(stderr,
		  "Invalid event caused SendRFBEvent(ptr,<buttonMask>)\n");
	  return;
	}
	buttonMask = atoi(params[1]);
	SendPointerEvent(x, y, buttonMask);
      } else {
	fprintf(stderr,
		"Invalid params: SendRFBEvent(ptr,<x>,<y>,<buttonMask>)\n"
		"             or SendRFBEvent(ptr,<buttonMask>)\n");
	return;
      }

    } else {
      fprintf(stderr,"Invalid event '%s' passed to SendRFBEvent\n", params[0]);
    }
    return;
  }

  switch (ev->type) {

  case MotionNotify:
    while (XCheckTypedWindowEvent(dpy, desktopWin, MotionNotify, ev))
      ;	// discard all queued motion notify events 

    SendPointerEvent(ev->xmotion.x, ev->xmotion.y,
		     (ev->xmotion.state & 0x1f00) >> 8);
    return;

  case ButtonPress:
    SendPointerEvent(ev->xbutton.x, ev->xbutton.y,
		     (((ev->xbutton.state & 0x1f00) >> 8) |
		      (1 << (ev->xbutton.button - 1))));
    return;

  case ButtonRelease:
    SendPointerEvent(ev->xbutton.x, ev->xbutton.y,
		     (((ev->xbutton.state & 0x1f00) >> 8) &
		      ~(1 << (ev->xbutton.button - 1))));
    return;

  case KeyPress:
  case KeyRelease:
    XLookupString(&ev->xkey, keyname, 256, &ks, NULL);

    if (IsModifierKey(ks)) {
      ks = XKeycodeToKeysym(dpy, ev->xkey.keycode, 0);
      modifierPressed[ev->xkey.keycode] = (ev->type == KeyPress);
    }

    SendKeyEvent(ks, (ev->type == KeyPress));
    return;

  default:
    fprintf(stderr,"Invalid event passed to SendRFBEvent\n");
  }
}
*/

/*
 * CreateDotCursor.
 */

static Cursor
CreateDotCursor()
{
  Cursor cursor;
  Pixmap src, msk;
  static char srcBits[] = { 0, 14,14,14, 0 };
  static char mskBits[] = { 14,31,31,31,14 };
  XColor fg, bg;

  src = XCreateBitmapFromData(dpy, DefaultRootWindow(dpy), srcBits, 5, 5);
  msk = XCreateBitmapFromData(dpy, DefaultRootWindow(dpy), mskBits, 5, 5);
  XAllocNamedColor(dpy, DefaultColormap(dpy,DefaultScreen(dpy)), "black",
		   &fg, &fg);
  XAllocNamedColor(dpy, DefaultColormap(dpy,DefaultScreen(dpy)), "white",
		   &bg, &bg);
  cursor = XCreatePixmapCursor(dpy, src, msk, &fg, &bg, 2, 2);
  XFreePixmap(dpy, src);
  XFreePixmap(dpy, msk);

  return cursor;
}

/*
 * SyncScreenRegion
 */
void
SyncScreenRegion(int x, int y, int width, int height) {
  lockQt();
  if (useShm) 
    XShmPutImage(dpy, desktopWin, gc, image, x, y, x, y, width, height, False);
  else
    XPutImage(dpy, desktopWin, gc, image, x, y, x, y, width, height);
  unlockQt();
}

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
  if (appData.rawDelay != 0) {
    lockQt();
    XFillRectangle(dpy, desktopWin, gc, x, y, width, height);
    XSync(dpy,False);
    unlockQt();

    usleep(appData.rawDelay * 1000);
  }

  if (!appData.useBGR233) {
    int h;
    int widthInBytes = width * myFormat.bitsPerPixel / 8;
    int scrWidthInBytes = si.framebufferWidth * myFormat.bitsPerPixel / 8;

    char *scr = (image->data + y * scrWidthInBytes
		 + x * myFormat.bitsPerPixel / 8);

    for (h = 0; h < height; h++) {
      memcpy(scr, buf, widthInBytes);
      buf += widthInBytes;
      scr += scrWidthInBytes;
    }
  } else {
    CopyBGR233ToScreen((CARD8 *)buf, x, y, width, height);
  }

  SyncScreenRegion(x, y, width, height);
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
    int scrWidthInBytes = si.framebufferWidth * myFormat.bitsPerPixel / 8;

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
  int scrWidthInBytes = si.framebufferWidth * myFormat.bitsPerPixel / 8;
  
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
  int scrWidthInBytes = si.framebufferWidth * myFormat.bitsPerPixel / 8;
  
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
 * CopyArea
 */

/*void
CopyArea(int srcX, int srcY, int x, int y, int width, int height)
{
  int h;
  int widthInBytes = width * myFormat.bitsPerPixel / 8;
  int scrWidthInBytes = si.framebufferWidth * myFormat.bitsPerPixel / 8;
  
  char *scr = (image->data + y * scrWidthInBytes
	       + x * myFormat.bitsPerPixel / 8);

  if ((srcY != y) ||
      ())
  
  for (h = 0; h < height; h++) {
    memcpy(scr, buf, widthInBytes);
    buf += widthInBytes;
    scr += scrWidthInBytes;
  }

  SyncScreenRegion(x, y, width, height);
}
*/

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
  if (useShm)
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
