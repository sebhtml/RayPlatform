/*
 	RayPlatform
    Copyright (C) 2012  Sébastien Boisvert

	http://github.com/sebhtml/RayPlatform

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU Lesser General Public License as published by
    the Free Software Foundation, version 3 of the License.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU Lesser General Public License for more details.

    You have received a copy of the GNU Lesser General Public License
    along with this program (lgpl-3.0.txt).  
	see <http://www.gnu.org/licenses/>

*/

#include "MessageQueue.h"

#include <iostream>
using namespace std;

#ifdef ASSERT
#include <assert.h>
#endif /* ASSERT */

#ifdef CONFIG_COMPARE_AND_SWAP

#if defined(__INTEL_COMPILER) || defined(__GNUC__)
/* The compiler provides what we need */

#elif defined( __APPLE__)
#include <libkern/OSAtomic.h>

#elif defined(_WIN32)
#include <windows.h>

#endif /* _WIN32 */

#endif /* CONFIG_COMPARE_AND_SWAP */

void MessageQueue::constructor(uint32_t bins){

	#ifdef ASSERT
	assert(bins);
	assert(bins>0);
	#endif /* ASSERT */

/*
 * For the non-blocking lockless algorithm, we need an extra slot.
 */
	m_size=bins+1;
	m_headForPopOperations=0;
	m_tailForPushOperations=0;

	m_ring=(Message*)__Malloc(m_size*sizeof(Message),"/MessageQueue",false);

	m_dead=false;

	#ifdef ASSERT
	assert(m_ring!=NULL);
	assert(m_headForPopOperations<m_size);
	assert(m_tailForPushOperations<m_size);
	#endif /* ASSERT */

#ifdef CONFIG_USE_LOCKING
#ifdef CONFIG_USE_SPINLOCK
	pthread_spin_init(&m_lock,PTHREAD_PROCESS_PRIVATE);
#endif /* CONFIG_USE_SPINLOCK */

#ifdef CONFIG_USE_MUTEX
	pthread_mutex_init(&m_lock,NULL);
#endif /* CONFIG_USE_MUTEX */
#endif /* CONFIG_USE_LOCKING */

}

/*
 * push() is thread-safe if only one thread calls it.
 * The calling thread can be different from the other thread
 * that is calling pop().
 */
bool MessageQueue::push(Message*message){

	uint32_t nextTail=increment(m_tailForPushOperations); // not atomic

/*
 * The queue is full.
 * Maybe there is a pop() call in progress, but the new head
 * has not been published yet so we must wait.
 */
	if(nextTail==m_headForPopOperations)
		return false;

	m_ring[m_tailForPushOperations]=*message; // not atomic

/*
 * Publish the change to other threads.
 * This must be atomic.
 * Instructions must be executed in-order otherwise,
 * m_tailForPushOperations will publish a draft of the message.
 */
	m_tailForPushOperations=nextTail; // likely atomic if sizeof(SystemBus) >= 32 bits

	return true;
}

/**
 * \see http://www.codeproject.com/Articles/43510/Lock-Free-Single-Producer-Single-Consumer-Circular
 */
bool MessageQueue::isFull(){
	return increment(m_tailForPushOperations) == m_headForPopOperations;
}

/*
 * Pop is thread-safe if only one thread calls it.
 * It can be a thread different from the one that calls
 * push().
 */
bool MessageQueue::pop(Message*message){

/*
 * We can not pop from am empty list.
 */
	if(m_headForPopOperations==m_tailForPushOperations)
		return false;

	*message=m_ring[m_headForPopOperations]; // not atomic

	uint32_t nextHead=increment(m_headForPopOperations); // not atomic
	
/*
 * Publish the change.
 * Instructions must be executed in-order otherwise,
 * m_tailForPushOperations will publish a draft of the message.
 */
	m_headForPopOperations=nextHead; // likely atomic if sizeof(SystemBus) >= 32 bits

	return true;
}

bool MessageQueue::hasContent(){
	return m_headForPopOperations!=m_tailForPushOperations;
}

