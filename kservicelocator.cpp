/*
 *  Interface to find SLP services.
 *  Copyright (C) 2002 Tim Jansen <tim@tjansen.de>
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Library General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Library General Public License for more details.
 *
 *  You should have received a copy of the GNU Library General Public License
 *  along with this library; see the file COPYING.LIB.  If not, write to
 *  the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 *  Boston, MA 02110-1301, USA.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "kservicelocator.h"
#include <kdebug.h>
#include <qregexp.h>
#include <qapplication.h>
//Added by qt3to4:
#include <QCustomEvent>

#ifdef HAVE_SLP
#include <slp.h>
#include <qevent.h>
#include <qthread.h>
#endif

const QString KServiceLocator::DEFAULT_AUTHORITY = "";
const QString KServiceLocator::ALL_AUTHORITIES = QString::null;

class AsyncThread;

class KServiceLocatorPrivate {
public:
	KServiceLocator *m_ksl;
        bool m_opened;
	volatile bool m_abort;
	QString m_lang;
	bool m_async;

	bool ensureOpen();

	KServiceLocatorPrivate(KServiceLocator *ksl,
			       const QString &lang,
			       bool async);
	~KServiceLocatorPrivate();

#ifdef HAVE_SLP
        SLPHandle m_handle;
	AsyncThread *m_thread;
	QString m_operationServiceUrl; // for findAttributes()/foundAttributes()

	friend class FindSrvTypesThread;	
	friend class FindSrvsThread;	
	friend class FindAttrsThread;	
	friend class FindScopesThread;	

	friend SLPBoolean srvTypeCallback(SLPHandle hslp, 
					  const char* srvtypes, 
					  SLPError errcode, 
					  void* cookie);
	friend SLPBoolean srvTypeCallbackAsync(SLPHandle hslp, 
					       const char* srvtypes, 
					       SLPError errcode, 
					       void* cookie);
	friend SLPBoolean srvUrlCallback(SLPHandle hslp, 
					 const char* srvurl,
					 unsigned short lifetime,
					 SLPError errcode, 
					 void* cookie);
	friend SLPBoolean srvUrlCallbackAsync(SLPHandle hslp, 
					      const char* srvurl,
					      unsigned short lifetime,
					      SLPError errcode, 
					      void* cookie);
	friend SLPBoolean srvAttrCallback(SLPHandle hslp, 
					  const char* attrlist,
					  SLPError errcode, 
					  void* cookie);
	friend SLPBoolean srvAttrCallbackAsync(SLPHandle hslp, 
					       const char* attrlist,
					       SLPError errcode, 
					       void* cookie);

	SLPBoolean handleSrvTypeCallback(const char* srvtypes, 
					 SLPError errcode);
	SLPBoolean handleSrvTypeCallbackAsync(const char* srvtypes, 
					      SLPError errcode);
	SLPBoolean handleSrvUrlCallback(const char* srvUrl,
					unsigned short lifetime,
					SLPError errcode);
	SLPBoolean handleSrvUrlCallbackAsync(const char* srvUrl,
					     unsigned short lifetime,
					     SLPError errcode);
	SLPBoolean handleAttrCallback(const char* attrlist,
				      SLPError errcode);
	SLPBoolean handleAttrCallbackAsync(const char* attrlist,
					   SLPError errcode);
#endif
};

KServiceLocator::KServiceLocator(const QString &lang, bool async) :
	QObject(0, "KServiceLocator") {

	d = new KServiceLocatorPrivate(this, lang, async);
}

KServiceLocatorPrivate::KServiceLocatorPrivate(KServiceLocator *ksl,
					       const QString &lang,
					       bool async) :
	m_ksl(ksl),
	m_opened(false),
	m_abort(false),
	m_lang(lang),
	m_async(async) {

#ifdef HAVE_SLP
	m_thread = 0;
#endif
}


#ifdef HAVE_SLP   /** The real SLP implementations ********************** */


/*   ****** *** ****** *** ******   */
/*     Signals for async events     */
/*   ****** *** ****** *** ******   */

