/*
     rdphostpref.h, handles preferences for RDP hosts
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

#ifndef RDPHOSTPREF_H
#define RDPHOSTPREF_H

#include "hostpreferences.h"

static const QString rdpKeymaps[] = { "ar",
                                      "da",
                                      "de",
                                      "en-gb",
                                      "en-us",
                                      "es",
                                      "fi",
                                      "fr",
                                      "fr-be",
                                      "hr",
                                      "hu",
                                      "it",
                                      "ja",
                                      "lt",
                                      "lv",
                                      "mk",
                                      "no",
                                      "pl",
                                      "pt",
                                      "pt-br",
                                      "ru",
                                      "sl",
                                      "sv",
                                      "th",
                                      "tr" };
static const int rdpNumKeymaps = 25;
static const int rdpDefaultKeymap = 4; // en-us

inline int keymap2int(QString &keymap)
{
	int layout;
	for(layout = 0; layout < rdpNumKeymaps; layout++)
	{
		if(keymap == rdpKeymaps[layout])
		{
			break;
		}
	}
	if(layout == rdpNumKeymaps)
	{
		layout = rdpDefaultKeymap;
	}
	return layout;
}

inline QString int2keymap(int layout)
{
	if(layout < 0 || layout >= rdpNumKeymaps)
	{
		return rdpKeymaps[rdpDefaultKeymap];
	}

	return rdpKeymaps[layout];
}

class RdpHostPref : public HostPref
{
	protected:
		friend class HostPreferences;

		int      m_width;
		int      m_height;
		QString  m_layout;
		bool     m_askOnConnect;

		virtual void load();
		virtual void setDefaults();
		virtual void save();
		virtual void remove();

	public:
		static const QString RdpType;

		RdpHostPref(KConfig *conf, const QString &host, const QString &type);
		virtual ~RdpHostPref();

		virtual QString  prefDescription() const;
		void             setWidth(int w);
		int              width() const;
		void             setHeight(int h);
		int              height() const;
		void             setLayout(QString &l);
		QString          layout() const;
		void             setAskOnConnect(bool ask);
		bool             askOnConnect() const;
};

#endif
