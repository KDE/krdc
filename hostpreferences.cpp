/***************************************************************************
                       hostpreferences.cpp  -  per host preferences
                             -------------------
    begin                : Fri May 09 22:33 CET 2003
    copyright            : (C) 2003 by Tim Jansen
                         : (C) 2004 Nadeem Hasan <nhasan@kde.org>
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
#include "rdp/rdphostpref.h"

#include <kapplication.h>
#include <kconfig.h>
#include <kstaticdeleter.h>

#include <qregexp.h>
#include <qmap.h>

HostPreferences *HostPreferences::m_instance = 0;
static KStaticDeleter<HostPreferences> sd;

HostPreferences *HostPreferences::instance()
{
	if ( m_instance == 0 )
		sd.setObject( m_instance, new HostPreferences() );

	return m_instance;
}

HostPref::HostPref(KConfig *conf, const QString &host, const QString &type) :
	m_host(host),
	m_type(type),
	m_config(conf) {
}

HostPref::~HostPref() {
	m_config->sync();
}

QString HostPref::host() const {
	return m_host;
}

QString HostPref::type() const {
	return m_type;
}

QString HostPref::prefix() const {
	return prefix(m_host, m_type);
}

QString HostPref::prefix(const QString &host, const QString &type) {
	return QString("PerHost-%1-%2-").arg(type).arg(host);
}


HostPreferences::HostPreferences() {
	m_config = kapp->config();
}

HostPreferences::~HostPreferences() {
  if ( m_instance == this )
    sd.setObject( m_instance, 0, false );
}

HostPrefPtr HostPreferences::getHostPref(const QString &host, const QString &type) {
	m_config->setGroup("PerHostSettings");
	if (!m_config->readBoolEntry(HostPref::prefix(host, type)+"exists"))
		return 0;

	if (type == VncHostPref::VncType) {
		HostPrefPtr hp = new VncHostPref(m_config, host, type);
		hp->load();
		return hp;
	}
	else if(type == RdpHostPref::RdpType) {
		HostPrefPtr hp = new RdpHostPref(m_config, host, type);
		hp->load();
		return hp;
	}
	Q_ASSERT(true);
	return 0;
}

HostPrefPtr HostPreferences::createHostPref(const QString &host, const QString &type) {
	HostPrefPtr hp = getHostPref(host, type);
	if (hp)
		return hp;

	if(type == VncHostPref::VncType)
	{
		hp = new VncHostPref(m_config, host, type);
	}
	else if(type == RdpHostPref::RdpType)
	{
		hp = new RdpHostPref(m_config, host, type);
	}
	hp->setDefaults();
	return hp;
}

HostPrefPtr HostPreferences::vncDefaults()
{
	HostPrefPtr hp = new VncHostPref( m_config );
	hp->load();

	return hp;
}

HostPrefPtr HostPreferences::rdpDefaults()
{
	HostPrefPtr hp = new RdpHostPref( m_config );
	hp->load();

	return hp;
}

HostPrefPtrList HostPreferences::getAllHostPrefs() {
	HostPrefPtrList r;
	QMap<QString, QString> map = m_config->entryMap("PerHostSettings");
	QStringList keys = map.keys();
	QStringList::iterator it = keys.begin();
	while (it != keys.end()) {
		QString key = *it;
		if (key.endsWith("-exists")) {
			QRegExp re("PerHost-([^-]+)-(.*)-exists");
			if (re.exactMatch(key)) {
				HostPrefPtr hp = getHostPref(re.cap(2), re.cap(1));
				if (hp)
					r += hp;
			}
			
		}
		it++;
	}
	return r;
}

void HostPreferences::removeHostPref(HostPref *hostPref) {
	hostPref->remove();
}

void HostPreferences::setShowBrowsingPanel( bool b )
{
	m_config->setGroup( QString::null );
	m_config->writeEntry( "browsingPanel", b );
}

void HostPreferences::setServerCompletions( const QStringList &list )
{
	m_config->setGroup( QString::null );
	m_config->writeEntry( "serverCompletions", list );
}

void HostPreferences::setServerHistory( const QStringList &list )
{
	m_config->setGroup( QString::null );
	m_config->writeEntry( "serverHistory", list );
}

bool HostPreferences::showBrowsingPanel()
{
	m_config->setGroup( QString::null );
	return m_config->readBoolEntry( "browsingPanel" );
}

QStringList HostPreferences::serverCompletions()
{
	m_config->setGroup( QString::null );
	return m_config->readListEntry( "serverCompletions" );
}

QStringList HostPreferences::serverHistory()
{
	m_config->setGroup( QString::null );
	return m_config->readListEntry( "serverHistory" );
}

void HostPreferences::sync() {
	m_config->sync();
}