const int MinLastSignalEventType = 45001;
const int LastServiceTypeSignalEventType = 45001;
const int LastServiceSignalEventType = 45002;
const int LastAttributesSignalEventType = 45003;
const int MaxLastSignalEventType = 45003;
class LastSignalEvent : public QCustomEvent
{
private:
	bool m_success;
public:
	LastSignalEvent(int type, bool s) : 
		QCustomEvent(type), 
		m_success(s)
	{};
	int success() const { return m_success; };
};

const int FoundServiceTypesEventType = 45012;
class FoundServiceTypesEvent : public QCustomEvent
{
private:
	QString m_srvtypes;
public:
	FoundServiceTypesEvent(const char *srvtypes) : 
		QCustomEvent(FoundServiceTypesEventType), 
		m_srvtypes(srvtypes)
	{};
	QString srvtypes() const { return m_srvtypes; };
};

const int FoundServiceEventType = 45013;
class FoundServiceEvent : public QCustomEvent
{
private:
	QString m_srvUrl;
	unsigned short m_lifetime;
public:
	FoundServiceEvent(const char *srvUrl, unsigned short lifetime) : 
		QCustomEvent(FoundServiceEventType), 
		m_srvUrl(srvUrl),
		m_lifetime(lifetime)
	{};
	QString srvUrl() const { return m_srvUrl; };
	unsigned short lifetime() const { return m_lifetime; };
};

const int FoundAttributesEventType = 45014;
class FoundAttributesEvent : public QCustomEvent
{
private:
	QString m_attributes;
public:
	FoundAttributesEvent(const char *attributes) : 
		QCustomEvent(FoundAttributesEventType), 
		m_attributes(attributes)
	{};
	QString attributes() const { return m_attributes; };
};

const int FoundScopesEventType = 45015;
class FoundScopesEvent : public QCustomEvent
{
private:
	QString m_scopes;
public:
	FoundScopesEvent(const char *scopes) : 
		QCustomEvent(FoundScopesEventType), 
		m_scopes(scopes)
	{};
	QString scopes() const { return m_scopes; };
};



/*   ****** *** ****** *** ******   */
/*             Callbacks            */
/*   ****** *** ****** *** ******   */

SLPBoolean srvTypeCallback(SLPHandle, 
			   const char* srvtypes, 
			   SLPError errcode, 
			   void* cookie) {
	KServiceLocatorPrivate *ksl = (KServiceLocatorPrivate*) cookie;
	return ksl->handleSrvTypeCallback(srvtypes, errcode);
}
SLPBoolean srvTypeCallbackAsync(SLPHandle, 
				const char* srvtypes, 
				SLPError errcode, 
				void* cookie) {
	KServiceLocatorPrivate *ksl = (KServiceLocatorPrivate*) cookie;
	if ((errcode != SLP_OK) || ksl->m_abort) {
		QApplication::postEvent(ksl->m_ksl, new LastSignalEvent(LastServiceTypeSignalEventType,
						   (errcode == SLP_OK) || 
						   (errcode == SLP_LAST_CALL) || 
						   ksl->m_abort));
		return SLP_FALSE;
	}
	QApplication::postEvent(ksl->m_ksl, new FoundServiceTypesEvent(srvtypes));
	return SLP_TRUE;
}

SLPBoolean srvUrlCallback(SLPHandle, 
			  const char* srvurl,
			  unsigned short lifetime,
			  SLPError errcode, 
			  void* cookie) {
	KServiceLocatorPrivate *ksl = (KServiceLocatorPrivate*) cookie;
	return ksl->handleSrvUrlCallback(srvurl, lifetime, errcode);
}
SLPBoolean srvUrlCallbackAsync(SLPHandle, 
			       const char* srvurl,
			       unsigned short lifetime,
			       SLPError errcode, 
			       void* cookie) {
	KServiceLocatorPrivate *ksl = (KServiceLocatorPrivate*) cookie;
	if ((errcode != SLP_OK) || ksl->m_abort) {
               QApplication::postEvent(ksl->m_ksl, new LastSignalEvent(LastServiceSignalEventType,
						  (errcode == SLP_OK) ||
						  (errcode == SLP_LAST_CALL) ||
						  ksl->m_abort));
		return SLP_FALSE;
	}
	QApplication::postEvent(ksl->m_ksl, 
				new FoundServiceEvent(srvurl, lifetime));
	return SLP_TRUE;
}

