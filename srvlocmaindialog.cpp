/***************************************************************************
             srvlocncdialog.cpp - MainDialog with SrvLoc support
                             -------------------
    begin                : Sun Jul 21 14:51:47 CET 2002
    copyright            : (C) 2002-2003 by Tim Jansen
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

#include "srvlocmaindialog.h"
#include "kservicelocator.h"
#include <kurl.h>
#include <klocale.h>
#include <kdebug.h>
#include <klistview.h>
#include <klineedit.h>
#include <kcombobox.h>
#include <kmessagebox.h>
#include <qmap.h>
#include <qregexp.h>
#include <qpushbutton.h>

static const QString DEFAULT_SCOPE = "default";

class UrlListViewItem : public KListViewItem {
	QString m_url;
	QString m_serviceid;
public:
	UrlListViewItem(QListView *v, 
			const QString &url,
			const QString &host,
			const QString &protocol,
			const QString &type, 
			const QString &userid,
			const QString &fullname,
			const QString &desc,
			const QString &serviceid) :
		KListViewItem(v, host, 
			      i18n("unknown"), 
			      host, protocol),
		m_url(url),
		m_serviceid(serviceid) {
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

SrvLocMainDialog::SrvLocMainDialog(QWidget* parent, const char* name, bool browsing) :
	MainDialog(parent, name, true),
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
	connect(searchInput, SIGNAL(returnPressed(const QString&)),
		SLOT(rescan()));
	searchInput->setTrapReturnKey(true);
	if (!browsing) {
		enableBrowsingArea(false);
		adjustSize();
	}
	else
		rescan();
}

SrvLocMainDialog::~SrvLocMainDialog() {
}

void SrvLocMainDialog::enableBrowsingArea(bool enable) {
	m_browsing = enable;
	MainDialog::enableBrowsingArea(enable);
}

bool SrvLocMainDialog::browsing() {
	return m_browsing;
}

void SrvLocMainDialog::itemSelected(QListViewItem *it) {
	UrlListViewItem *u = (UrlListViewItem*) it;
	QRegExp rx("^service:remotedesktop\\.kde:([^;]*)");
	if (rx.search(u->url()) < 0)
		return;
	serverInput->setCurrentText(rx.cap(1));
}

void SrvLocMainDialog::itemDoubleClicked(QListViewItem *it) {
	itemSelected(it);
	accept();
}

void SrvLocMainDialog::scopeSelected(const QString &string) {
	QString s = string;
	if (s == i18n("default"))
		s = DEFAULT_SCOPE;

	if (m_scope == s)
		return;
	m_scope = s;
	rescan();
}

bool SrvLocMainDialog::ensureLocatorOpen() {
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

void SrvLocMainDialog::rescan() {
	QStringList scopeList;

	if (m_scanning)
		return;
	m_scanning = true;
	rescanButton->setEnabled(false);
	scopeCombo->setEnabled(false);
	if (!ensureLocatorOpen())
		return;

	browsingView->clear();

	QString filter = QString::null;
	if (!searchInput->text().stripWhiteSpace().isEmpty()) {
		QString ef = KServiceLocator::escapeFilter(
			searchInput->text().stripWhiteSpace());
		filter = "(|(|(description=*"+ef+"*)(username=*"+ef+"*))(fullname=*"+ef+"*))";
	}

	if (!m_locator->findServices("service:remotedesktop.kde",
				     filter,
				     m_scope)) {
		kdWarning() << "Failure in findServices()" << endl;
		errorScanning();
		return;
	}
}

void SrvLocMainDialog::foundService(QString url, int) {
	QRegExp rx("^service:remotedesktop\\.kde:(\\w+)://([^;]+);(.*)$");
	if (rx.search(url) < 0)
		return;

	QMap<QString,QString> map;
	KServiceLocator::parseAttributeList(rx.cap(3), map);

	new UrlListViewItem(browsingView, url, rx.cap(2), rx.cap(1),
			    KServiceLocator::decodeAttributeValue(map["type"]), 
			    KServiceLocator::decodeAttributeValue(map["username"]), 
			    KServiceLocator::decodeAttributeValue(map["fullname"]),
			    KServiceLocator::decodeAttributeValue(map["description"]),
			    KServiceLocator::decodeAttributeValue(map["serviceid"]));
}

void SrvLocMainDialog::errorScanning() {
	KMessageBox::error(0, i18n("An error occurred while scanning the network."),
			   i18n("Error While Scanning"), false);
	finishScanning();
}

void SrvLocMainDialog::finishScanning() {
	rescanButton->setEnabled(true);
	scopeCombo->setEnabled(true);
	m_scanning = false;
}

void SrvLocMainDialog::lastSignalServices(bool success) {
	if (!success) {
		errorScanning();
		return;
	}

	if (!m_locator->findScopes()) {
		kdWarning() << "Failure in findScopes()" << endl;
		errorScanning();
	}
}

void SrvLocMainDialog::foundScopes(QStringList scopeList) {
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



#include "srvlocmaindialog.moc"

