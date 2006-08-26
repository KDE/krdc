/* This file is part of the KDE project
   Copyright (C) 2003-2004 Nadeem Hasan <nhasan@kde.org>

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

#ifndef MAINDIALOG_H
#define MAINDIALOG_H

#include <kdialog.h>

class MainDialogWidget;

class MainDialog : public KDialog
{
    Q_OBJECT
  public:
    MainDialog( QWidget *parent, const char *name=0 );
    ~MainDialog() {}

    void setRemoteHost( const QString & );
    QString remoteHost();

  protected slots:
    virtual void slotOk();
    virtual void slotClose();
    virtual void slotUser1();
    virtual void slotHelp();
    void adjustWidgetSize();

  protected:
    MainDialogWidget *m_dialogWidget;
};

#endif // MAINDIALOG_H

