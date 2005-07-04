/*
     rdphostpref.cpp, handles preferences for RDP hosts
     Copyright (C) 2003 Arend van Beelen jr.

     This program is free software; you can redistribute it and/or modify it under the terms of the
     GNU General Public License as published by the Free Software Foundation; either version 2 of
     the License, or (at your option) any later version.

     This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
     without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See
     the GNU General Public License for more details.

     You should have received a copy of the GNU General Public License along with this program; if
     not, write to the Free Software Foundation, Inc., 59 Temple Place, Suite 330, Boston,
     MA 02111-1307 USA

     For any questions, comments or whatever, you may mail me at: arend@auton.nl
*/

#include "rdphostpref.h"
#include <kconfig.h>
#include <klocale.h>

const QString RdpHostPref::RdpType = "RDP";

RdpHostPref::RdpHostPref(KConfig *conf, const QString &host, const QString &type) :
	HostPref(conf, host, type),
	m_width(800),
	m_height(600),
	m_colorDepth(8),
	m_layout("en-us"),
	m_useKWallet(true),
	m_askOnConnect(true)
{
}

RdpHostPref::~RdpHostPref()
{
}

void RdpHostPref::save()
{
	if ( !m_host.isEmpty() && !m_type.isEmpty() )
	{
		m_config->setGroup("PerHostSettings");
		QString p = prefix();
		m_config->writeEntry(p+"exists", true);
		m_config->writeEntry(p+"width", m_width);
		m_config->writeEntry(p+"height", m_height);
		m_config->writeEntry(p+"colorDepth", m_colorDepth);
		m_config->writeEntry(p+"layout", m_layout);
		m_config->writeEntry(p+"askOnConnect", m_askOnConnect);
		m_config->writeEntry(p+"useKWallet", m_useKWallet);
	}
	else
	{
		m_config->setGroup( "RdpDefaultSettings" );
		m_config->writeEntry( "rdpWidth", m_width );
		m_config->writeEntry( "rdpHeight", m_height );
		m_config->writeEntry( "rdpColorDepth", m_colorDepth);
		m_config->writeEntry( "rdpKeyboardLayout", m_layout );
		m_config->writeEntry( "rdpShowHostPreferences", m_askOnConnect );
		m_config->writeEntry( "rdpUseKWallet", m_useKWallet );
	}
}

void RdpHostPref::load()
{
	if ( !m_host.isEmpty() && !m_type.isEmpty() )
	{
		m_config->setGroup("PerHostSettings");
		QString p = prefix();
		m_width = m_config->readNumEntry(p+"width", 800);
		m_height = m_config->readNumEntry(p+"height", 600);
		m_colorDepth = m_config->readNumEntry(p+"colorDepth", 8);
		m_layout = m_config->readEntry(p+"layout", "en-us");
		m_askOnConnect = m_config->readBoolEntry(p+"askOnConnect", true);
		m_useKWallet = m_config->readBoolEntry(p+"useKWallet", true);
	}
	else
	{
		setDefaults();
	}
}

void RdpHostPref::remove()
{
	m_config->setGroup("PerHostSettings");
	QString p = prefix();
	m_config->deleteEntry(p+"exists");
	m_config->deleteEntry(p+"width");
	m_config->deleteEntry(p+"height");
	m_config->deleteEntry(p+"colorDepth");
	m_config->deleteEntry(p+"layout");
	m_config->deleteEntry(p+"askOnConnect");
}

void RdpHostPref::setDefaults()
{
	m_config->setGroup("RdpDefaultSettings");
	m_width = m_config->readNumEntry("rdpWidth", 800);
	m_height = m_config->readNumEntry("rdpHeight", 600);
	m_colorDepth = m_config->readNumEntry("rdpColorDepth", 8);
	m_layout = m_config->readEntry("rdpLayout", "en-us");
	m_askOnConnect = m_config->readBoolEntry("rdpShowHostPreferences", true);
	m_useKWallet = m_config->readBoolEntry("rdpUseKWallet", true);
}

QString RdpHostPref::prefDescription() const
{
	return i18n("Show Preferences: %1, Resolution: %2x%3, Color Depth: %4, Keymap: %5, KWallet: %6")
	  .arg(m_askOnConnect ? i18n("yes") : i18n("no")).arg(m_width).arg(m_height)
	  .arg(m_colorDepth).arg(m_layout).arg(m_useKWallet ? i18n("yes") : i18n("no"));
}

void RdpHostPref::setWidth(int w)
{
	m_width = w;
	save();
}

int RdpHostPref::width() const
{
	return m_width;
}

void RdpHostPref::setHeight(int h)
{
	m_height = h;
	save();
}

int RdpHostPref::height() const
{
	return m_height;
}

void RdpHostPref::setColorDepth(int d)
{
	m_colorDepth = d;
	save();
}

int RdpHostPref::colorDepth() const
{
	return m_colorDepth;
}


void RdpHostPref::setLayout(const QString &l)
{
	m_layout = l;
	save();
}

QString RdpHostPref::layout() const
{
	return m_layout;
}

void RdpHostPref::setAskOnConnect(bool ask)
{
	m_askOnConnect = ask;
	save();
}

bool RdpHostPref::askOnConnect() const
{
	return m_askOnConnect;
}

void RdpHostPref::setUseKWallet(bool use) {
	m_useKWallet = use;
	save();
}

bool RdpHostPref::useKWallet() const {
	return m_useKWallet;
}