SLPBoolean srvAttrCallback(SLPHandle, 
			   const char* attrlist,
			   SLPError errcode, 
			   void* cookie) {
	KServiceLocatorPrivate *ksl = (KServiceLocatorPrivate*) cookie;
	return ksl->handleAttrCallback(attrlist, errcode);
}
SLPBoolean srvAttrCallbackAsync(SLPHandle, 
				const char* attrlist,
				SLPError errcode, 
				void* cookie) {
	KServiceLocatorPrivate *ksl = (KServiceLocatorPrivate*) cookie;
	if ((errcode != SLP_OK) || ksl->m_abort) {
		QApplication::postEvent(ksl->m_ksl, 
					new LastSignalEvent(LastAttributesSignalEventType,
							    (errcode == SLP_OK) ||
							    (errcode == SLP_LAST_CALL) ||
							    ksl->m_abort));
		return SLP_FALSE;
	}
	QApplication::postEvent(ksl->m_ksl, 
				new FoundAttributesEvent(attrlist));
	return SLP_TRUE;
}



/*   ****** *** ****** *** ******   */
/*     Threads for async events     */
/*   ****** *** ****** *** ******   */

class AsyncThread : public QThread {
protected:
	SLPHandle m_handle;
	KServiceLocatorPrivate *m_parent;
	AsyncThread(SLPHandle handle, KServiceLocatorPrivate *parent) :
		m_handle(handle), 
		m_parent(parent) {		
	}
};
class FindSrvTypesThread : public AsyncThread {
	QString m_namingAuthority, m_scopeList;
public:
	FindSrvTypesThread(SLPHandle handle, 
			   KServiceLocatorPrivate *parent,
			   const char *namingAuthority,
			   const char *scopeList) :
		AsyncThread(handle, parent),
		m_namingAuthority(namingAuthority),
		m_scopeList(scopeList){
	}
      	virtual void run() {
		SLPError e;
		e = SLPFindSrvTypes(m_handle, 
				    m_namingAuthority.latin1(), 
				    m_scopeList.latin1(),
				    srvTypeCallbackAsync, 
				    m_parent);
		if (e != SLP_OK)
			QApplication::postEvent(m_parent->m_ksl, 
						new LastSignalEvent(LastServiceTypeSignalEventType, 
								    false));
	}
};
class FindSrvsThread : public AsyncThread {
	QString m_srvUrl, m_scopeList, m_filter;
public:
	FindSrvsThread(SLPHandle handle, 
		       KServiceLocatorPrivate *parent,
		       const char *srvUrl,
		       const char *scopeList,
		       const char *filter) :
		AsyncThread(handle, parent),
		m_srvUrl(srvUrl),
		m_scopeList(scopeList),
		m_filter(filter) {
	}
      	virtual void run() {
		SLPError e;

		e = SLPFindSrvs(m_handle, 
				m_srvUrl.latin1(), 
				m_scopeList.isNull() ? "" : m_scopeList.latin1(), 
				m_filter.isNull() ? "" : m_filter.latin1(), 
				srvUrlCallbackAsync, 
				m_parent);
		if (e != SLP_OK)
			QApplication::postEvent(m_parent->m_ksl, 
						new LastSignalEvent(LastServiceSignalEventType, 
								    false));
	}
};
class FindAttrsThread : public AsyncThread {
	QString m_srvUrl, m_scopeList, m_attrIds;
public:
	FindAttrsThread(SLPHandle handle, 
		       KServiceLocatorPrivate *parent,
		       const char *srvUrl,
		       const char *scopeList,
		       const char *attrIds) :
		AsyncThread(handle, parent),
		m_srvUrl(srvUrl),
		m_scopeList(scopeList),
		m_attrIds(attrIds) {
	}
      	virtual void run() {
		SLPError e;
		e = SLPFindAttrs(m_handle, 
				 m_srvUrl.latin1(),
				 m_scopeList.isNull() ? "" : m_scopeList.latin1(),
				 m_attrIds.isNull() ? "" : m_attrIds.latin1(),
				 srvAttrCallbackAsync,
				 m_parent);
		if (e != SLP_OK)
			QApplication::postEvent(m_parent->m_ksl, 
						new LastSignalEvent(LastAttributesSignalEventType, 
								    false));
	}
};
class FindScopesThread : public AsyncThread {
public:
	FindScopesThread(SLPHandle handle, 
			 KServiceLocatorPrivate *parent) :
		AsyncThread(handle, parent){
	}
      	virtual void run() {
		SLPError e;
		char *_scopelist;

		e = SLPFindScopes(m_handle, &_scopelist);
		if (e != SLP_OK) {
                        QApplication::postEvent(m_parent->m_ksl, 
						new FoundScopesEvent(""));
			return;
		}

		QString scopeList(_scopelist);
		SLPFree(_scopelist);
                QApplication::postEvent(m_parent->m_ksl, 
					new FoundScopesEvent(scopeList.latin1()));
	}
};

