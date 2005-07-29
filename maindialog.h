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
   the Free Software Foundation, Inc., 51 Franklin Steet, Fifth Floor,
   Boston, MA 02110-1301, USA.
*/

#ifndef MAINDIALOG_H
#define MAINDIALOG_H

#include <kdialogbase.h>

class MainDialogWidget;

class MainDialog : public KDialogBase
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

  protected:
    MainDialogWidget *m_dialogWidget;
};

#endif // MAINDIALOG_H

