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

#ifndef _MessageQueue_h
#define _MessageQueue_h

#include <pthread.h>
#include <communication/Message.h>
#include <memory/allocator.h> /* for __Malloc and __Free */

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
 *   tail                  head
 *   pop here         push here
 *    *                    *
 *   --->                 --->
 * -------------------------------------------------------------------->
 *
 *
 * \author Élénie Godzaridis (design)
 * \author Sébastien Boisvert (programming & design)
 */
class MessageQueue{

	pthread_spinlock_t m_spinlock;

	Message*m_ring;
	int m_size;
	int m_headForPushOperations;
	int m_tailForPopOperations;
	int m_numberOfMessages;

	bool m_dead;
public:

	void constructor(int bins);
	void push(Message*message);
	void pop(Message*message);
	void destructor();
	bool hasContent();
	void lock();
	void unlock();

	void sendKillSignal();
	bool isDead();
	bool isFull();
	
};

#endif /* _MessageQueue_h */

