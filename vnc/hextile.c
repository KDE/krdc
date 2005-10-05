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
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301,
 *  USA.
 */

/*
 * hextile.c - handle hextile encoding.
 *
 * This file shouldn't be compiled directly.  It is included multiple times by
 * rfbproto.c, each time with a different definition of the macro BPP.  For
 * each value of BPP, this file defines a function which handles a hextile
 * encoded rectangle with BPP bits per pixel.
 */

#define HandleHextileBPP CONCAT2E(HandleHextile,BPP)
#define CARDBPP CONCAT2E(CARD,BPP)
#define GET_PIXEL CONCAT2E(GET_PIXEL,BPP)
#define FillRectangleBPP CONCAT2E(FillRectangle,BPP)

static Bool
HandleHextileBPP (int rx, int ry, int rw, int rh)
{
  CARDBPP bg, fg;
  int i;
  CARD8 *ptr;
  int x, y, w, h;
  int sx, sy, sw, sh;
  CARD8 subencoding;
  CARD8 nSubrects;

  for (y = ry; y < ry+rh; y += 16) {
    for (x = rx; x < rx+rw; x += 16) {
      w = h = 16;
      if (rx+rw - x < 16)
	w = rx+rw - x;
      if (ry+rh - y < 16)
	h = ry+rh - y;

      if (!ReadFromRFBServer((char *)&subencoding, 1))
	return False;

      if (subencoding & rfbHextileRaw) {
	if (!ReadFromRFBServer(buffer, w * h * (BPP / 8)))
	  return False;

	CopyDataToScreen(buffer, x, y, w, h);
	continue;
      }

      if (subencoding & rfbHextileBackgroundSpecified)
	if (!ReadFromRFBServer((char *)&bg, sizeof(bg)))
	  return False;

      LockFramebuffer();
      FillRectangleBPP(bg, x, y, w, h);

      if (subencoding & rfbHextileForegroundSpecified)
	if (!ReadFromRFBServer((char *)&fg, sizeof(fg))) {
	  UnlockFramebuffer();
	  return False;
	}

      if (!(subencoding & rfbHextileAnySubrects)) {
	UnlockFramebuffer();
	SyncScreenRegion(x, y, w, h);
	continue;
      }

      if (!ReadFromRFBServer((char *)&nSubrects, 1)) {
	UnlockFramebuffer();
	return False;
      }

      ptr = (CARD8 *)buffer;

      if (subencoding & rfbHextileSubrectsColoured) {
	if (!ReadFromRFBServer(buffer, nSubrects * (2 + (BPP / 8)))) {
	  UnlockFramebuffer();
	  return False;
	}

	for (i = 0; i < nSubrects; i++) {
	  GET_PIXEL(fg, ptr);
	  sx = rfbHextileExtractX(*ptr);
	  sy = rfbHextileExtractY(*ptr);
	  ptr++;
	  sw = rfbHextileExtractW(*ptr);
	  sh = rfbHextileExtractH(*ptr);
	  ptr++;
	  FillRectangleBPP(fg, x+sx, y+sy, sw, sh);
	}

      } else {
	if (!ReadFromRFBServer(buffer, nSubrects * 2)) {
	  UnlockFramebuffer();
	  return False;
	}

	for (i = 0; i < nSubrects; i++) {
	  sx = rfbHextileExtractX(*ptr);
	  sy = rfbHextileExtractY(*ptr);
	  ptr++;
	  sw = rfbHextileExtractW(*ptr);
	  sh = rfbHextileExtractH(*ptr);
	  ptr++;
	  FillRectangleBPP(fg, x+sx, y+sy, sw, sh);
	}
      }
      UnlockFramebuffer();
      SyncScreenRegion(x, y, w, h);
    }
  }

  return True;
}
