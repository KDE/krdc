/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#ifndef RDPPREFS_H
#define RDPPREFS_H

#include "ui_rdpprefs.h"
#include <QWidget>

/*
  RDP preferences dialog
*/
class RdpPrefs : public QWidget, public Ui::RdpPrefs
{
    Q_OBJECT

public:
    RdpPrefs(QWidget* parent);
    ~RdpPrefs();

    void setRdpWidth( int w );
    int rdpWidth();
    void setRdpHeight( int h );
    int rdpHeight();
    void setResolution();
    int resolution();
    void setKbLayout( int i );
    int kbLayout();
    void setShowPrefs( bool b );
    bool showPrefs();
    void setUseKWallet( bool b );
    bool useKWallet();

public slots:
    int colorDepth();
    void setColorDepth( int depth );
    void resolutionChanged( int selection );
};

#endif
