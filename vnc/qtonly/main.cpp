/*
    SPDX-FileCopyrightText: 2008 Urs Wolfer <uwolfer@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include <QApplication>

#include "krdc_debug.h"
#include "vncview.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    if (QCoreApplication::arguments().count() < 2) {
        qCritical(KRDC)
            << ("Please define an URL as argument. Example: vnc://:password@server:1\n"
                "Optionally, you can define the quality as second argument (1-3, where 1 is the best). Default is 2.");
        return 1;
    }
    VncView vncView(0, QCoreApplication::arguments().at(1));
    vncView.show();
    vncView.start();
    return app.exec();
}
