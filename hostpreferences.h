/***************************************************************************
                       hostpreferences.h  -  per host preferences
                             -------------------
    begin                : Fri May 09 19:02 CET 2003
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

#ifndef HOSTPREFERENCES_H
#define HOSTPREFERENCES_H

#include <qstring.h>
#include <qvaluelist.h>
#include "smartptr.h"

class HostPreferences;
class KConfig;

class HostPref {
protected:
	friend class HostPreferences;
	QString m_host;
	QString m_type;
	KConfig *m_config;

	HostPref(KConfig *conf, const QString &host, const QString &type);

	virtual void load() = 0;
	virtual void setDefaults() = 0;
	virtual void save() = 0;
	virtual void remove() = 0;
public:
	virtual ~HostPref();

	virtual QString prefDescription() const = 0;
	QString host() const;
	QString type() const;
	QString prefix() const;
	static QString getPrefix(const QString &host, const QString &type);
};

class HostPreferences {
private:
	KConfig *m_config;

public:
	HostPreferences(KConfig *conf);

	SmartPtr<HostPref> getHostPref(const QString &host, const QString &type);
	SmartPtr<HostPref> createHostPref(const QString &host, const QString &type);
	QValueList<SmartPtr<HostPref> > getAllHostPrefs();
	void removeHostPref(HostPref *hostPref);
	void sync();

};

#endif