void MessageQueue::destructor(){
	if(m_size==0)
		return;

	#ifdef ASSERT
	assert(m_size>0);
	assert(m_ring!=NULL);
	#endif /* ASSERT */

	m_size=0;
	m_headForPopOperations=0;
	m_tailForPushOperations=0;
	__Free(m_ring,"/MessageQueue",false);
	m_ring=NULL;

	#ifdef ASSERT
	assert(m_size==0);
	assert(m_ring==NULL);
	#endif /* ASSERT */

#ifdef CONFIG_USE_LOCKING

#ifdef CONFIG_USE_SPINLOCK
	pthread_spin_destroy(&m_lock);
#endif /* CONFIG_USE_SPINLOCK */

#ifdef CONFIG_USE_MUTEX 
	pthread_mutex_destroy(&m_lock);
#endif /* CONFIG_USE_MUTEX */

#endif /* CONFIG_USE_LOCKING */
}

bool MessageQueue::isDead(){
	return m_dead;
}

#ifdef CONFIG_USE_LOCKING

/* 
 * The following 3 locking methods
 * are not compiled if locking is not enabled.
 */

void MessageQueue::lock(){
#ifdef CONFIG_USE_SPINLOCK
	pthread_spin_lock(&m_lock);
#elif defined(CONFIG_USE_MUTEX)
	pthread_mutex_lock(&m_lock);
#endif /* CONFIG_USE_MUTEX */
}

bool MessageQueue::tryLock(){
#ifdef CONFIG_USE_SPINLOCK
	return pthread_spin_trylock(&m_spinlock)==0;
#elif defined(CONFIG_USE_MUTEX)
	pthread_mutex_trylock(&m_lock);
#endif /* CONFIG_USE_MUTEX */
}

void MessageQueue::unlock(){
#ifdef CONFIG_USE_SPINLOCK
	pthread_spin_unlock(&m_spinlock);
#elif defined(CONFIG_USE_MUTEX)
	pthread_mutex_unlock(&m_lock);
#endif /* CONFIG_USE_MUTEX */
}

#endif /* CONFIG_USE_LOCKING */

void MessageQueue::sendKillSignal(){
	m_dead=true;
}

uint32_t MessageQueue::increment(uint32_t index){

	#ifdef ASSERT
	assert(index<m_size);
	assert(m_size!=0);
	assert(index>=0);
	#endif /* ASSERT */

	return (index+1)%m_size;
}

/*
 * The code below is not used at the moment, but could be an
 * alternative to CONFIG_USE_LOCKING for some architectures.
 */
#ifdef CONFIG_COMPARE_AND_SWAP
bool CompareAndSwap(uint32_t*memory,uint32_t oldValue,uint32_t newValue){
	#ifdef __INTEL_COMPILER
/*
 * Intel provides a compatibility layer with gcc it seems.
 * \see Intel® C++ Intrinsic Reference, p.165 (Document Number: 312482-003US)
 * \see http://software.intel.com/sites/default/files/m/9/4/c/8/e/18072-347603.pdf
 */
	return __sync_bool_compare_and_swap(memory,oldValue,newValue);

	#elif defined(__GNUC__)
/*
 * gcc provides nice builtins.
 * http://gcc.gnu.org/onlinedocs/gcc-4.1.1/gcc/Atomic-Builtins.html
 */
	return __sync_bool_compare_and_swap(memory,oldValue,newValue);

	#elif defined(_WIN32)
/*
 * This code is not tested. From the documentation, the ordering is different.
 * The documentation is from http://msdn.microsoft.com/en-us/library/windows/desktop/ms683560%28v=vs.85%29.aspx
 * There is also an intrisic called _InterlockedCompareExchange.
 */
	InterlockedCompareExchange(memory,newValue,oldValue);
/*
 * This Microsoft API is retarded, InterlockedCompareExchange returns the value of *memory 
 * before the call, which is ridiculous because we need to know if the compare-and-swap
 * was successful regardless.
 */
	return *memory==newValue;

	#elif defined(BLUE_GENE_Q)

/*
 * For the Blue Gene /Q, the name is the same as in gcc.
 * \see IBM XL C/C++ for Blue Gene/Q, V12.1, p. 484
 * \see https://support.scinet.utoronto.ca/wiki/images/d/d5/Bgqccompiler.pdf
 */
	return __sync_bool_compare_and_swap(memory,oldValue,newValue);

	#else
/*
 * Provide a dummy implementation that will fail sometimes I guess.
 */
	if(*memory!=oldValue)
		return false;
	*memory=newValue
	return true;
	#endif
}

#endif /* CONFIG_COMPARE_AND_SWAP */

