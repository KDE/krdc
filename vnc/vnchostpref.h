/***************************************************************************
                       vnchostprefs.h  -  vnc host preferences
                             -------------------
    begin                : Fri May 09 22:32 CET 2003
    copyright            : (C) 2003 by Tim Jansen
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

#ifndef VNCHOSTPREF_H
#define VNCHOSTPREF_H

#include "hostpreferences.h"

class VncHostPref : public HostPref {
protected:
	friend class HostPreferences;

	int m_quality;
	bool m_askOnConnect;

	virtual void load();
	virtual void setDefaults();
	virtual void save();
	virtual void remove();

public:
	static const QString VncType;

	VncHostPref(KConfig *conf, const QString &host, const QString &type);
	virtual ~VncHostPref();
	
	virtual QString prefDescription() const;
	void setQuality(int q);
	int quality() const;
	void setAskOnConnect(bool ask);
	bool askOnConnect() const;
};

#endif
