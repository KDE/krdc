/***************************************************************************
    begin                : Wed Jan 1 17:56 CET 2003
    copyright            : (C) 2003 by Tim Jansen
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

#ifndef KDE_UTIL_SMARTPTR_H
#define KDE_UTIL_SMARTPTR_H

#include <qstring.h>

class WeakPtr;

/**
 * @internal
 */
struct SmartPtrRefCount {
	SmartPtrRefCount(int toObj, int toThis) :
		refsToObject(toObj),
	        refsToThis(toThis) {
	}
	int refsToObject; // number of pointers to the object, 0 if released
	int refsToThis;   // number of pointer to the ref count
};

class WeakPtr;

/**
 * SmartPtr is a reference counting smart pointer. When you create
 * the first instance it will create a new counter for the pointee
 * and it share it with all other SmartPtr instances for that pointee.
 * The reference count can only be kept accurate when you do not create
 * a second 'realm' of references by converting a SmartPtr into a
 * regular pointer and then create a new SmartPtr from that pointer.
 * When the last instance of a SmartPtr for the given object has been
 * deleted the object itself will be deleted. You can stop the SmartPtr
 * system to manage an object by calling @ref release() on any of
 * the pointers pointing to that object. All SmartPtrs will then stop
 * managing the object, and you can also safely create a second 'realm'.
 *
 * SmartPtr can be combined with @ref WeakPtr. A WeakPtr
 * does not influence its life cycle, but notices when a SmartPtr
 * deletes the object.
 *
 * The recommended way to use SmartPtr and @ref WeakPtr is to use SmartPtr
 * for all aggregations and WeakPtr for associations. Unlike auto_ptr, 
 * SmartPtr can be used in collections.
 *
 * SmartPtr is not thread-safe. All instances of SmartPtrs pointing
 * to a pointee must always be in the same thread, unless you break
 * the 'realm' by calling @ref release() in one thread and give the
 * original pointer the other thread. It can then create a new SmartPtr
 * and control the lifecycle of the object.
 * @see WeakPtr
 */
template <class T> 
class SmartPtr
{
 public: // members are public because of problems with gcc 3.2
	friend class WeakPtr;

  /// @internal
	T* ptr;
  /// @internal
	mutable SmartPtrRefCount *rc; // if !rc, refcount=1 is assumed

protected:
	void freePtr() {
		if (!ptr)
			return;
		if (!rc)
			delete ptr;
		else {
			if (rc->refsToObject > 0) {
				Q_ASSERT(rc->refsToObject >= rc->refsToThis);
				if (rc->refsToObject == 1) {
					delete ptr;
					rc->refsToObject = -1;
				}
				else
					rc->refsToObject--;
			}
			rc->refsToThis--;
			if (rc->refsToThis < 1)
				delete rc;
		}
	}
	
	void init(T *sptr, SmartPtrRefCount *&orc)
	{
		ptr = sptr;
		if (!sptr)
			rc = 0;
		else if (!orc) {
			orc = new SmartPtrRefCount(2, 2);
			rc = orc;
		}
		else {
			rc = orc;
			rc->refsToThis++;
			if (rc->refsToObject) {
				// prevent initialization from invalid WeakPtr
				Q_ASSERT(rc->refsToObject > 0);
				rc->refsToObject++;
			}
		}
	}
	
	SmartPtr(T *p, SmartPtrRefCount *&orc)
	{
		init(p, orc);
	}

public:
	/**
	 * Creates a SmartPtr that refers to the given pointer @p.
	 * SmartPtr will take control over the object and delete it
	 * when the last SmartPtr that referes to the object
	 * has been deleted.
	 * @param p the pointer to the object to manage, or the null pointer
	 */
	SmartPtr(T* p = 0) :
		ptr(p),
		rc(0)
	{
	}
	
