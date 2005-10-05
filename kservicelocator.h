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

#ifndef __KSERVICELOCATOR_H
#define __KSERVICELOCATOR_H

#include <qobject.h>
#include <qmap.h>
#include <qstringlist.h>
//Added by qt3to4:
#include <QCustomEvent>

class KServiceLocatorPrivate;

/**
 * KServiceLocator allows you to search the network for service types, 
 * services and their attributes using SLP.
 * Note that most find methods can work in a synchronous and a asynchronous 
 * mode. In synchronous mode the find method emits a number of signals and
 * returns. In asynchronous mode the method returns as soon as possible and 
 * will emit the signals either during or after the find method. You can have 
 * only one find search at a given time.
 *
 * @version $Id$
 * @author Tim Jansen, tim@tjansen.de 
 */
class KServiceLocator : public QObject {
  Q_OBJECT
 private:
        friend class KServiceLocatorPrivate;
        KServiceLocatorPrivate *d;

 public:
	/**
	 * Creates a new KServiceLocator.
	 * @param lang the language to search in, or QString::null for the 
	 *             default language
	 * @param async true to create the service locator in asynchronous
	 *                   mode, false otherwise
	 */
        KServiceLocator(const QString &lang = QString::null, 
			bool async = true);

	virtual ~KServiceLocator();
	

	/**
	 * Parses a list of attributes that has been encoded in a string, as
	 * returned by foundServiceAttributes() and foundAttributes().
	 * The format of the encoded attributes is "(name=value), (name=value)".
	 * Note that some attributes contain lists that must be parsed using 
	 * parseCommaList(), and that all values must be decoded using 
	 * decodeAttributeValue().
	 * @param attributes a list of encoded attributes
	 * @param attributeMap the attributes will be added to this map
	 */
	static void parseAttributeList(const QString &attributes, 
				       QMap<QString,QString> &attributeMap);

	/**
	 * Decodes the value of an attribute (removes the escape codes). This
	 * is neccessary even when you parsed an attribute list.
	 * This function requires the presence of the SLP library, otherwise it
	 * will return the original value.
	 * @param value the attribute value to decode
	 * @return the decoded value. If @p value was QString::null or decoding
	 *         failed, QString::null will be returned
	 */
	static QString decodeAttributeValue(const QString &value);

	/**
	 * Parses a comma-separated string of lists, as returned by many signals.
	 * @param list the comma-separated list
	 * @return the items as a QStringList
	 */
	static QStringList parseCommaList(const QString &list);

	/**
	 * Creates a comma-separated string of lists, as required by many functions.
	 * @param map the items of this list will be converted
	 * @return the comma-separated list
	 */
	static QString createCommaList(const QStringList &values);

	/**
	 * Escapes a string for use as part of a filter, as described in
	 * RFC 2254. This will replace all occurrences of special 
	 * characters like paranthesis, backslash and "*", so you can use 
	 * the converted string as part of the query. Never escape the whole 
	 * query because then even the neccessary paranthesis characters 
	 * will be escaped.
	 * @param str the string to escape 
	 * @return the escaped string
	 */
	static QString escapeFilter(const QString &str);

	/**
	 * Returns true if service location is generally possible.
	 * It will fail if SLP libraries are not installed.
	 * @return true if service location seems to be possible
	 */
	bool available();

	/**
	 * Use this constant for findServiceTypes()'s namingAuthority argument
	 * to get only services from the default (IANA) naming authority.
	 */
	static const QString DEFAULT_AUTHORITY;

	/**
	 * Use this constant for findServiceTypes()'s namingAuthority argument
	 * to get all services,
	 */
	static const QString ALL_AUTHORITIES;

	/**
	 * Finds all service types in the given scope with the given naming 
	 * authority. This function emits the signal foundServiceTypes()
	 * each time it discovered one or more service types. When the last 
	 * service type has been found lastServiceTypeSignal() will be emitted. 
	 * When KServiceLocator is in synchronous mode the function will not be
	 * returned before lastServiceTypeSignal() has been emitted, in 
	 * asynchronous mode lastServiceTypeSignal() can be emitted later. If 
	 * you call this function while another asynchronous operation is 
	 * running it will fail.
	 * 
	 * @param namingAuthority the naming authorities of the service 
	 *                        types to be found. If DEFAULT_AUTHORITY
	 *                        only IANA service types will be returned,
	 *                        if it is ALL_AUTHORITIES or the
	 *                        argument has been omitted all service types
	 *                        will be returned.
	 * @param scopelist a comma-separated list of all scopes that will 
	 *                  be searched, or QString:null to search in all 
	 *                  scopes
	 * @return true if the operation was successful
	 */
	bool findServiceTypes(const QString &namingAuthority = QString::null,
			      const QString &scopelist = QString::null);

