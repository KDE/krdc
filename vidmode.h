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

struct Resolution {
  Resolution(int w, int h, int s) :
    valid(true), width(w), height(h), screen(s) {
  }
  Resolution() :
    valid(false), width(0), height(0), screen(0) {
  }
  bool valid;
  int width;
  int height;
  int screen;
};

void vidmodeNormalSwitch(Display *dpy, Resolution oldResolution);
Resolution vidmodeFullscreenSwitch(Display *dpy, int screen, int sw, int sh, int &nx, int &ny);

void grabInput(Display *dpy, unsigned int winId);
void ungrabInput(Display *dpy);