	/**
	 * Copies the given SmartPtr, sharing ownership with the other
	 * pointer. Increases the reference count by 1 (if the object
	 * has not been @ref release()d).
	 * @param sptr the object pointer to copy
	 */
	SmartPtr(const SmartPtr<T> &sptr) 
	{
		init(sptr.ptr, sptr.rc);
	}

	/**
	 * Copies the given SmartPtr, sharing ownership with the other
	 * pointer. Increases the reference count by 1 (if the object
	 * has not been @ref release()d).
	 * @param sptr the object pointer to copy
	 */
	template<class T2>
	SmartPtr(const SmartPtr<T2> &sptr) 
	{
		init((T*)sptr.ptr, sptr.rc);
	}

	/**
	 * Delete the pointer and, if the reference count is one and the object has not
	 * been released, deletes the object. 
	 */
	~SmartPtr() {
		freePtr();
	}

	/**
	 * Copies the given SmartPtr, sharing ownership with the other
	 * pointer. Increases the reference count by 1 (if the object
	 * has not been @ref release()d). The original object will be dereferenced
	 * and thus deleted, if the reference count is 1.
	 * @param sptr the object pointer to copy
	 * @return this SmartPtr object
	 */
	SmartPtr &operator=(const SmartPtr<T> &sptr) {
		if (this == &sptr)
			return *this;

		freePtr();
		init(sptr.ptr, sptr.rc);
		return *this;
	}

	/**
	 * Copies the given SmartPtr, sharing ownership with the other
	 * pointer. Increases the reference count by 1 (if the object
	 * has not been @ref release()d). The original object will be dereferenced
	 * and thus deleted, if the reference count is 1.
	 * @param sptr the object pointer to copy
	 * @return this SmartPtr object
	 */
	template<class T2>
	SmartPtr &operator=(const SmartPtr<T2> &sptr) {
		if (this == static_cast<SmartPtr<T> >(&sptr))
			return *this;

		freePtr();
		init(static_cast<T>(sptr.ptr), sptr.rc);
		return *this;
	}

	/**
	 * Sets the SmartPointer to the given value. The original object
	 * will be dereferenced and thus deleted, if the reference count is 1.
	 * @param p the value of the new pointer
	 */
	void set(T *p) {
		if (ptr == p)
			return;
		freePtr();
		
		ptr = p;
		rc = 0;
	}

	/**
	 * Releases the ptr. This means it will not be memory-managed 
	 * anymore, neither by this SmartPtr nor by any other pointer that
	 * shares the object. The caller is responsible for freeing the 
	 * object. It is possible to assign the plain pointer (but not the 
	 * SmartPtr!) to another SmartPtr that will then start memory
	 * management. This may be useful, for example, to let another
	 * thread manage the lifecyle.
	 * @return the pointer, must be freed by the user
	 * @see data()
	 */
	T* release() {
		if (!rc)
			rc = new SmartPtrRefCount(0, 1);
		else
			rc->refsToObject = 0;
		return ptr;
	}

	/**
	 * Sets the SmartPointer to the given value. The original object
 	 * will be dereferenced and thus deleted, if the reference count is 1.
	 * @param p the value of the new pointer
	 * @return this SmartPtr object
	 */
	SmartPtr &operator=(T *p) {
		set(p);
		return *this;
	}

        /**
         * Returns true if the SmartPtr points to an actual object, false
         * if it is the null pointer.
         * @return true for an actual pointer, false for the null pointer
         */
        operator bool() const {
                return ptr != 0;
        }

        /**
         * Returns the plain pointer to the pointed object. The object will
         * still be managed by the SmartPtr. You must ensure that the pointer
         * is valid (so don't delete the SmartPtr before you are done with the
         * plain pointer).
         * @return the plain pointer
         * @see data()
         * @see release()
         * @see WeakPtr
         */
	template<class T2>
	operator T2*() const { 
		return static_cast<T2*>(ptr);
	}

