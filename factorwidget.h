/****************************************************************************
**
** Copyright (C) 2021 Rafał Lalik <rafallalik @ gmail.com>
**
** This file is part of KDE.
**
** This program is free software; you can redistribute it and/or modify
** it under the terms of the GNU General Public License as published by
** the Free Software Foundation; either version 2 of the License, or
** (at your option) any later version.
**
** This program is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
** GNU General Public License for more details.
**
** You should have received a copy of the GNU General Public License
** along with this program; see the file COPYING. If not, write to
** the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
** Boston, MA 02110-1301, USA.
**
****************************************************************************/

#ifndef FACTORWIDGET_H
#define FACTORWIDGET_H

#ifndef QTONLY
    #include "krdccore_export.h"
#else
    #define KRDCCORE_EXPORT
#endif

#include <QWidgetAction>

class MainWindow;

/**
 * Widget Action to display slider in the action toolbar. Each time action is
 * add, the new QSlider widget is created via @ref createWidget() method.
 */
class KRDCCORE_EXPORT FactorWidget : public QWidgetAction
{
    Q_OBJECT

public:
    FactorWidget(QWidget *parent = nullptr);
    FactorWidget(const QString &text, MainWindow * receiver, QObject *parent = nullptr);
    ~FactorWidget();

protected:
    virtual QWidget * createWidget(QWidget * parent) override;
    virtual void deleteWidget(QWidget * widget) override;

    MainWindow * m_receiver;
};

#endif