KServiceLocatorPrivate::~KServiceLocatorPrivate() {
	if (m_thread) {
		m_abort = true;
		m_thread->wait();
		delete m_thread;
		m_thread = 0; // important, because event handler will run
	}
	if (m_opened)
		SLPClose(m_handle);
}

KServiceLocator::~KServiceLocator() {
	delete d;
}
	
bool KServiceLocator::available() {
	return d->ensureOpen();
}

void KServiceLocator::abortOperation() {
	d->m_abort = true;
}

bool KServiceLocatorPrivate::ensureOpen() {
	SLPError e;

	if (m_opened)
		return true;
	e = SLPOpen(m_lang.latin1(), SLP_FALSE, &m_handle);
	if (e != SLP_OK) {
		kdError() << "KServiceLocator: error while opening:" << e <<endl;
		return false;
	}
	m_opened = true;
	return true;
}

bool KServiceLocator::findServiceTypes(const QString &namingAuthority,
				       const QString &scopeList) {
	if (!d->ensureOpen())
		return false;
	if (d->m_thread)
		return false;
	d->m_abort = false;

	if (d->m_async) {
		d->m_thread = new FindSrvTypesThread(d->m_handle, 
				    d,
				    namingAuthority.isNull() ? "*" : namingAuthority.latin1(),
				    scopeList.isNull() ? "" : scopeList.latin1());
		d->m_thread->start();
		return true;
	}
	else {
		SLPError e;
		e = SLPFindSrvTypes(d->m_handle, 
				    namingAuthority.isNull() ? "*" : namingAuthority.latin1(), 
				    scopeList.isNull() ? "" : scopeList.latin1(), 
				    srvTypeCallback, 
				    d);
		return e == SLP_OK;
	}
}

bool KServiceLocator::findServices(const QString &srvtype, 
				   const QString &filter, 
				   const QString &scopeList) {
	if (!d->ensureOpen())
		return false;
	if (d->m_thread)
		return false;
	d->m_abort = false;

	if (d->m_async) {
		d->m_thread = new FindSrvsThread(d->m_handle, 
						 d,
						 srvtype.latin1(), 
						 scopeList.isNull() ? "" : scopeList.latin1(), 
						 filter.isNull() ? "" : filter.latin1());
		d->m_thread->start();
		return true;
	}
	else {
		SLPError e;
		e = SLPFindSrvs(d->m_handle, 
				srvtype.latin1(), 
				scopeList.isNull() ? "" : scopeList.latin1(), 
				filter.isNull() ? "" : filter.latin1(), 
				srvUrlCallback, 
				d);
		return e == SLP_OK;
	}
}

