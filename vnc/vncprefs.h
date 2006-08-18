/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#ifndef VNCPREFS_H
#define VNCPREFS_H

#include "ui_vncprefs.h"
#include <QWidget>


/*
  VNC preferences dialog
*/
class VncPrefs : public QWidget, public Ui::VncPrefs
{
  Q_OBJECT

public:
  VncPrefs( QWidget *parent );
  void setQuality( int quality );
  int quality();
  void setShowPrefs( bool b );
  int showPrefs(); // should return bool?
  void setUseEncryption( bool b );
  bool useEncryption();
  void setUseKWallet( bool b );
  bool useKWallet();
};

#endif