        /**
         * Returns the plain pointer to the pointed object. The object will
         * still be managed by the SmartPtr. You must ensure that the pointer
         * is valid (so don't delete the SmartPtr before you are done with the
         * plain pointer).
         * @return the plain pointer
         * @see data()
         * @see release()
         * @see WeakPtr
         */
	template<class T2>
	operator const T2*() const { 
		return static_cast<const T2*>(ptr);
	}

        /**
         * Returns a reference to the pointed object. This works exactly
         * like on a regular pointer.
         * @return the pointer object
         */
	T& operator*() {
		return *ptr;
	}

        /**
         * Returns a reference to the pointed object. This works exactly
         * like on a regular pointer.
         * @return the pointer object
         */
	const T& operator*() const {
		return *ptr;
	}

        /**
         * Access a member of the pointed object. This works exactly
         * like on a regular pointer.
         * @return the pointer
         */
	T* operator->() {
		return ptr;
	}

        /**
         * Access a member of the pointed object. This works exactly
         * like on a regular pointer.
         * @return the pointer
         */
	const T* operator->() const {
		return ptr;
	}

        /**
         * Compares two SmartPtrs. They are equal if both point to the
         * same object.
         * @return true if both point to the same object
         */
	bool operator==(const SmartPtr<T>& sptr) const { 
		return ptr == sptr.ptr; 
	}

        /**
         * Compares two SmartPtrs. They are unequal if both point to
         * different objects.
         * @return true if both point to different objects
         */
	bool operator!=(const SmartPtr<T>& sptr) const {
		return ptr != sptr.ptr;
	}
 
        /**
         * Compares a SmartPtr with a plain pointer. They are equal if
         * both point to the same object.
         * @return true if both point to the same object
         */
	bool operator==(const T* p) const {
		return ptr == p;
	}

        /**
         * Compares a SmartPtr with a plain pointer. They are unequal if
         * both point to different objects.
         * @return true if both point to different objects
         */
	bool operator!=(const T* p) const {
		return ptr == p;
	}

        /** 
         * Negates the pointer. True if the pointer is the null pointer
         * @return true for the null pointer, false otherwise
         */
	bool operator!() const {
		return ptr == 0;
	}

        /**
         * Returns the pointer. The object will still be managed
         * by the SmartPtr. You must ensure that the pointer
         * is valid (so don't delete the SmartPtr before you are done with the
         * plain pointer).
         * @return the plain pointer
         * @see release()
         * @see WeakPtr
         */ 
	T* data() { 
		return ptr; 
	}
	
        /**
         * Returns the pointer. The object will still be managed
         * by the SmartPtr. You must ensure that the pointer
         * is valid (so don't delete the SmartPtr before you are done with the
         * plain pointer).
         * @return the plain pointer
         * @see release()
         * @see WeakPtr
         */ 
	const T* data() const { 
		return ptr; 
	}

	/**
	 * Checks whether both SmartPtrs use the same pointer but two
	 * different reference counts.
	 * If yes, one of them must be 0 (object released), otherwise 
	 * it is an error.
	 * @return true if the pointers are used correctly
	 */
	bool isRCCorrect(const SmartPtr<T> &p2) const {
		if (ptr == p2.ptr)
			return true;
		if (rc == p2.rc)
			return true;
		return (rc->refsToObject == 0) || (p2.rc->refsToObject == 0);
	}

	/**
	 * Returns the reference count of the object. The count is 0 if
	 * the object has been released (@ref release()). For the null pointer
	 * the reference count is always 1.
	 * @return the reference count, or 0 for released objects
	 */
	int referenceCount() const {
		return rc ? rc->refsToObject : 1;
	}

	/**
	 * Returns a string representation of the pointer.
	 * @return a string representation
	 */
	QString toString() const {
		int objrcount = 1;
		int rcrcount = 0;

		if (rc) {
			objrcount = rc->refsToObject;
			rcrcount = rc->refsToThis;
		}
		return QString("SmartPtr: ptr=%1, refcounts=%2, ptrnum=%3")
			.arg((int)ptr).arg(objrcount).arg(rcrcount);
	}

};

#endif