bool KServiceLocator::findAttributes(const QString &serviceUrl,
				     const QString &attributeIds) {
	if (!d->ensureOpen())
		return false;
	if (d->m_thread)
		return false;
	d->m_abort = false;

	d->m_operationServiceUrl = serviceUrl;
	if (d->m_async) {
		d->m_thread = new FindAttrsThread(d->m_handle, 
						  d,
						  serviceUrl.latin1(), 
						  "",
						  attributeIds.isNull() ? "" : attributeIds.latin1());
		d->m_thread->start();
		return true;
	}
	else {
		SLPError e;
		e = SLPFindAttrs(d->m_handle, 
				 serviceUrl.latin1(), 
				 "",
				 attributeIds.isNull() ? "" : attributeIds.latin1(), 
				 srvAttrCallback, 
				 d);
		return e == SLP_OK;
	}
}

bool KServiceLocator::findScopes() {
	if (!d->ensureOpen())
		return false;
	if (d->m_thread)
		return false;
	d->m_abort = false;

	if (d->m_async) {
		d->m_thread = new FindScopesThread(d->m_handle, d);
		d->m_thread->start();
		return true;
	}
	else {
		SLPError e; 
		char *_scopelist;
		QStringList scopeList;
		e = SLPFindScopes(d->m_handle, &_scopelist);
		if (e != SLP_OK)
			return false;
		scopeList = parseCommaList(_scopelist);
		SLPFree(_scopelist);
		emit foundScopes(scopeList);
		return true;
	}
}

SLPBoolean KServiceLocatorPrivate::handleSrvTypeCallback(const char* srvtypes, 
							 SLPError errcode) {
	if ((errcode != SLP_OK) || m_abort) {
		m_ksl->emitLastServiceTypeSignal((errcode == SLP_OK) || 
						  (errcode == SLP_LAST_CALL) || 
						  m_abort);
		return SLP_FALSE;
	}
	m_ksl->emitFoundServiceTypes(srvtypes);
	return SLP_TRUE;
}
	
SLPBoolean KServiceLocatorPrivate::handleSrvUrlCallback(const char* srvurl, 
							unsigned short lifetime,
							SLPError errcode) {
	if ((errcode != SLP_OK) || m_abort) {
		m_ksl->emitLastServiceSignal((errcode == SLP_OK) || 
					      (errcode == SLP_LAST_CALL) || 
					      m_abort);
		return SLP_FALSE;
	}
	m_ksl->emitFoundService(srvurl, lifetime);
	return SLP_TRUE;
}
	
SLPBoolean KServiceLocatorPrivate::handleAttrCallback(const char* attrlist, 
						      SLPError errcode) {
	if ((errcode != SLP_OK) || m_abort) {
		m_ksl->emitLastAttributesSignal((errcode == SLP_OK) || 
						(errcode == SLP_LAST_CALL) || 
						m_abort);
		return SLP_FALSE;
	}
	m_ksl->emitFoundAttributes(m_operationServiceUrl, attrlist);
	return SLP_TRUE;
}


void KServiceLocator::customEvent(QCustomEvent *e) {
	if ((e->type() >= MinLastSignalEventType) &&
	    (e->type() <= MaxLastSignalEventType)){
		bool s = true;
		if (d->m_thread) {
			d->m_thread->wait();
			delete d->m_thread;
			d->m_thread = 0;
			s = ((LastSignalEvent*)e)->success();
		}
		if (e->type() == LastServiceTypeSignalEventType)
			emit lastServiceTypeSignal(s);
		else if (e->type() == LastServiceSignalEventType)
			emit lastServiceSignal(s);
		else if (e->type() == LastAttributesSignalEventType)
			emit lastAttributesSignal(s);
		else
			kdFatal() << "unmapped last signal type " << e->type()<< endl;
	}
	else if (e->type() == FoundAttributesEventType) {
		emit foundAttributes(d->m_operationServiceUrl, 
				     ((FoundAttributesEvent*)e)->attributes());
	}
	else if (e->type() == FoundServiceEventType) {
		FoundServiceEvent *fse = (FoundServiceEvent*)e;
		emit foundService(fse->srvUrl(), fse->lifetime());
	}
	else if (e->type() == FoundServiceTypesEventType) {
		emit foundServiceTypes(((FoundServiceTypesEvent*)e)->srvtypes());
	}
	else if (e->type() == FoundScopesEventType) {
		if (d->m_thread) {
			d->m_thread->wait();
			delete d->m_thread;
			d->m_thread = 0;
			emit foundScopes(KServiceLocator::parseCommaList(((FoundScopesEvent*)e)->scopes()));
		}
	}
}

