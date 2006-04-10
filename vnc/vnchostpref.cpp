/***************************************************************************
                      vnchostprefs.cpp  -  vnc host preferences
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

#include "vnchostpref.h"
#include <kconfig.h>
#include <klocale.h>

const QString VncHostPref::VncType = "VNC";

VncHostPref::VncHostPref(KConfig *conf, const QString &host, const QString &type) :
	HostPref(conf, host, type),
	m_quality(0),
	m_useKWallet(true),
	m_askOnConnect(true) {
}

VncHostPref::~VncHostPref() {
}

void VncHostPref::save() {
	if ( !m_host.isEmpty() && !m_type.isEmpty() )
	{
		m_config->setGroup("PerHostSettings");
		QString p = prefix();
		m_config->writeEntry(p+"exists", true);
		m_config->writeEntry(p+"quality", m_quality);
		m_config->writeEntry(p+"askOnConnect", m_askOnConnect);
		m_config->writeEntry(p+"useKWallet", m_useKWallet);
	}
	else
	{
		m_config->setGroup( "VncDefaultSettings" );
		m_config->writeEntry( "vncQuality", m_quality );
		m_config->writeEntry( "vncShowHostPreferences", m_askOnConnect );
		m_config->writeEntry( "vncUseKWallet", m_useKWallet );
	}
}

void VncHostPref::load() {
	if ( !m_host.isEmpty() && !m_type.isEmpty() )
	{
		m_config->setGroup("PerHostSettings");
		QString p = prefix();
		m_quality = m_config->readNumEntry(p+"quality", 0);
		m_askOnConnect = m_config->readBoolEntry(p+"askOnConnect", true);
		m_useKWallet = m_config->readBoolEntry(p+"useKWallet", true);
	}
	else
	{
		setDefaults();
	}
}

void VncHostPref::remove() {
	m_config->setGroup("PerHostSettings");
	QString p = prefix();
	m_config->deleteEntry(p+"exists");
	m_config->deleteEntry(p+"quality");
	m_config->deleteEntry(p+"askOnConnect");
}

void VncHostPref::setDefaults() {
	m_config->setGroup("VncDefaultSettings");
	m_quality = m_config->readNumEntry("vncQuality", 0);
	m_askOnConnect = m_config->readBoolEntry("vncShowHostPreferences", true);
	m_useKWallet = m_config->readBoolEntry("vncUseKWallet", true);
}

QString VncHostPref::prefDescription() const {
	QString q;
	switch(m_quality) {
	case 0:
		q = i18n("High");
		break;
	case 1:
		q = i18n("Medium");
		break;
	case 2:
		q = i18n("Low");
		break;
	default:
		Q_ASSERT(true);
	}
	return i18n("Show Preferences: %1, Quality: %2, KWallet: %3",
	   m_askOnConnect ? i18n("yes") : i18n("no"), q, m_useKWallet ? i18n("yes") : i18n("no"));
}

void VncHostPref::setQuality(int q) {
	m_quality = q;
	save();
}

int VncHostPref::quality() const {
	return m_quality;
}

void VncHostPref::setAskOnConnect(bool ask) {
	m_askOnConnect = ask;
	save();
}

bool VncHostPref::askOnConnect() const {
	return m_askOnConnect;
}

void VncHostPref::setUseKWallet(bool use) {
	m_useKWallet = use;
	save();
}

bool VncHostPref::useKWallet() const {
	return m_useKWallet;
}
