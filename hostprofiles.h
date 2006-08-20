/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#ifndef HOSTPROFILES_H
#define HOSTPROFILES_H

#include "ui_hostprofiles.h"
#include <QWidget>

/*
  Host profiles (options) dialog
*/
class HostProfiles : public QWidget, public Ui::HostProfiles
{
    Q_OBJECT

public:
    HostProfiles(QWidget* parent = 0);
    ~HostProfiles();

    void load();
    void save();

public slots:
    void removeHost();
    void removeAllHosts();
    void selectionChanged();

private:
    HostPrefPtrList deletedHosts;
};

#endif // HOSTPROFILES_H

