/*
   Copyright (C) 2003 Tim Jansen <tim@tjansen.de>
   Copyright (C) 2004 Nadeem Hasan <nhasan@kde.org>
*/
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


#include <QStringList>
#include <QList>
#include <ksharedconfig.h>

#include "smartptr.h"

class HostPreferences;

class HostPref {
protected:
	friend class HostPreferences;
	QString m_host;
	QString m_type;
	KSharedConfigPtr m_config;

	HostPref(KSharedConfigPtr conf, const QString &host, const QString &type);

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
	static QString prefix(const QString &host, const QString &type);
};

typedef SmartPtr<HostPref> HostPrefPtr;
typedef QList<HostPrefPtr> HostPrefPtrList;

class HostPreferences {
public:
	static HostPreferences *instance();
	~HostPreferences();

	HostPrefPtr getHostPref(const QString &host, const QString &type);
	HostPrefPtr createHostPref(const QString &host, const QString &type);
	HostPrefPtrList getAllHostPrefs();
	HostPrefPtr vncDefaults();
	HostPrefPtr rdpDefaults();
	void removeHostPref(HostPref *hostPref);

	void setShowBrowsingPanel( bool b );
	void setServerCompletions( const QStringList &list );
	void setServerHistory( const QStringList &list );

	bool showBrowsingPanel();
	QStringList serverCompletions();
	QStringList serverHistory();

	void sync();

private:
	HostPreferences();

	KSharedConfigPtr m_config;
	static HostPreferences *m_instance;

};

#endif
