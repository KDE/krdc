/***************************************************************************
                          vidmode.h  -  video mode switching
                             -------------------
    begin                : Tue June 3 03:11:00 CET 2002
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


void vidmodeNormalSwitch(Display *dpy, int oldResolution);
int vidmodeFullscreenSwitch(Display *dpy, int sw, int sh, int &nx, int &ny);

void grabInput(Display *dpy, unsigned int winId);
void ungrabInput(Display *dpy);