	/**
	 * Finds all services in the given scope with the given service type.
	 * Examples for service types are "service:ftp" to find all ftp servers
	 * or "service:remotedesktop:" to find all remote desktop services.
	 * You can also specify a filter to match services depending their 
	 * attributes. The filter uses the LDAP Search Filter syntax as 
	 * described in RFC 2254, "String Representation of LDAP Search 
	 * Filters".
	 * The function emits the signal foundService() each time it 
	 * discovered a service types. When the last service has been found 
	 * lastServiceSignal() will be emitted. When KServiceLocator is in 
	 * synchronous mode the function will not be returned before 
	 * lastServiceSignal() has been emitted, in asynchronous mode 
	 * lastServiceSignal() can be emitted later. If you call this function 
	 * while another asynchronous operation is running it will fail.
	 * 
	 * @param srvtype the type of the service to search.
	 * @param filter a filter in LDAP Search Filter syntax, as described
	 *               in RFC 2254.
	 * @param scopelist a comma-separated list of all scopes that will 
	 *                  be searched, or QString:null to search in all 
	 *                  scopes
	 * @return true if the operation was successful
	 */
	bool findServices(const QString &srvtype, 
			  const QString &filter = QString::null, 
			  const QString &scopelist = QString::null);

	/**
	 * Finds the attributes of the service with the given URL.
	 * The function emits the signal foundAttributes() if the service
	 * has been found, followed by lastAttributesSignal(). When 
	 * KServiceLocator is  in synchronous mode the function will not be 
	 * returned before lastAttributesSignal() has been emitted, in 
	 * asynchronous mode lastAttributesSignal() can be emitted later. If 
	 * you call this function while another asynchronous operation is 
	 * running it will fail.
	 * 
	 * @param serviceURL the URL of the service to search
	 * @param attributeIds a comma-separated list of attributes to 
	 *                     retrieve, or QString::null to retrieve all
	 *                     attributes
	 * @return true if the operation was successful
	 */
	bool findAttributes(const QString &serviceUrl,
			    const QString &attributeIds = QString::null);

	/**
	 * Finds all scopes that can be searched. Always finds at least
	 * one scope (the default scope).
	 * The function emits the signal foundScopes() if the service
	 * has been found. When KServiceLocator is in synchronous mode 
	 * the function will not be returned before foundScopes() has been 
	 * emitted, in asynchronous mode it can be emitted later. If 
	 * you call this function while another asynchronous operation is 
	 * running it will fail.
	 *
	 * @return true if the operation was successful
	 */
	bool findScopes();

	/**
	 * If there is a asynchronous operation running, it will aborted.
	 * It is not guaranteed that you dont receive any signals after 
	 * calling abortOperation. You will always get a lastSignal call
	 * even after aborting.
	 */
	void abortOperation();


 signals:
	/**
	 * Called by findServiceTypes() each time one or more service
	 * types have been discovered.
	 * @param serviceTypes a comma-separated list of service types
	 */
	void foundServiceTypes(QString serviceTypes);

	/**
	 * Called by findServices() each time a service has been 
	 * found.
	 * @param serviceUrl the service url
	 * @param lifetime the lifetime of the service in seconds
	 */
	void foundService(QString serviceUrl, 
			  int lifetime);

	/**
	 * Called by findAttributes() when the service's attributes
	 * have been found.
	 * @param serviceUrl the service url
	 * @param attributes an attribute map (see parseAttributeList() and
	 *                   decodeAttributeValue())
	 */
	void foundAttributes(QString serviceUrl, 
			     QString attributes);

	/**
	 * Called by findScopes() when the scopes have been discovered.
	 * @param scopeList a list of valid scopes, empty if an error 
	 *                  occurred
	 */
	void foundScopes(QStringList scopeList);

	
	/**
	 * Emitted when a service type discovery operation finished. If 
	 * you are in async  mode it is guaranteed that the operation is 
	 * finished when you  receive the signal, so you can start a new 
	 * one. In synced mode the operation is still running, so you 
	 * must wait until the find function returns.
	 * @param success true if all items have been found, false when 
	 *                an error occurred during the operation
	 */
	void lastServiceTypeSignal(bool success);

	/**
	 * Emitted when a service discovery operation finished. If you are 
	 * in async  mode it is guaranteed that the operation is finished 
	 * when you receive the signal, so you can start a new one. In 
	 * synced mode the operation is still running, so you must wait 
	 * until the find function returns.
	 * @param success true if all items have been found, false when 
	 *                an error occurred during the operation
	 */
	void lastServiceSignal(bool success);

	/**
	 * Emitted when a attributes reqyest operation finished. If you are 
	 * in async mode it is guaranteed that the operation is finished 
	 * when you receive the signal, so you can start a new one. In synced 
	 * mode the operation is still running, so you must wait until the
	 * find function returns.
	 * @param success true if all items have been found, false when 
	 *                an error occurred during the operation
	 */
	void lastAttributesSignal(bool success);

 protected:
	virtual void customEvent(QCustomEvent *);

 private:
	void emitFoundServiceTypes(QString serviceTypes);
	void emitFoundService(QString serviceUrl,
			      int lifetime);
	void emitFoundAttributes(QString serviceUrl,
				 QString attributes);
	void emitFoundScopes(QStringList scopeList);
	void emitLastServiceTypeSignal(bool success);
	void emitLastServiceSignal(bool success);
	void emitLastAttributesSignal(bool success);
};

#endif
