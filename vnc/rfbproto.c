/*
 *  Copyright (C) 2002, Tim Jansen.  All Rights Reserved.
 *  Copyright (C) 2000-2002 Constantin Kaplinsky.  All Rights Reserved.
 *  Copyright (C) 2000 Tridia Corporation.  All Rights Reserved.
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
 */

/*
 * rfbproto.c - functions to deal with client side of RFB protocol.
 * tim@tjansen.de: - added softcursor encoding
 *                 - changed various things for krdc
 */

#include <unistd.h>
#include <errno.h>
#include <pwd.h>
#include "vncviewer.h"
#include "vncauth.h"
#include <zlib.h>
#include <jpeglib.h>

static Bool HandleHextile8(int rx, int ry, int rw, int rh);
static Bool HandleHextile16(int rx, int ry, int rw, int rh);
static Bool HandleHextile32(int rx, int ry, int rw, int rh);
static Bool HandleZlib8(int rx, int ry, int rw, int rh);
static Bool HandleZlib16(int rx, int ry, int rw, int rh);
static Bool HandleZlib32(int rx, int ry, int rw, int rh);
static Bool HandleTight8(int rx, int ry, int rw, int rh);
static Bool HandleTight16(int rx, int ry, int rw, int rh);
static Bool HandleTight32(int rx, int ry, int rw, int rh);

static long ReadCompactLen (void);

static void JpegInitSource(j_decompress_ptr cinfo);
static boolean JpegFillInputBuffer(j_decompress_ptr cinfo);
static void JpegSkipInputData(j_decompress_ptr cinfo, long num_bytes);
static void JpegTermSource(j_decompress_ptr cinfo);
static void JpegSetSrcManager(j_decompress_ptr cinfo, CARD8 *compressedData,
                              int compressedLen);


