/***************************************************************************
                          vidmode.cpp  -  video mode switching
                             -------------------
    begin                : Tue June 3 03:08:00 CET 2002
    copyright            : (C) 2002 by Tim Jansen
    email                : tim@tjansen.de
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include <config-krdc.h>
#include <X11/Xlib.h>

#ifdef HAVE_VIDMODE_EXTENSION
#include <X11/extensions/xf86vmode.h>
#endif

#include "vidmode.h"

#ifdef HAVE_VIDMODE_EXTENSION

void vidmodeNormalSwitch(Display *dpy, Resolution oldResolution)
{
	if (!oldResolution.valid)
		return;

	XF86VidModeModeInfo **modes;
	int modecount;
	int eventB, errorB;

	if (!XF86VidModeQueryExtension(dpy, &eventB, &errorB))
		return;

	if (!XF86VidModeGetAllModeLines(dpy,oldResolution.screen,&modecount, &modes))
		return;

	for (int i = 0; i < modecount; i++) {
		int w = (*modes[i]).hdisplay;
		int h = (*modes[i]).vdisplay;

		if ((oldResolution.width == w) &&
		    (oldResolution.height == h)) {
			XF86VidModeSwitchToMode(dpy,oldResolution.screen,modes[i]);
			XFlush(dpy);
			XF86VidModeSetViewPort(dpy,DefaultScreen(dpy),0,0);
			XFlush(dpy);
			return;
		}
	}
}

Resolution vidmodeFullscreenSwitch(Display *dpy, int screen,
				   int sw, int sh, int &nx, int &ny)
{
	XF86VidModeModeInfo **modes;
	int modecount;
	int bestmode = -1;
	int bestw, besth;
	int eventB, errorB;

	if (screen < 0)
		return Resolution();

	if (!XF86VidModeQueryExtension(dpy, &eventB, &errorB))
		return Resolution();

	if (!XF86VidModeGetAllModeLines(dpy,screen,&modecount, &modes))
		return Resolution();

	int cw = (*modes[0]).hdisplay;
	int ch = (*modes[0]).vdisplay;
	nx = cw;
	ny = ch;
	if ((cw == sw) && (ch == sh))
		return Resolution();
	bool foundLargeEnoughRes = (cw>=sw) && (ch>=sh);
	bestw = cw;
	besth = ch;

	for (int i = 1; i < modecount; i++) {
		int w = (*modes[i]).hdisplay;
		int h = (*modes[i]).vdisplay;

		if ((w == cw) && (h == ch))
			continue;

		/* If resolution matches framebuffer, take it */
		if ((w == sw) && (h == sh)) {
			bestw = w;
			besth = h;
			bestmode = i;
			break;
		}
		/* if resolution larger than framebuffer... */
		if ((w>=sw) && (h>=sh)) {
			/* and no other previously found resoltion was smaller or
			   this is smaller than the best resolution so far, take it*/
			if ((!foundLargeEnoughRes) ||
			    (w*h < bestw*besth)) {
				bestw = w;
				besth = h;
				bestmode = i;
				foundLargeEnoughRes = true;
			}
		}
		/* If all resolutions so far were smaller than framebuffer... */
		else if (!foundLargeEnoughRes) {
			/* take this one it it is bigger then they were */
			if (w*h > bestw*besth) {
				bestw = w;
				besth = h;
				bestmode = i;
			}
		}
	}

	if (bestmode == -1)
		return Resolution();

	nx = bestw;
	ny = besth;
	XF86VidModeSwitchToMode(dpy,screen,modes[bestmode]);
	XF86VidModeSetViewPort(dpy,screen,0,0);
	XFlush(dpy);

	return Resolution(cw, ch, screen);
}

#else

void vidmodeNormalSwitch(Display *dpy, Resolution oldResolution)
{
}

Resolution vidmodeFullscreenSwitch(Display *dpy, int screen, int sw, int sh, int &nx, int &ny)
{
	return Resolution();
}

#endif

void grabInput(Display *dpy, unsigned int winId) {
	XGrabPointer(dpy, winId, True, 0,
		     GrabModeAsync, GrabModeAsync,
		     winId, None, CurrentTime);
	XFlush(dpy);
}

void ungrabInput(Display *dpy) {
	XUngrabPointer(dpy, CurrentTime);
}

