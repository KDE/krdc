/* This file is part of the KDE project
   Copyright (C) 2002-2003 Nadeem Hasan <nhasan@kde.org>

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public
   License as published by the Free Software Foundation; either
   version 2 of the License, or (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; see the file COPYING.  If not, write to
   the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
   Boston, MA 02110-1301, USA.
*/

#ifndef PREFERENCESDIALOG_H
#define PREFERENCESDIALOG_H

#include <kpagedialog.h>

#include "smartptr.h"
#include "vnc/vnchostpref.h"
#include "rdp/rdphostpref.h"

class HostProfiles;
class VncPrefs;
class RdpPrefs;

class PreferencesDialog : public KPageDialog
{
  Q_OBJECT

  public:
    PreferencesDialog( QWidget *parent );
    ~PreferencesDialog() {};

  protected slots:
    void slotOk();

  protected:
    void load();
    void save();

    HostProfiles *m_hostProfiles;
    VncPrefs *m_vncPrefs;
    RdpPrefs *m_rdpPrefs;
    SmartPtr<VncHostPref> m_vncDefaults;
    SmartPtr<RdpHostPref> m_rdpDefaults;
};

#endif // PREFERENCESDIALOG_H