#define RGB24_TO_PIXEL(bpp,r,g,b)                                       \
   ((((CARD##bpp)(r) & 0xFF) * myFormat.redMax + 127) / 255             \
    << myFormat.redShift |                                              \
    (((CARD##bpp)(g) & 0xFF) * myFormat.greenMax + 127) / 255           \
    << myFormat.greenShift |                                            \
    (((CARD##bpp)(b) & 0xFF) * myFormat.blueMax + 127) / 255            \
    << myFormat.blueShift)

int rfbsock;
char *desktopName;
rfbPixelFormat myFormat;
rfbServerInitMsg si;

int endianTest = 1;

/*
 * Softcursor variables
 */

int cursorX, cursorY;
int imageIndex = -1;

PointerImage pointerImages[rfbSoftCursorMaxImages];


/*  Hextile assumes it is big enough to hold 16 * 16 * 32 bits.
   Tight encoding assumes BUFFER_SIZE is at least 16384 bytes. */

#define BUFFER_SIZE (16384)
static char buffer[BUFFER_SIZE];


/* The zlib encoding requires expansion/decompression/deflation of the
   compressed data in the "buffer" above into another, result buffer.
   However, the size of the result buffer can be determined precisely
   based on the bitsPerPixel, height and width of the rectangle.  We
   allocate this buffer one time to be the full size of the buffer. */

static int raw_buffer_size = -1;
static char *raw_buffer = NULL;

static z_stream decompStream;
static Bool decompStreamInited = False;


/*
 * Variables for the ``tight'' encoding implementation.
 */

/* Separate buffer for compressed data. */
#define ZLIB_BUFFER_SIZE 512
static char zlib_buffer[ZLIB_BUFFER_SIZE];

/* Four independent compression streams for zlib library. */
static z_stream zlibStream[4];
static Bool zlibStreamActive[4] = {
  False, False, False, False
};

/* Filter stuff. Should be initialized by filter initialization code. */
static Bool cutZeros;
static int rectWidth, rectColors;
static char tightPalette[256*4];
static CARD8 tightPrevRow[2048*3*sizeof(CARD16)];

/* JPEG decoder state. */
static Bool jpegError;

/* Maximum length for the cut buffer (16 MB)*/
#define MAX_CUTBUFFER (1024*1024*16)

/* Maximum length for the strings (64 kB)*/
#define MAX_STRING (1024*64)

/* Maximum length for the strings (32 MB)*/
#define MAX_JPEG_SIZE (1024*1024*32)


/*
 * ConnectToRFBServer.
 */

int
ConnectToRFBServer(const char *hostname, int port)
{
  unsigned int host;

  if (!StringToIPAddr(hostname, &host)) {
    fprintf(stderr,"Couldn't convert '%s' to host address\n", hostname);
    return -(int)INIT_NAME_RESOLUTION_FAILURE;
  }

  rfbsock = ConnectToTcpAddr(host, port);
  if (rfbsock < 0) {
    fprintf(stderr,"Unable to connect to VNC server\n");
  }

  return rfbsock;
}


/*
 * InitialiseRFBConnection.
 */

enum InitStatus
InitialiseRFBConnection()
{
  rfbProtocolVersionMsg pv;
  int major,minor;
  CARD32 authScheme, reasonLen, authResult;
  char *reason;
  CARD8 challenge[CHALLENGESIZE];
  char passwd[9];
  int i;
  rfbClientInitMsg ci;

  /* if the connection is immediately closed, don't report anything, so
       that pmw's monitor can make test connections */

  if (!ReadFromRFBServer(pv, sz_rfbProtocolVersionMsg)) return INIT_SERVER_BLOCKED;

  errorMessageOnReadFailure = True;

  pv[sz_rfbProtocolVersionMsg] = 0;

  if (sscanf(pv,rfbProtocolVersionFormat,&major,&minor) != 2) {
    fprintf(stderr,"Not a valid VNC server\n");
    return INIT_PROTOCOL_FAILURE;
  }

  fprintf(stderr,"VNC server supports protocol version %d.%d (viewer %d.%d)\n",
	  major, minor, rfbProtocolMajorVersion, rfbProtocolMinorVersion);

  major = rfbProtocolMajorVersion;
  minor = rfbProtocolMinorVersion;

  sprintf(pv,rfbProtocolVersionFormat,major,minor);

  if (!WriteExact(rfbsock, pv, sz_rfbProtocolVersionMsg)) return INIT_CONNECTION_FAILED;

  if (!ReadFromRFBServer((char *)&authScheme, 4)) return INIT_CONNECTION_FAILED;

  authScheme = Swap32IfLE(authScheme);

  switch (authScheme) {

  case rfbConnFailed:
    if (!ReadFromRFBServer((char *)&reasonLen, 4)) return INIT_CONNECTION_FAILED;
    reasonLen = Swap32IfLE(reasonLen);

    if (reasonLen > MAX_STRING) {
      fprintf(stderr, "Connection failure reason too long.\n");
      return INIT_CONNECTION_FAILED;
    }
      
    reason = malloc(reasonLen);
    if (!reason)
      return INIT_CONNECTION_FAILED;
      
    if (!ReadFromRFBServer(reason, reasonLen)) return INIT_CONNECTION_FAILED;

    fprintf(stderr,"VNC connection failed: %.*s\n",(int)reasonLen, reason);
    free(reason);
    return INIT_CONNECTION_FAILED;

  case rfbNoAuth:
    fprintf(stderr,"No authentication needed\n");
    break;

  case rfbVncAuth:
    if (!ReadFromRFBServer((char *)challenge, CHALLENGESIZE)) return INIT_CONNECTION_FAILED;

    if (!getPassword(passwd, 8))
      return INIT_ABORTED;

    passwd[8] = '\0';

    vncEncryptBytes(challenge, passwd);

	/* Lose the password from memory */
    for (i = strlen(passwd); i >= 0; i--) {
      passwd[i] = '\0';
    }

    if (!WriteExact(rfbsock, (char *)challenge, CHALLENGESIZE)) return INIT_CONNECTION_FAILED;

    if (!ReadFromRFBServer((char *)&authResult, 4)) return INIT_CONNECTION_FAILED;

    authResult = Swap32IfLE(authResult);

    switch (authResult) {
    case rfbVncAuthOK:
      fprintf(stderr,"VNC authentication succeeded\n");
      break;
    case rfbVncAuthFailed:
      fprintf(stderr,"VNC authentication failed\n");
      return INIT_AUTHENTICATION_FAILED;
    case rfbVncAuthTooMany:
      fprintf(stderr,"VNC authentication failed - too many tries\n");
      return INIT_AUTHENTICATION_FAILED;
    default:
      fprintf(stderr,"Unknown VNC authentication result: %d\n",
	      (int)authResult);
      return INIT_CONNECTION_FAILED;
    }
    break;

  default:
    fprintf(stderr,"Unknown authentication scheme from VNC server: %d\n",
	    (int)authScheme);
    return INIT_CONNECTION_FAILED;
  }

  ci.shared = (appData.shareDesktop ? 1 : 0);

  if (!WriteExact(rfbsock, (char *)&ci, sz_rfbClientInitMsg)) return INIT_CONNECTION_FAILED;

  if (!ReadFromRFBServer((char *)&si, sz_rfbServerInitMsg)) return INIT_CONNECTION_FAILED;

  si.framebufferWidth = Swap16IfLE(si.framebufferWidth);
  si.framebufferHeight = Swap16IfLE(si.framebufferHeight);
  si.format.redMax = Swap16IfLE(si.format.redMax);
  si.format.greenMax = Swap16IfLE(si.format.greenMax);
  si.format.blueMax = Swap16IfLE(si.format.blueMax);
  si.nameLength = Swap32IfLE(si.nameLength);

  if ((si.framebufferWidth*si.framebufferHeight) > (4096*4096))
    return INIT_CONNECTION_FAILED;

  if (si.nameLength > MAX_STRING) {
    fprintf(stderr, "Display name too long.\n");  
    return INIT_CONNECTION_FAILED;
  }

  desktopName = malloc(si.nameLength + 1);
  if (!desktopName) {
    fprintf(stderr, "Error allocating memory for desktop name, %lu bytes\n",
            (unsigned long)si.nameLength);
    return INIT_CONNECTION_FAILED;
  }

  if (!ReadFromRFBServer(desktopName, si.nameLength)) return INIT_CONNECTION_FAILED;

  desktopName[si.nameLength] = 0;

  fprintf(stderr,"Desktop name \"%s\"\n",desktopName);

  fprintf(stderr,"Connected to VNC server, using protocol version %d.%d\n",
	  rfbProtocolMajorVersion, rfbProtocolMinorVersion);

  fprintf(stderr,"VNC server default format:\n");
  PrintPixelFormat(&si.format);

  return INIT_OK;
}


/*
 * SetFormatAndEncodings.
 */

Bool
SetFormatAndEncodings()
{
  rfbSetPixelFormatMsg spf;
  char buf[sz_rfbSetEncodingsMsg + MAX_ENCODINGS * 4];
  rfbSetEncodingsMsg *se = (rfbSetEncodingsMsg *)buf;
  CARD32 *encs = (CARD32 *)(&buf[sz_rfbSetEncodingsMsg]);
  int len = 0;
  Bool requestCompressLevel = False;
  Bool requestQualityLevel = False;
  Bool requestLastRectEncoding = False;

  spf.type = rfbSetPixelFormat;
  spf.pad1 = 0;
  spf.pad2 = 0;
  spf.format = myFormat;
  spf.format.redMax = Swap16IfLE(spf.format.redMax);
  spf.format.greenMax = Swap16IfLE(spf.format.greenMax);
  spf.format.blueMax = Swap16IfLE(spf.format.blueMax);

  if (!WriteExact(rfbsock, (char *)&spf, sz_rfbSetPixelFormatMsg))
    return False;

  se->type = rfbSetEncodings;
  se->pad = 0;
  se->nEncodings = 0;

  if (appData.encodingsString) {
    const char *encStr = appData.encodingsString;
    int encStrLen;
    do {
      char *nextEncStr = strchr(encStr, ' ');
      if (nextEncStr) {
	encStrLen = nextEncStr - encStr;
	nextEncStr++;
      } else {
	encStrLen = strlen(encStr);
      }

      if (strncasecmp(encStr,"raw",encStrLen) == 0) {
	encs[se->nEncodings++] = Swap32IfLE(rfbEncodingRaw);
      } else if (strncasecmp(encStr,"copyrect",encStrLen) == 0) {
	encs[se->nEncodings++] = Swap32IfLE(rfbEncodingCopyRect);
      } else if (strncasecmp(encStr,"softcursor",encStrLen) == 0) {
	encs[se->nEncodings++] = Swap32IfLE(rfbEncodingSoftCursor);
        /* if server supports SoftCursor, it will ignore X/RichCursor 
	 * and PointerPos */
        encs[se->nEncodings++] = Swap32IfLE(rfbEncodingXCursor);
        encs[se->nEncodings++] = Swap32IfLE(rfbEncodingRichCursor);
        encs[se->nEncodings++] = Swap32IfLE(rfbEncodingPointerPos);
      } else if (strncasecmp(encStr,"background",encStrLen) == 0) {
	encs[se->nEncodings++] = Swap32IfLE(rfbEncodingBackground);
      } else if (strncasecmp(encStr,"tight",encStrLen) == 0) {
	encs[se->nEncodings++] = Swap32IfLE(rfbEncodingTight);
	requestLastRectEncoding = True;
	if (appData.compressLevel >= 0 && appData.compressLevel <= 9)
	  requestCompressLevel = True;
	if (appData.qualityLevel >= 0 && appData.qualityLevel <= 9)
	  requestQualityLevel = True;
      } else if (strncasecmp(encStr,"hextile",encStrLen) == 0) {
	encs[se->nEncodings++] = Swap32IfLE(rfbEncodingHextile);
      } else if (strncasecmp(encStr,"zlib",encStrLen) == 0) {
	encs[se->nEncodings++] = Swap32IfLE(rfbEncodingZlib);
	if (appData.compressLevel >= 0 && appData.compressLevel <= 9)
	  requestCompressLevel = True;
      } else {
	fprintf(stderr,"Unknown encoding '%.*s'\n",encStrLen,encStr);
      }

      encStr = nextEncStr;
    } while (encStr && se->nEncodings < MAX_ENCODINGS);

    if (se->nEncodings < MAX_ENCODINGS && requestCompressLevel) {
      encs[se->nEncodings++] = Swap32IfLE(appData.compressLevel +
					  rfbEncodingCompressLevel0);
    }

    if (se->nEncodings < MAX_ENCODINGS && requestQualityLevel) {
      encs[se->nEncodings++] = Swap32IfLE(appData.qualityLevel +
					  rfbEncodingQualityLevel0);
    }

    if (se->nEncodings < MAX_ENCODINGS && requestLastRectEncoding) {
      encs[se->nEncodings++] = Swap32IfLE(rfbEncodingLastRect);
    }
  }
  else {
    encs[se->nEncodings++] = Swap32IfLE(rfbEncodingCopyRect);
    encs[se->nEncodings++] = Swap32IfLE(rfbEncodingTight);
    encs[se->nEncodings++] = Swap32IfLE(rfbEncodingHextile);
    encs[se->nEncodings++] = Swap32IfLE(rfbEncodingZlib);
    encs[se->nEncodings++] = Swap32IfLE(rfbEncodingRaw);

    if (appData.compressLevel >= 0 && appData.compressLevel <= 9) {
      encs[se->nEncodings++] = Swap32IfLE(appData.compressLevel +
					  rfbEncodingCompressLevel0);
    } 

    if (appData.qualityLevel >= 0 && appData.qualityLevel <= 9) {
      encs[se->nEncodings++] = Swap32IfLE(appData.qualityLevel +
					  rfbEncodingQualityLevel0);
    }

    if (si.format.depth >= 8) 
      encs[se->nEncodings++] = Swap32IfLE(rfbEncodingSoftCursor);
    encs[se->nEncodings++] = Swap32IfLE(rfbEncodingLastRect);
  }

  len = sz_rfbSetEncodingsMsg + se->nEncodings * 4;

  se->nEncodings = Swap16IfLE(se->nEncodings);

  if (!WriteExact(rfbsock, buf, len)) return False;

  return True;
}


/*
 * SendIncrementalFramebufferUpdateRequest.
 * Note: this should only be called by the WriterThread
 */

Bool
SendIncrementalFramebufferUpdateRequest()
{
  return SendFramebufferUpdateRequest(0, 0, si.framebufferWidth,
				      si.framebufferHeight, True);
}


/*
 * SendFramebufferUpdateRequest.
 * Note: this should only be called by the WriterThread
 */

Bool
SendFramebufferUpdateRequest(int x, int y, int w, int h, Bool incremental)
{
  rfbFramebufferUpdateRequestMsg fur;

  fur.type = rfbFramebufferUpdateRequest;
  fur.incremental = incremental ? 1 : 0;
  fur.x = Swap16IfLE(x);
  fur.y = Swap16IfLE(y);
  fur.w = Swap16IfLE(w);
  fur.h = Swap16IfLE(h);

  if (!WriteExact(rfbsock, (char *)&fur, sz_rfbFramebufferUpdateRequestMsg))
    return False;

  return True;
}


/*
 * SendPointerEvent.
 * Note: this should only be called by the WriterThread
 */

Bool
SendPointerEvent(int x, int y, int buttonMask)
{
  rfbPointerEventMsg pe;

  pe.type = rfbPointerEvent;
  pe.buttonMask = buttonMask;
  if (x < 0) x = 0;
  if (y < 0) y = 0;
  pe.x = Swap16IfLE(x);
  pe.y = Swap16IfLE(y);
  return WriteExact(rfbsock, (char *)&pe, sz_rfbPointerEventMsg);
}


/*
 * SendKeyEvent.
 * Note: this should only be called by the WriterThread
 */

Bool
SendKeyEvent(CARD32 key, Bool down)
{
  rfbKeyEventMsg ke;

  ke.type = rfbKeyEvent;
  ke.down = down ? 1 : 0;
  ke.key = Swap32IfLE(key);
  return WriteExact(rfbsock, (char *)&ke, sz_rfbKeyEventMsg);
}


/*
 * SendClientCutText.
 * Note: this should only be called by the WriterThread
 */

Bool
SendClientCutText(const char *str, int len)
{
  rfbClientCutTextMsg cct;

  cct.type = rfbClientCutText;
  cct.length = Swap32IfLE((unsigned int)len);
  return  (WriteExact(rfbsock, (char *)&cct, sz_rfbClientCutTextMsg) &&
	   WriteExact(rfbsock, str, len));
}


static Bool
HandleSoftCursorSetImage(rfbSoftCursorSetImage *msg, rfbRectangle *rect) 
{
  int iindex = msg->imageIndex - rfbSoftCursorSetIconOffset;
  PointerImage *pi = &pointerImages[iindex];
  if (iindex >= rfbSoftCursorMaxImages) {
    fprintf(stderr, "Received invalid soft cursor image index %d for SetImage\n", iindex);
    return False;
  }
  EnableClientCursor(0);

  if (pi->set && pi->image) 
    free(pi->image);

  pi->w = rect->w;
  pi->h = rect->h;
  pi->hotX = rect->x;
  pi->hotY = rect->y;
  pi->len = Swap16IfLE(msg->imageLength);
  pi->image = malloc(pi->len);
  if (!pi->image) {
    fprintf(stderr, "out of memory (size=%d)\n", pi->len);
    return False;
  }

  if (!ReadFromRFBServer(pi->image, pi->len)) 
    return False;
  pi->set = 1;
  return True;
}

/* framebuffer must be locked when calling this!!! */
static Bool
PointerMove(unsigned int x, unsigned int y, unsigned int mask,
	    int ox, int oy, int ow, int oh) 
{
  int nx, ny, nw, nh;

  if (x >= si.framebufferWidth)
    x = si.framebufferWidth - 1;
  if (y >= si.framebufferHeight)
    y = si.framebufferHeight - 1;

  cursorX = x;
  cursorY = y;
  drawCursor();
  UnlockFramebuffer();

  getBoundingRectCursor(cursorX, cursorY, imageIndex,
			&nx, &ny, &nw, &nh);

  if (rectsIntersect(ox, oy, ow, oh, nx, ny, nw, nh)) {
    rectsJoin(&ox, &oy, &ow, &oh, nx, ny, nw, nh);
    SyncScreenRegion(ox, oy, ow, oh);
  }
  else {
    SyncScreenRegion(ox, oy, ow, oh);
    SyncScreenRegion(nx, ny, nw, nh);
  }

  postMouseEvent(cursorX, cursorY, mask);

  return True;
}

static Bool
HandleSoftCursorMove(rfbSoftCursorMove *msg, rfbRectangle *rect) 
{
  int ii, ox, oy, ow, oh;

  /* get old cursor rect to know what to update */
  getBoundingRectCursor(cursorX, cursorY, imageIndex,
			&ox, &oy, &ow, &oh);

  ii = msg->imageIndex;
  if (ii >= rfbSoftCursorMaxImages) {
    fprintf(stderr, "Received invalid soft cursor image index %d for Move\n", ii);
    return False;
  }

  if (!pointerImages[ii].set)
    return True;

  LockFramebuffer();
  undrawCursor();
  imageIndex = ii;

  return PointerMove(rect->w, rect->h, msg->buttonMask, ox, oy, ow, oh);
}

static Bool
HandleCursorPos(unsigned int x, unsigned int y) 
{
  int ox, oy, ow, oh;

  /* get old cursor rect to know what to update */
  getBoundingRectCursor(cursorX, cursorY, imageIndex,
			&ox, &oy, &ow, &oh);
  if (!pointerImages[0].set)
    return True;

  LockFramebuffer();
  undrawCursor();
  imageIndex = 0;
  return PointerMove(x, y, 0, ox, oy, ow, oh);
}

/* call only from X11 thread. Only updates framebuffer, does not sync! */
void DrawCursorX11Thread(int x, int y) {
  int ox, oy, ow, oh, nx, ny, nw, nh;
  if (!pointerImages[0].set)
    return True;
  imageIndex = 0;

  if (x >= si.framebufferWidth)
    x = si.framebufferWidth - 1;
  if (y >= si.framebufferHeight)
    y = si.framebufferHeight - 1;

  LockFramebuffer();
  getBoundingRectCursor(cursorX, cursorY, imageIndex,
			&ox, &oy, &ow, &oh);
  undrawCursor();
  cursorX = x;
  cursorY = y;
  drawCursor();
  UnlockFramebuffer();

  getBoundingRectCursor(cursorX, cursorY, imageIndex,
			&nx, &ny, &nw, &nh);
  if (rectsIntersect(ox, oy, ow, oh, nx, ny, nw, nh)) {
    rectsJoin(&ox, &oy, &ow, &oh, nx, ny, nw, nh);
    SyncScreenRegionX11Thread(ox, oy, ow, oh);
  }
  else {
    SyncScreenRegionX11Thread(ox, oy, ow, oh);
    SyncScreenRegionX11Thread(nx, ny, nw, nh);
  }
}

/**
 * Create a softcursor in the "compressed alpha" format.
 * Returns the softcursor, caller owns the object
 */
static void *MakeSoftCursor(int bpp, int cursorWidth, int cursorHeight, 
			    CARD8 *cursorData, CARD8 *cursorMask, short *imageLen)
{
    int w = (cursorWidth+7)/8;
    unsigned char *cp, *sp, *dstData;
    int state; /* 0 = transparent, 1 otherwise */
    CARD8 *counter;
    unsigned char bit;
    int i,j;
    
    sp = (unsigned char*)cursorData;
    dstData = cp = (unsigned char*)calloc(cursorWidth*(bpp+2),cursorHeight);
    if (!dstData)
      return 0;

    state = 0;
    counter = cp++;
    *counter = 0;
    
    for(j=0;j<cursorHeight;j++)
	for(i=0,bit=0x80;i<cursorWidth;i++,bit=(bit&1)?0x80:bit>>1)
	    if(cursorMask[j*w+i/8]&bit) {
		if (state) {
		    memcpy(cp,sp,bpp);
		    cp += bpp;
		    sp += bpp;
		    (*counter)++;
		    if (*counter == 255) {
			state = 0;
			counter = cp++;
			*counter = 0;
		    }
		}
		else {
		    state = 1;
		    counter = cp++;
		    *counter = 1;
		    memcpy(cp,sp,bpp);
		    cp += bpp;
		    sp += bpp;
		}
	    }
	    else {
		if (!state) {
		    (*counter)++;
		    if (*counter == 255) {
			state = 1;
			counter = cp++;
			*counter = 0;
		    }
		}
		else {
		    state = 0;
		    counter = cp++;
		    *counter = 1;
		}
		sp += bpp;
	    }

    *imageLen = cp - dstData;
    return (void*) dstData;
}


/*********************************************************************
 * HandleCursorShape(). Support for XCursor and RichCursor shape
 * updates. We emulate cursor operating on the frame buffer (that is
 * why we call it "software cursor").
 ********************************************************************/

static Bool HandleCursorShape(int xhot, int yhot, int width, int height, CARD32 enc)
{
  int bytesPerPixel;
  size_t bytesPerRow, bytesMaskData;
  rfbXCursorColors rgb;
  CARD32 colors[2];
  CARD8 *ptr, *rcSource, *rcMask;
  void *softCursor;
  int x, y, b;
  int ox, oy, ow, oh;
  PointerImage *pi;
  short imageLen;

  bytesPerPixel = myFormat.bitsPerPixel / 8;
  bytesPerRow = (width + 7) / 8;
  bytesMaskData = bytesPerRow * height;

  if (width * height == 0)
    return True;

  /* Allocate memory for pixel data and temporary mask data. */

  rcSource = malloc(width * height * bytesPerPixel);
  if (rcSource == NULL)
    return False;

  rcMask = malloc(bytesMaskData);
  if (rcMask == NULL) {
    free(rcSource);
    return False;
  }

  /* Read and decode cursor pixel data, depending on the encoding type. */

  if (enc == rfbEncodingXCursor) {
    /* Read and convert background and foreground colors. */
    if (!ReadFromRFBServer((char *)&rgb, sz_rfbXCursorColors)) {
      free(rcSource);
      free(rcMask);
      return False;
    }
    colors[0] = RGB24_TO_PIXEL(32, rgb.backRed, rgb.backGreen, rgb.backBlue);
    colors[1] = RGB24_TO_PIXEL(32, rgb.foreRed, rgb.foreGreen, rgb.foreBlue);

    /* Read 1bpp pixel data into a temporary buffer. */
    if (!ReadFromRFBServer((char*)rcMask, bytesMaskData)) {
      free(rcSource);
      free(rcMask);
      return False;
    }

    /* Convert 1bpp data to byte-wide color indices. */
    ptr = rcSource;
    for (y = 0; y < height; y++) {
      for (x = 0; x < width / 8; x++) {
	for (b = 7; b >= 0; b--) {
	  *ptr = rcMask[y * bytesPerRow + x] >> b & 1;
	  ptr += bytesPerPixel;
	}
      }
      for (b = 7; b > 7 - width % 8; b--) {
	*ptr = rcMask[y * bytesPerRow + x] >> b & 1;
	ptr += bytesPerPixel;
      }
    }

    /* Convert indices into the actual pixel values. */
    switch (bytesPerPixel) {
    case 1:
      for (x = 0; x < width * height; x++)
	rcSource[x] = (CARD8)colors[rcSource[x]];
      break;
    case 2:
      for (x = 0; x < width * height; x++)
	((CARD16 *)rcSource)[x] = (CARD16)colors[rcSource[x * 2]];
      break;
    case 4:
      for (x = 0; x < width * height; x++)
	((CARD32 *)rcSource)[x] = colors[rcSource[x * 4]];
      break;
    }
    

  } else {
    if (!ReadFromRFBServer((char *)rcSource, width * height * bytesPerPixel)) {
      free(rcSource);
      free(rcMask);
      return False;
    }
  }

  /* Read mask data. */

  if (!ReadFromRFBServer((char*)rcMask, bytesMaskData)) {
    free(rcSource);
    free(rcMask);
    return False;
  }


  /* Set the soft cursor. */
  softCursor = MakeSoftCursor(bytesPerPixel, width, height, rcSource, rcMask, &imageLen);
  if (!softCursor) {
    free(rcMask);
    free(rcSource);
    return False;
  }

  /* get old cursor rect to know what to update */
  EnableClientCursor(1);
  LockFramebuffer();
  getBoundingRectCursor(cursorX, cursorY, imageIndex,
			&ox, &oy, &ow, &oh);
  undrawCursor();

  pi = &pointerImages[0];
  if (pi->set && pi->image)
    free(pi->image);
  pi->w = width;
  pi->h = height;
  pi->hotX = xhot;
  pi->hotY = yhot;
  pi->len = imageLen;
  pi->image = softCursor;
  pi->set = 1;

  imageIndex = 0;

  free(rcMask);
  free(rcSource);

  return PointerMove(cursorX, cursorY, 0, ox, oy, ow, oh);
}



/*
 * HandleRFBServerMessage.
 */

Bool
HandleRFBServerMessage()
{
  rfbServerToClientMsg msg;
  if (!ReadFromRFBServer((char *)&msg, 1))
    return False;

  switch (msg.type) {

  case rfbSetColourMapEntries:
  {
    int i;
    CARD16 rgb[3];
    XColor xc;

    if (!ReadFromRFBServer(((char *)&msg) + 1,
			   sz_rfbSetColourMapEntriesMsg - 1))
      return False;

    msg.scme.firstColour = Swap16IfLE(msg.scme.firstColour);
    msg.scme.nColours = Swap16IfLE(msg.scme.nColours);

    for (i = 0; i < msg.scme.nColours; i++) {
      if (!ReadFromRFBServer((char *)rgb, 6))
	return False;
      xc.pixel = msg.scme.firstColour + i;
      xc.red = Swap16IfLE(rgb[0]);
      xc.green = Swap16IfLE(rgb[1]);
      xc.blue = Swap16IfLE(rgb[2]);
      xc.flags = DoRed|DoGreen|DoBlue;
      /* Disable colormaps
      lockQt();
      XStoreColor(dpy, cmap, &xc);
      unlockQt();
      */
    }

    break;
  }

  case rfbFramebufferUpdate:
  {
    rfbFramebufferUpdateRectHeader rect;
    int linesToRead;
    int bytesPerLine;
    int i;

    announceIncrementalUpdateRequest();

    if (!ReadFromRFBServer(((char *)&msg.fu) + 1,
			   sz_rfbFramebufferUpdateMsg - 1))
      return False;

    msg.fu.nRects = Swap16IfLE(msg.fu.nRects);

    for (i = 0; i < msg.fu.nRects; i++) {
      if (!ReadFromRFBServer((char *)&rect, sz_rfbFramebufferUpdateRectHeader))
	return False;

      rect.encoding = Swap32IfLE(rect.encoding);
      if (rect.encoding == rfbEncodingLastRect)
	break;

      rect.r.x = Swap16IfLE(rect.r.x);
      rect.r.y = Swap16IfLE(rect.r.y);
      rect.r.w = Swap16IfLE(rect.r.w);
      rect.r.h = Swap16IfLE(rect.r.h);

      if (rect.encoding == rfbEncodingPointerPos) {
        if (!HandleCursorPos(rect.r.x, rect.r.y)) {
          return False;
        }
        continue;
      }

      if (rect.encoding == rfbEncodingXCursor ||
	  rect.encoding == rfbEncodingRichCursor) {
	if (!HandleCursorShape(rect.r.x, rect.r.y, rect.r.w, rect.r.h,
			      rect.encoding)) {
	  return False;
	}
	continue;
      }

      if ((rect.r.x + rect.r.w > si.framebufferWidth) ||
	  (rect.r.y + rect.r.h > si.framebufferHeight))
	{
	  fprintf(stderr,"Rect too large: %dx%d at (%d, %d)\n",
		  rect.r.w, rect.r.h, rect.r.x, rect.r.y);
	  return False;
	}

      if ((rect.r.h * rect.r.w == 0) && 
	  (rect.encoding != rfbEncodingSoftCursor)) {
	fprintf(stderr,"Zero size rect - ignoring\n");
	continue;
      }

      switch (rect.encoding) {

      case rfbEncodingRaw:

	bytesPerLine = rect.r.w * myFormat.bitsPerPixel / 8;
	linesToRead = BUFFER_SIZE / bytesPerLine;

	while (rect.r.h > 0) {
	  if (linesToRead > rect.r.h)
	    linesToRead = rect.r.h;

	  if (!ReadFromRFBServer(buffer,bytesPerLine * linesToRead))
	    return False;

	  CopyDataToScreen(buffer, rect.r.x, rect.r.y, rect.r.w,
			   linesToRead);

	  rect.r.h -= linesToRead;
	  rect.r.y += linesToRead;

	}
	break;

      case rfbEncodingCopyRect:
      {
	rfbCopyRect cr;

	if (!ReadFromRFBServer((char *)&cr, sz_rfbCopyRect))
	  return False;

	cr.srcX = Swap16IfLE(cr.srcX);
	cr.srcY = Swap16IfLE(cr.srcY);

	CopyArea(cr.srcX, cr.srcY, rect.r.w, rect.r.h, rect.r.x, rect.r.y);

	break;
      }

      case rfbEncodingHextile:
      {
	switch (myFormat.bitsPerPixel) {
	case 8:
	  if (!HandleHextile8(rect.r.x,rect.r.y,rect.r.w,rect.r.h))
	    return False;
	  break;
	case 16:
	  if (!HandleHextile16(rect.r.x,rect.r.y,rect.r.w,rect.r.h))
	    return False;
	  break;
	case 32:
	  if (!HandleHextile32(rect.r.x,rect.r.y,rect.r.w,rect.r.h))
	    return False;
	  break;
	}
	break;
      }

      case rfbEncodingZlib:
      {
	switch (myFormat.bitsPerPixel) {
	case 8:
	  if (!HandleZlib8(rect.r.x,rect.r.y,rect.r.w,rect.r.h))
	    return False;
	  break;
	case 16:
	  if (!HandleZlib16(rect.r.x,rect.r.y,rect.r.w,rect.r.h))
	    return False;
	  break;
	case 32:
	  if (!HandleZlib32(rect.r.x,rect.r.y,rect.r.w,rect.r.h))
	    return False;
	  break;
	}
	break;
     }

      case rfbEncodingTight:
      {
	switch (myFormat.bitsPerPixel) {
	case 8:
	  if (!HandleTight8(rect.r.x,rect.r.y,rect.r.w,rect.r.h))
	    return False;
	  break;
	case 16:
	  if (!HandleTight16(rect.r.x,rect.r.y,rect.r.w,rect.r.h))
	    return False;
	  break;
	case 32:
	  if (!HandleTight32(rect.r.x,rect.r.y,rect.r.w,rect.r.h))
	    return False;
	  break;
	}
	break;
      }

      case rfbEncodingSoftCursor:
      {
	rfbSoftCursorMsg scmsg;
	if (!ReadFromRFBServer((char *)&scmsg, 1))
	  return False;
	if (scmsg.type < rfbSoftCursorMaxImages) {
	  if (!ReadFromRFBServer(((char *)&scmsg)+1, 
				 sizeof(rfbSoftCursorMove)- 1))
	    return False;
	  if (!HandleSoftCursorMove(&scmsg.move, &rect.r))
	    return False;
	}
	else if((scmsg.type >= rfbSoftCursorSetIconOffset) && 
		(scmsg.type < rfbSoftCursorSetIconOffset+rfbSoftCursorMaxImages)) {
	  if (!ReadFromRFBServer(((char *)&scmsg)+1, 
				 sizeof(rfbSoftCursorSetImage)- 1))
	    return False;
	  if (!HandleSoftCursorSetImage(&scmsg.setImage, &rect.r))
	    return False;
	}
	else {
	  fprintf(stderr,"Unknown soft cursor image index %d\n", 
		  (int)scmsg.type);
	  return False;
	}
	break;
      }

      default:
	fprintf(stderr,"Unknown rect encoding %d\n",
		(int)rect.encoding);
	return False;
      }

    }

    queueIncrementalUpdateRequest();

    break;
  }

  case rfbBell:
  {
    beep();
    break;
  }

  case rfbServerCutText:
  {
    char *serverCutText;
    if (!ReadFromRFBServer(((char *)&msg) + 1,
			   sz_rfbServerCutTextMsg - 1))
      return False;

    msg.sct.length = Swap32IfLE(msg.sct.length);

    if (msg.sct.length > MAX_CUTBUFFER) {
      fprintf(stderr, "Cutbuffer too long.\n");
      return False;
    }

    serverCutText = malloc(msg.sct.length+1);

    if (!serverCutText) {
      fprintf(stderr, "Out-of-memory, cutbuffer too long.\n");
      return False;
    }

    if (!ReadFromRFBServer(serverCutText, msg.sct.length))
      return False;

    serverCutText[msg.sct.length] = 0;
    newServerCut(serverCutText, msg.sct.length); /* takes ownership of serverCutText */

    break;
  }

  default:
    fprintf(stderr,"Unknown message type %d from VNC server\n",msg.type);
    return False;
  }

  return True;
}


#define GET_PIXEL8(pix, ptr) ((pix) = *(ptr)++)

#define GET_PIXEL16(pix, ptr) (((CARD8*)&(pix))[0] = *(ptr)++, \
			       ((CARD8*)&(pix))[1] = *(ptr)++)

#define GET_PIXEL32(pix, ptr) (((CARD8*)&(pix))[0] = *(ptr)++, \
			       ((CARD8*)&(pix))[1] = *(ptr)++, \
			       ((CARD8*)&(pix))[2] = *(ptr)++, \
			       ((CARD8*)&(pix))[3] = *(ptr)++)

/* CONCAT2 concatenates its two arguments.  CONCAT2E does the same but also
   expands its arguments if they are macros */

#define CONCAT2(a,b) a##b
#define CONCAT2E(a,b) CONCAT2(a,b)

#define BPP 8
#include "hextile.c"
#include "zlib.c"
#include "tight.c"
#undef BPP
#define BPP 16
#include "hextile.c"
#include "zlib.c"
#include "tight.c"
#undef BPP
#define BPP 32
#include "hextile.c"
#include "zlib.c"
#include "tight.c"
#undef BPP


/*
 * PrintPixelFormat.
 */

void
PrintPixelFormat(format)
    rfbPixelFormat *format;
{
  if (format->bitsPerPixel == 1) {
    fprintf(stderr,"  Single bit per pixel.\n");
    fprintf(stderr,
	    "  %s significant bit in each byte is leftmost on the screen.\n",
	    (format->bigEndian ? "Most" : "Least"));
  } else {
    fprintf(stderr,"  %d bits per pixel.\n",format->bitsPerPixel);
    if (format->bitsPerPixel != 8) {
      fprintf(stderr,"  %s significant byte first in each pixel.\n",
	      (format->bigEndian ? "Most" : "Least"));
    }
    if (format->trueColour) {
      fprintf(stderr,"  True colour: max red %d green %d blue %d",
	      format->redMax, format->greenMax, format->blueMax);
      fprintf(stderr,", shift red %d green %d blue %d\n",
	      format->redShift, format->greenShift, format->blueShift);
    } else {
      fprintf(stderr,"  Colour map (not true colour).\n");
    }
  }
}

static long
ReadCompactLen (void)
{
  long len;
  CARD8 b;

  if (!ReadFromRFBServer((char *)&b, 1))
    return -1;
  len = (int)b & 0x7F;
  if (b & 0x80) {
    if (!ReadFromRFBServer((char *)&b, 1))
      return -1;
    len |= ((int)b & 0x7F) << 7;
    if (b & 0x80) {
      if (!ReadFromRFBServer((char *)&b, 1))
	return -1;
      len |= ((int)b & 0xFF) << 14;
    }
  }
  return len;
}

void freeRFBProtoResources() {
  int i;

  if (desktopName)
    free(desktopName);
  if (raw_buffer)
    free(raw_buffer);
  for (i = 0; i < rfbSoftCursorMaxImages; i++) 
    if (pointerImages[i].set && pointerImages[i].image)
      free(pointerImages[i].image);

  raw_buffer_size = -1;
  raw_buffer = NULL;
  decompStreamInited = False;
  zlibStreamActive[0] = False;
  zlibStreamActive[1] = False;
  zlibStreamActive[2] = False;
  zlibStreamActive[3] = False;
  for (i = 0; i < rfbSoftCursorMaxImages; i++) 
    pointerImages[i].set = 0;
  imageIndex = -1;
}

void freeResources() {
  freeSocketsResources();
  freeDesktopResources();
  freeRFBProtoResources();
}

/*
 * JPEG source manager functions for JPEG decompression in Tight decoder.
 */

static struct jpeg_source_mgr jpegSrcManager;
static JOCTET *jpegBufferPtr;
static size_t jpegBufferLen;

static void
JpegInitSource(j_decompress_ptr cinfo)
{
  jpegError = False;
}

static boolean
JpegFillInputBuffer(j_decompress_ptr cinfo)
{
  jpegError = True;
  jpegSrcManager.bytes_in_buffer = jpegBufferLen;
  jpegSrcManager.next_input_byte = (JOCTET *)jpegBufferPtr;

  return TRUE;
}

static void
JpegSkipInputData(j_decompress_ptr cinfo, long num_bytes)
{
  if (num_bytes < 0 || num_bytes > jpegSrcManager.bytes_in_buffer) {
    jpegError = True;
    jpegSrcManager.bytes_in_buffer = jpegBufferLen;
    jpegSrcManager.next_input_byte = (JOCTET *)jpegBufferPtr;
  } else {
    jpegSrcManager.next_input_byte += (size_t) num_bytes;
    jpegSrcManager.bytes_in_buffer -= (size_t) num_bytes;
  }
}

static void
JpegTermSource(j_decompress_ptr cinfo)
{
  /* No work necessary here. */
}

static void
JpegSetSrcManager(j_decompress_ptr cinfo, CARD8 *compressedData,
		  int compressedLen)
{
  jpegBufferPtr = (JOCTET *)compressedData;
  jpegBufferLen = (size_t)compressedLen;

  jpegSrcManager.init_source = JpegInitSource;
  jpegSrcManager.fill_input_buffer = JpegFillInputBuffer;
  jpegSrcManager.skip_input_data = JpegSkipInputData;
  jpegSrcManager.resync_to_restart = jpeg_resync_to_restart;
  jpegSrcManager.term_source = JpegTermSource;
  jpegSrcManager.next_input_byte = jpegBufferPtr;
  jpegSrcManager.bytes_in_buffer = jpegBufferLen;

  cinfo->src = &jpegSrcManager;
}

