/***************************************************************************
                       hostpreferences.cpp  -  per host preferences
                             -------------------
    begin                : Fri May 09 22:33 CET 2003
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

#include "hostpreferences.h"
#include "vnc/vnchostpref.h"
#include <kconfig.h>
#include <qregexp.h>
#include <qmap.h>

HostPref::HostPref(KConfig *conf, const QString &host, const QString &type) :
	m_host(host),
	m_type(type),
	m_config(conf) {
}

HostPref::~HostPref() {
}

QString HostPref::host() const {
	return m_host;
}

QString HostPref::type() const {
	return m_type;
}

QString HostPref::prefix() const {
	return getPrefix(m_host, m_type);
}

QString HostPref::getPrefix(const QString &host, const QString &type) {
	return QString("PerHost-%1-%2-").arg(type).arg(host);
}


HostPreferences::HostPreferences(KConfig *conf) :
	m_config(conf) {
}

SmartPtr<HostPref> HostPreferences::getHostPref(const QString &host, const QString &type) {
	m_config->setGroup("PerHostSettings");
	if (!m_config->readBoolEntry(HostPref::getPrefix(host, type)+"exists"))
		return 0;
	
	if (type == VncHostPref::VncType) {
		SmartPtr<HostPref> hp = new VncHostPref(m_config, host, type);
		hp->load();
		return hp;
	}
	Q_ASSERT(true);
	return 0;
}

SmartPtr<HostPref> HostPreferences::createHostPref(const QString &host, const QString &type) {
	SmartPtr<HostPref> hp = getHostPref(host, type);
	if (hp)
		return hp;
	hp = new VncHostPref(m_config, host, type);
	hp->setDefaults();
	return hp;
}

QValueList<SmartPtr<HostPref> > HostPreferences::getAllHostPrefs() {
	QValueList<SmartPtr<HostPref> > r;
	QMap<QString, QString> map = m_config->entryMap("PerHostSettings");
	QStringList keys = map.keys();
	QStringList::iterator it = keys.begin();
	while (it != keys.end()) {
		QString key = *it;
		if (key.endsWith("-exists")) {
			QRegExp re("PerHost-([^-]+)-(.*)-exists");
			if (re.exactMatch(key)) {
				SmartPtr<HostPref> hp = getHostPref(re.cap(2), re.cap(1));
				if (hp)
					r += hp;
			}
			
		}
		it++;
	}
	return r;
}

void HostPreferences::sync() {
	m_config->sync();
}
