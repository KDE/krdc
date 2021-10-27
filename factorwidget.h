/*
    SPDX-FileCopyrightText: 2021 Rafał Lalik <rafallalik@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

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
    ~FactorWidget() override;

protected:
    virtual QWidget * createWidget(QWidget * parent) override;
    virtual void deleteWidget(QWidget * widget) override;

    MainWindow * m_receiver;
};

#endif
