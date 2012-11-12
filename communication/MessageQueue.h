/*
 	RayPlatform
    Copyright (C) 2012 Sébastien Boisvert

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

#ifndef _MessageQueue_h
#define _MessageQueue_h

#include "Message.h"

#include <RayPlatform/memory/allocator.h> /* for __Malloc and __Free */

/*
 * For uint32_t
 * Heck, even Microsoft provides this, at least in MSVC 2010.
 *
 * \see http://stackoverflow.com/questions/126279/c99-stdint-h-header-and-ms-visual-studio
 */
#include <stdint.h>

//#define CONFIG_USE_LOCKING
//#define CONFIG_USE_MUTEX
//#define CONFIG_USE_SPINLOCK

#ifdef CONFIG_USE_LOCKING
#if defined(CONFIG_USE_MUTEX) || defined(CONFIG_USE_SPINLOCK)
#include <pthread.h>
#endif
#endif /* CONFIG_USE_LOCKING */

/**
 * \brief This is a message queue as its name implies.
 *
 * This class is used by the MPI ranks in the mini-ranks model.
 * The buffers are allocated by the RingAllocator of the corresponding
 * mini-rank.
 *
 * *************************************************************
 * Here is a user story (where the user is a processor !):
 *
 * Number of MPI ranks: 8 (one thread each)
 * Number of mini-ranks per MPI rank: 23 (one thread each)
 * Total number of mini-ranks: 8*23 = 184
 *
 * - Mini-rank 4 ([0,4]) pushes something for mini-rank 101 ([4,9]) in its StaticVector outbox  [NO SPINLOCK]  /in the application
 * - the ComputeCore object of mini-rank 4 pushes the something in its MessageQueue outbox	[SPINLOCK] #1	in RayPlatform land (rare)
 * - the MessagesHandler of RankProcess 0 pushes it with MPI_Isend (src: [0,4], dest: [4,9])	[SPINLOCK] #2	in RayPlatform land (frequent)
 * - the MessagesHandler of RankProcess 4 pops it with MPI_Iprobe/MPI_Recv			[SPINLOCK] #3	in RayPlatform land (rare)
 * - RankProcess 4 pushes the messages in the MessageQueue inbox of [4,9]	(continued)
 * - Mini-rank 101 pops from its MessageQueue inbox and pushes it into its StaticVector inbox	[SPINLOCK] #4	in RayPlatform land (frequent)
 * - Mini-rank 101 consumes its inbox message 							[NO SPINLOCK]  /in the application
 *
 * *************************************************************
 *
 * The most important thing is that there are no global lock.
 *
 * Locks:
 *
 * - a MPI rank needs to lock the MessageQueue outbox of every mini-rank to check for messages.
 * - a MPI rank does not lock any MessageQueue inbox unless MPI_Iprobe was productive.
 * - a mini-rank does not lock its MessageQueue outbox unless it has something in its StaticVector outbox
 * - a mini-rank needs to lock its MessageQueue inbox to check for messages.
 *
 * *************************************************************
 * So this is really a nice design (by Élénie Godzaridids) because 
 *
 * 1. a MPI rank locks/unlocks endlessly MessageQueue outbox objects, but mini-ranks do 
 * not care unless they need to push a message.
 * 2. a mini-rank locks/unlocks endlessly its MessageQueue inbox object, but the MPI rank
 * does not care because it will only lock/unlock it when MPI_Iprobe is productive, which
 * does not happen so often.
 *
 *
 * => So nobody fight over the same thing really.
 *
 * *************************************************************
 *
 * The last thing to think about is who is responsible for managing the dirty/available slots
 * for the RingAllocator objects. Clearly, it is the RankProcess object because MiniRank are
 * not allowed to call MPI.
 *
 *   head		tail
 *   pop here         push here
 *    *                    *
 *   --->                 --->
 * -------------------------------------------------------------------->
 *
 *
 * \author Élénie Godzaridis (design)
 * \author Sébastien Boisvert (programming & design)
 *
 * \see http://www.codeproject.com/Articles/43510/Lock-Free-Single-Producer-Single-Consumer-Circular
 *
 * There is also a Ph.D. thesis by Yi Zhang in 2003 about non-blocking algorithms:
 *
 * \see http://www.cse.chalmers.se/~tsigas/papers/Yi-Thesis.pdf
 */
class MessageQueue{

#ifdef CONFIG_USE_LOCKING

#if defined(CONFIG_USE_MUTEX)
	pthread_mutex_t m_lock;
#elif defined(CONFIG_USE_SPINLOCK)
	pthread_spinlock_t m_lock;
#endif /* CONFIG_USE_SPINLOCK */

#endif /* CONFIG_USE_LOCKING */

	Message*m_ring;
	uint32_t m_size;

/*
 * The volatile keyword means:
 *
 * " Generally speaking, the volatile keyword is intended to prevent the compiler from applying any 
 * optimizations on the code that assume values of variables cannot change "on their own."
 *
 * This is necessary to tell the compiler to avoid any reordering of instructions for this
 * variable.
 *
 * \see http://en.wikipedia.org/wiki/Volatile_variable
 */
	volatile uint32_t m_headForPopOperations;
	volatile uint32_t m_tailForPushOperations;

	bool m_dead;

	uint32_t increment(uint32_t index);

public:

	void constructor(uint32_t bins);

	bool push(Message*message);
	bool pop(Message*message);

	void destructor();
	bool hasContent();

#ifdef CONFIG_USE_LOCKING
	void lock();
	void tryLock();
	void unlock();
#endif /* CONFIG_USE_LOCKING */

	void sendKillSignal();
	bool isDead();
	bool isFull();
};

#endif /* _MessageQueue_h */

