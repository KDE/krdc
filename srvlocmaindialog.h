/***************************************************************************
             srvlocncdialog.h - NewConnectionDialog with SrvLoc support
                             -------------------
    begin                : Sun Jul 21 14:47:01 CET 2002
    copyright            : (C) 2002 by Tim Jansen
    email                : tim@tjansen.de
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#ifndef SRVLOCMAINDIALOG_H
#define SRVLOCMAINDIALOG_H

#include "maindialog.h"
#include "smartptr.h"
#include <qstringlist.h>

class KServiceLocator;

class SrvLocMainDialog : public MainDialog {
	Q_OBJECT
private:
	SmartPtr<KServiceLocator> m_locator;	
	bool m_scanning;
	bool m_browsing;
	QString m_scope;
	QListViewItem *m_nextItem;

	bool ensureLocatorOpen();
	void errorScanning();
	void finishScanning();

public:
	SrvLocMainDialog(QWidget* parent = 0, const char* name = 0, bool modal = FALSE);
	virtual ~SrvLocMainDialog();

	void enableBrowsingArea(bool enable);
	bool browsing();

public slots:	
	void rescan();

private slots:
	void itemSelected(QListViewItem*);
	void itemDoubleClicked(QListViewItem*);
        void scopeSelected(const QString &string);

	void foundService(QString, int);
	void lastSignalServices(bool);
	void foundScopes(QStringList scopeList);
};

#endif