QString KServiceLocator::decodeAttributeValue(const QString &value) {
	char *n;
	if (value.isNull())
		return value;
	if (SLPUnescape(value.latin1(), &n, SLP_TRUE) != SLP_OK)
		return QString::null;
	QString r(n);
	SLPFree(n);
	return r;
}

#else /** Empty dummy functions is SLP is not available ************************* */


KServiceLocator::~KServiceLocator() {
}
bool KServiceLocator::available() {
	return false;
}
void KServiceLocator::abortOperation() {
}
bool KServiceLocator::findServiceTypes(const QString &,const QString &) {
	return false;
}
bool KServiceLocator::findServices(const QString &,const QString &,const QString &) {
	return false;
}
bool KServiceLocator::findAttributes(const QString &,const QString &) {
	return false;
}
bool KServiceLocator::findScopes() {
	return false;
}
void KServiceLocator::customEvent(QCustomEvent *) {
}
QString KServiceLocator::decodeAttributeValue(const QString &value) {
	return value;
}

#endif 


/*** Private emit-helpers ***/

void KServiceLocator::emitFoundServiceTypes(QString serviceTypes) {
	emit foundServiceTypes(serviceTypes);
}
void KServiceLocator::emitFoundService(QString serviceUrl,
				       int lifetime) {
	emit foundService(serviceUrl, lifetime);
}
void KServiceLocator::emitFoundAttributes(QString serviceUrl,
					  QString attributes) {
	emit foundAttributes(serviceUrl, attributes);
}
void KServiceLocator::emitFoundScopes(QStringList scopeList) {
	emit foundScopes(scopeList);
}
void KServiceLocator::emitLastServiceTypeSignal(bool success) {
	emit lastServiceTypeSignal(success);
}
void KServiceLocator::emitLastServiceSignal(bool success) {
	emit lastServiceSignal(success);
}
void KServiceLocator::emitLastAttributesSignal(bool success) {
	emit lastAttributesSignal(success);
}



/*** Static helpers ***/

void KServiceLocator::parseAttributeList(const QString &attributes, 
					 QMap<QString,QString> &attributeMap) {
	QRegExp r("\\((.*)=(.*)\\)");
	r.setMinimal(true);
	int pos = 0;
	while (pos >= 0) {
		pos = r.search(attributes, pos);
		if (pos != -1) {
			attributeMap[r.cap(1)] = r.cap(2);
			pos  += r.matchedLength();
		}
	}
}

QStringList KServiceLocator::parseCommaList(const QString &list) {
	return QStringList::split(QChar(','), list);
}

QString KServiceLocator::createCommaList(const QStringList &values) {
	return values.join(",");
}

QString KServiceLocator::escapeFilter(const QString &str) {
	QString f;
	int s = str.length();
	for (int i = 0; i < s; i++) {
		char c = str[i];
		switch(c) {
		case '*':
			f.append("\2a");
			break;
		case '(':
			f.append("\28");
			break;
		case ')':
			f.append("\29");
			break;
		case '\\':
			f.append("\5c");
			break;
		case 0:
			f.append("\2a");
			break;
		default:
			f.append(c);
			break;
		}
	}
	return f;
}

#include "kservicelocator.moc"

