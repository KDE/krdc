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

#include "factorwidget.h"

#include "mainwindow.h"

#include <QSlider>
#include <QToolBar>

#include <cstdio>

FactorWidget::FactorWidget(QWidget *parent) : QWidgetAction(parent)
{
}

FactorWidget::FactorWidget(const QString& text, MainWindow * receiver, QObject* parent)
    : QWidgetAction(parent)
    , m_receiver(receiver)
{
    setText(text);
}

FactorWidget::~FactorWidget()
{
}

/**
 * Each time action is add, the new QSlider widget is created and retun. As
 * the toolbar takes ownership of the widget, here we must take care of
 * conecting signals and slots to the MainWindow methods.
 *
 * The widget has by default 100 steps, and width of 100.
 */
QWidget * FactorWidget::createWidget(QWidget * parent)
{
    QToolBar *_parent = qobject_cast<QToolBar *>(parent);
    if (!_parent) {
        return QWidgetAction::createWidget(parent);
    }

    QSlider * s = new QSlider(Qt::Horizontal, _parent);
    s->setRange(0, 100);
    s->setMaximumWidth(100);

    connect(s, &QSlider::valueChanged, m_receiver, &MainWindow::setFactor);
    connect(m_receiver, &MainWindow::factorUpdated, s, &QSlider::setValue);
    connect(m_receiver, &MainWindow::scaleUpdated, s, &QSlider::setEnabled);

    return s;
}

void FactorWidget::deleteWidget(QWidget* widget)
{
    disconnect(widget);
    QWidgetAction::deleteWidget(widget);
}
