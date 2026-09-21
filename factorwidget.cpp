/*
    SPDX-FileCopyrightText: 2021 Rafał Lalik <rafallalik@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "factorwidget.h"

#include "mainwindow.h"

#include <QSlider>
#include <QToolBar>

#include <cstdio>

FactorWidget::FactorWidget(QWidget *parent)
    : QWidgetAction(parent)
{
}

FactorWidget::FactorWidget(const QString &text, MainWindow *receiver, QObject *parent)
    : QWidgetAction(parent)
    , m_receiver(receiver)
{
    setText(text);
}

FactorWidget::~FactorWidget()
{
}

/**
 * Each time action is add, the new QSlider widget is created and return. As
 * the toolbar takes ownership of the widget, here we must take care of
 * connecting signals and slots to the MainWindow methods.
 *
 * The widget has by default 100 steps, and width of 100.
 */
QWidget *FactorWidget::createWidget(QWidget *parent)
{
    QToolBar *_parent = qobject_cast<QToolBar *>(parent);
    if (!_parent) {
        return QWidgetAction::createWidget(parent);
    }

    QSlider *s = new QSlider(_parent->orientation(), _parent);
    s->setRange(100, 200);

    // Follow the toolbar's orientation and only limit the slider along that axis, so a
    // toolbar docked left/right gets a short vertical slider instead of a wide one.
    auto applyOrientation = [s](Qt::Orientation orientation) {
        s->setOrientation(orientation);
        if (orientation == Qt::Vertical) {
            s->setMaximumSize(QWIDGETSIZE_MAX, 100);
        } else {
            s->setMaximumSize(100, QWIDGETSIZE_MAX);
        }
    };
    applyOrientation(_parent->orientation());
    connect(_parent, &QToolBar::orientationChanged, s, applyOrientation);

    connect(s, &QSlider::valueChanged, m_receiver, &MainWindow::setFactor);
    connect(m_receiver, &MainWindow::factorUpdated, s, &QSlider::setValue);
    connect(m_receiver, &MainWindow::scaleUpdated, s, &QSlider::setEnabled);

    return s;
}

void FactorWidget::deleteWidget(QWidget *widget)
{
    disconnect(widget);
    QWidgetAction::deleteWidget(widget);
}
