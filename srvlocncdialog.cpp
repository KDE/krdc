/***************************************************************************
             srvlocncdialog.cpp - NewConnectionDialog with SrvLoc support
                             -------------------
    begin                : Sun Jul 21 14:51:47 CET 2002
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

#include "srvlocncdialog.h"
#include "kservicelocator.h"
#include <kurl.h>
#include <klocale.h>
#include <kdebug.h>
#include <klistview.h>
#include <kcombobox.h>
#include <kmessagebox.h>
#include <qmap.h>
#include <qregexp.h>
#include <qpushbutton.h>

static const QString DEFAULT_SCOPE = "default";

class UrlListViewItem : public KListViewItem {
	QString m_url;
public:
	UrlListViewItem(QListView *v, 
			const QString &url,
			const QString &host,
			const QString &protocol,
			const QString &type, 
			const QString &userid,
			const QString &fullname,
			const QString &desc) :
		KListViewItem(v, host, 
			      i18n("unknown"), 
			      host, protocol),
		m_url(url) {
		if (!type.isNull()) {
//User connects to somebody else's desktop, used for krfb
			if (type.lower() == "shared")
				setText(1, i18n("Shared Desktop"));
//User connects to desktop that exists only on the network
			else if (type.lower() == "private") 
				setText(1, i18n("Standalone Desktop"));
		}
		if (!desc.isNull())
			setText(0, desc);
		if ((!userid.isEmpty()) && (!fullname.isEmpty()))
			setText(0, 
				QString("%1 (%2)").arg(fullname).arg(userid));
		else if (!userid.isNull())
			setText(0, userid);
		else if (!fullname.isNull())
			setText(0, fullname);
	}
	
	QString url() {
		return m_url;
	}
};

SrvLocNCDialog::SrvLocNCDialog(QWidget* parent, const char* name, bool browsing) :
	NewConnectionDialog(parent, name, true),
	m_locator(0),
        m_scanning(false),
	m_browsing(browsing),
	m_scope(QString::null),
	m_nextItem(0) {
	connect(browsingView, SIGNAL(selectionChanged(QListViewItem*)),
		SLOT(itemSelected(QListViewItem*)));
	connect(browsingView, SIGNAL(doubleClicked(QListViewItem*,const QPoint&,int)),
		SLOT(itemDoubleClicked(QListViewItem*)));
	connect(scopeCombo, SIGNAL(activated(const QString&)),
		SLOT(scopeSelected(const QString&)));	
	if (!browsing) {
		enableBrowsingArea(false);
		adjustSize();
	}
	else
		rescan();
}

SrvLocNCDialog::~SrvLocNCDialog() {
	if (m_locator)
		delete m_locator;
}

void SrvLocNCDialog::enableBrowsingArea(bool enable) {
	m_browsing = enable;
	NewConnectionDialog::enableBrowsingArea(enable);
}

bool SrvLocNCDialog::browsing() {
	return m_browsing;
}

void SrvLocNCDialog::itemSelected(QListViewItem *it) {
	UrlListViewItem *u = (UrlListViewItem*) it;
	QRegExp rx("^service:remotedesktop\\.kde:([^;]*)");
	if (rx.search(u->url()) < 0)
		return;
	serverInput->setCurrentText(rx.cap(1));
}

void SrvLocNCDialog::itemDoubleClicked(QListViewItem *it) {
	itemSelected(it);
	accept();
}

void SrvLocNCDialog::scopeSelected(const QString &string) {
	QString s = string;
	if (s == i18n("default"))
		s = DEFAULT_SCOPE;

	if (m_scope == s)
		return;
	m_scope = s;
	rescan();
}

bool SrvLocNCDialog::ensureLocatorOpen() {
	if (m_locator)
		return true;
	m_locator = new KServiceLocator();
	if (!m_locator->available()) {
		KMessageBox::error(0, 
				   i18n("Browsing the network is not possible. You probably did not install SLP support correctly."), 
				   i18n("Browsing Not Possible"), false);
		return false;
	}

	connect(m_locator, SIGNAL(foundService(QString,int)),
		SLOT(foundService(QString,int)));
	connect(m_locator, SIGNAL(lastServiceSignal(bool)),
		SLOT(lastSignalServices(bool)));
	connect(m_locator, SIGNAL(foundScopes(QStringList)),
		SLOT(foundScopes(QStringList)));
	return true;
}

void SrvLocNCDialog::rescan() {
	QStringList scopeList;

	if (m_scanning)
		return;
	m_scanning = true;
	rescanButton->setEnabled(false);
	scopeCombo->setEnabled(false);
	if (!ensureLocatorOpen())
		return;

	browsingView->clear();

	if (!m_locator->findServices("service:remotedesktop.kde:vnc",
				     QString::null,
				     m_scope)) {
		kdWarning() << "Failure in findServices()" << endl;
		errorScanning();
		return;
	}
}

void SrvLocNCDialog::foundService(QString url, int) {
	QRegExp rx("^service:remotedesktop\\.kde:(\\w+)://([^;]+);(.*)$");
	if (rx.search(url) < 0)
		return;

	QMap<QString,QString> map;
	KServiceLocator::parseAttributeList(rx.cap(3), map);

	new UrlListViewItem(browsingView, url, rx.cap(2), rx.cap(1),
			    KServiceLocator::decodeAttributeValue(map["type"]), 
			    KServiceLocator::decodeAttributeValue(map["username"]), 
			    KServiceLocator::decodeAttributeValue(map["fullname"]),
			    KServiceLocator::decodeAttributeValue(map["description"]));

}

void SrvLocNCDialog::errorScanning() {
	KMessageBox::error(0, i18n("An error occurred while scanning the network."),
			   i18n("Error While Scanning"), false);
	finishScanning();
}

void SrvLocNCDialog::finishScanning() {
	rescanButton->setEnabled(true);
	scopeCombo->setEnabled(true);
	m_scanning = false;
}

void SrvLocNCDialog::lastSignalServices(bool success) {
	if (!success) {
		errorScanning();
		return;
	}

	if (!m_locator->findScopes()) {
		kdWarning() << "Failure in findScopes()" << endl;
		errorScanning();
	}
}

void SrvLocNCDialog::foundScopes(QStringList scopeList) {
	if (scopeList.isEmpty())
		return;
	
	int di = scopeList.findIndex(DEFAULT_SCOPE);
	if (di >= 0)
		scopeList[di] = i18n("default");

	int ct = scopeList.findIndex(scopeCombo->currentText());
	scopeCombo->clear();
	scopeCombo->insertStringList(scopeList);
	if (ct >= 0)
		scopeCombo->setCurrentItem(ct);
	finishScanning();
}



#include "srvlocncdialog.moc"

