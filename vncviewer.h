/*
 *  Copyright (C) 2000, 2001 Const Kaplinsky.  All Rights Reserved.
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
 * vncviewer.h
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <unistd.h>
#include <X11/IntrinsicP.h>
#include <X11/StringDefs.h>
#include <X11/Shell.h>
#include <X11/Xmd.h>
#include <X11/keysym.h>
#include <X11/Xatom.h>
#include <X11/Xmu/StdSel.h>

#if(defined __cplusplus)
extern "C"
{
#endif

#include "rfbproto.h"

extern int endianTest;

#define Swap16IfLE(s) \
    (*(char *)&endianTest ? ((((s) & 0xff) << 8) | (((s) >> 8) & 0xff)) : (s))

#define Swap32IfLE(l) \
    (*(char *)&endianTest ? ((((l) & 0xff000000) >> 24) | \
			     (((l) & 0x00ff0000) >> 8)  | \
			     (((l) & 0x0000ff00) << 8)  | \
			     (((l) & 0x000000ff) << 24))  : (l))

#define MAX_ENCODINGS 20

#define FLASH_PORT_OFFSET 5400
#define LISTEN_PORT_OFFSET 5500
#define TUNNEL_PORT_OFFSET 5500
#define SERVER_PORT_OFFSET 5900

#define DEFAULT_SSH_CMD "/usr/bin/ssh"
#define DEFAULT_TUNNEL_CMD  \
  (DEFAULT_SSH_CMD " -f -L %L:localhost:%R %H sleep 20")
#define DEFAULT_VIA_CMD     \
  (DEFAULT_SSH_CMD " -f -L %L:%H:%R %G sleep 20")


/** main.cpp **/

typedef enum {
  QUALITY_UNKNOWN,
  QUALITY_HIGH,
  QUALITY_MEDIUM,
  QUALITY_LOW
} Quality;

typedef struct {
  Bool shareDesktop;
  Bool viewOnly;

  const char* encodingsString;

  Bool useBGR233;
  int nColours;
  Bool useSharedColours;
  int requestedDepth;

  int rawDelay;
  int copyRectDelay;

  Bool debug;

  int compressLevel;
  int qualityLevel;
  Bool useRemoteCursor;

} AppData;

extern AppData appData;

extern Display* dpy;
extern const char *vncServerHost;
extern int vncServerPort;

extern int isQuitFlagSet();
extern void lockQt();
extern void unlockQt();
extern void unlockQtGui();
extern int getPassword(char *passwd, int pwlen);

/* threads.cpp */
extern void queueIncrementalUpdateRequest();

/* colour.c */

extern unsigned long BGR233ToPixel[];

extern Colormap cmap;
extern Visual *vis;
extern unsigned int visdepth, visbpp;

extern void SetVisualAndCmap(void);

/* cursor.c */

extern Bool HandleXCursor(int xhot, int yhot, int width, int height);
extern Bool HandleRichCursor(int xhot, int yhot, int width, int height);
extern void SoftCursorLockArea(int x, int y, int w, int h);
extern void SoftCursorUnlockScreen(void);
extern void SoftCursorMove(int x, int y);

/* desktop.c */

extern Widget form, viewport, desktop;
extern Window desktopWin;
extern Cursor dotCursor;
extern GC gc;
extern GC srcGC, dstGC;
extern Dimension dpyWidth, dpyHeight;

extern void DesktopInit(Window win);
extern void ToplevelInit(void);
extern void SendRFBEvent(XEvent *event, String *params, Cardinal *num_params);
extern void CopyDataToScreen(char *buf, int x, int y, int width, int height);
extern void ShmSync(void);
extern void Cleanup(void);

/* rfbproto.c */

extern int rfbsock;
extern Bool canUseCoRRE;
extern Bool canUseHextile;
extern char *desktopName;
extern rfbPixelFormat myFormat;
extern rfbServerInitMsg si;
extern char *serverCutText;
extern Bool newServerCutText;

enum InitStatus {
  INIT_OK,
  INIT_PROTOCOL_FAILURE,
  INIT_CONNECTION_FAILED,
  INIT_AUTHENTICATION_FAILED,
  INIT_ABORTED
};

extern Bool ConnectToRFBServer(const char *hostname, int port);
extern enum InitStatus InitialiseRFBConnection(void);
extern Bool SetFormatAndEncodings(void);
extern Bool SendIncrementalFramebufferUpdateRequest(void);
extern Bool SendFramebufferUpdateRequest(int x, int y, int w, int h,
					 Bool incremental);
extern Bool SendPointerEvent(int x, int y, int buttonMask);
extern Bool SendKeyEvent(CARD32 key, Bool down);
extern Bool SendClientCutText(char *str, int len);
extern Bool HandleRFBServerMessage(void);

extern void PrintPixelFormat(rfbPixelFormat *format);

/* selection.c */

extern void InitialiseSelection(void);
extern void SelectionToVNC(Widget w, XEvent *event, String *params,
			   Cardinal *num_params);
extern void SelectionFromVNC(Widget w, XEvent *event, String *params,
			     Cardinal *num_params);

/* shm.c */

extern XImage *CreateShmImage(void);
extern void ShmCleanup(void);

/* sockets.c */

extern Bool errorMessageOnReadFailure;

extern Bool ReadFromRFBServer(char *out, unsigned int n);
extern Bool WriteExact(int sock, char *buf, int n);
extern int FindFreeTcpPort(void);
extern int ListenAtTcpPort(int port);
extern int ConnectToTcpAddr(unsigned int host, int port);
extern int AcceptTcpConnection(int listenSock);
extern Bool SetNonBlocking(int sock);

extern int StringToIPAddr(const char *str, unsigned int *addr);
extern Bool SameMachine(int sock);

#if(defined __cplusplus)
}
#endif
