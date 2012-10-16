/*
 	Ray
    Copyright (C) 2010, 2011  Sébastien Boisvert

	http://DeNovoAssembler.SourceForge.Net/

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

#ifndef _MessagesHandler
#define _MessagesHandler

/* Many communication models are implemented:
 * latencies are for a system with 36 cores (or 120 cores), QLogic interconnect, 
 *  and Performance scaled messaging
 * - CONFIG_COMM_IPROBE_ANY_SOURCE  
     	latency(36): 16 micro seconds
	latency(120): 19 microseconds
	no fairness, possible starvation
 * - CONFIG_COMM_IPROBE_ROUND_ROBIN 
     	latency(36):, 18 microseconds
     	latency(120): 23 microseconds
     	latency with 1008 cores: 78 (84*12): microseconds (without -route-messages.)
     	with fairness, no starvation
 * - CONFIG_COMM_PERSISTENT (latency(36): , no fairness)
     	latency(36): 28 micro seconds
	latency(120): 75 microseconds
	no fairness, possible starvation
 * - CONFIG_COMM_IRECV_TESTANY (no data)
 */

/*
 * only one must be set
 */

/*
 * Persistent communication pumps the message more rapidly
 * on some systems
 */
//#define CONFIG_COMM_PERSISTENT

/**
 *  Use round-robin reception.
 */
//#define CONFIG_COMM_IPROBE_ROUND_ROBIN

/* this configuration may be important for low latency */
/* uncomment if you want to try it */
/* this is only useful with CONFIG_COMM_IPROBE_ROUND_ROBIN */
//#define CONFIG_ONE_IPROBE_PER_TICK

/*
 * Probing any message, then receive it.
 * This model is used by Converse/CHARM++/NAMD.
 */
#define CONFIG_COMM_IPROBE_ANY_SOURCE

/*
 * Non-blocking reception. This was suggested by Pavan Balaji
 * and Jeff Squyres.
 * It does not work that well on the IBM Blue Gene /Q.
 */
//#define CONFIG_COMM_IRECV_TESTANY

#include <mpi.h> // this is the only reference to MPI

#include <memory/MyAllocator.h>
#include <communication/Message.h>
//#include <core/common_functions.h>
#include <memory/RingAllocator.h>
#include <structures/StaticVector.h>
#include <plugins/CorePlugin.h>

class ComputeCore;

#include <string>
#include <vector>
using namespace std;

/**
 * A data model for storing dirty buffers
 */
class DirtyBuffer{
public:
	void*m_buffer;
	MPI_Request m_messageRequest;
	Rank m_destination;
	MessageTag m_messageTag;
};

/**
 * Software layer to handle messages.
 * MessagesHandler is the only part of Ray that is aware of the 
 * message-passing interface.
 * All the other parts rely only on a simple inbox and a simple outbox.
 * These boxes of messages could be implemented with something else 
 * than message-passing interface.
 *
 * Messages are sent with MPI_Isend and buffers are managed by a dirty buffer
 * laundry list.
 *
 * 4 communication models are implemented:
 *
 * CONFIG_COMM_IPROBE_ANY_SOURCE -> MPI_Iprobe with any source + MPI_Recv
 * CONFIG_COMM_IRECV_TESTANY -> MPI_Irecv + MPI_Testany
 * CONFIG_COMM_IPROBE_ROUND_ROBIN -> MPI_Iprobe with round-robin source + MPI_Recv
 * CONFIG_COMM_PERSISTENT -> MPI_Recv_init + MPI_Test 
 *
 * \author Sébastien Boisvert
 */
class MessagesHandler: public CorePlugin{

	int m_minimumNumberOfDirtyBuffersForSweep;

	int m_minimumNumberOfDirtyBuffersForWarning;

/** prints dirty buffers **/
	void printDirtyBuffers();

	// the number of peers for communication
	int m_peers;

	DirtyBuffer*m_dirtyBuffers;

	int m_numberOfDirtyBuffers;
	int m_maximumDirtyBuffers;
	int m_dirtyBufferSlots;

	MessageTag RAY_MPI_TAG_DUMMY;

	bool m_destroyed;

	vector<int> m_connections;

	Rank m_rank;
	int m_size;
	/** this variable stores counts for sent messages */
	uint64_t*m_messageStatistics;

	/** messages sent */
	uint64_t m_sentMessages;
	/** messages received */
	uint64_t m_receivedMessages;

/**
 * 	In Ray, all messages have buffer of the same type
 */
	MPI_Datatype m_datatype;

	string m_processorName;

#ifdef CONFIG_COMM_IPROBE_ROUND_ROBIN
	/** round-robin head */
	int m_currentRankIndexToTryToReceiveFrom;
#endif /* CONFIG_COMM_IPROBE_ROUND_ROBIN */

#ifdef CONFIG_COMM_PERSISTENT

	/** the number of buffered messages in the persistent layer */
	int m_bufferedMessages;

	/** linked lists */
	MessageNode**m_heads;

	/** linked lists */
	MessageNode**m_tails;

	/** the number of persistent requests in the ring */
	int m_ringSize;

	/** the current persistent request in the ring */
	int m_head;

	/** the ring of persistent requests */
	MPI_Request*m_ring;

	/** the ring of buffers for persistent requests */
	uint8_t*m_buffers;

#endif /* CONFIG_COMM_PERSISTENT */


#ifdef CONFIG_COMM_IRECV_TESTANY

	int m_numberOfNonBlockingReceives;
	MPI_Request*m_requests;
	uint8_t*m_receptionBuffers;
	int m_bufferSize;

#endif /* CONFIG_COMM_IRECV_TESTANY */

	/** initialize persistent communication parameters */
	void initialiseMembers();

	/** probe and read a message -- this method is not utilised */
	void probeAndRead(int source,int tag,StaticVector*inbox,RingAllocator*inboxAllocator);

#ifdef CONFIG_COMM_PERSISTENT
	/** pump a message from the persistent ring */
	void pumpMessageFromPersistentRing(StaticVector*inbox,RingAllocator*inboxAllocator);
#endif /* CONFIG_COMM_PERSISTENT */

#ifdef CONFIG_COMM_IPROBE_ROUND_ROBIN
	/** select and fetch a message from the internal messages using a round-robin policy */
	void roundRobinReception(StaticVector*inbox,RingAllocator*inboxAllocator);
#endif /* CONFIG_COMM_IPROBE_ROUND_ROBIN */

#ifdef CONFIG_COMM_IRECV_TESTANY

	void receiveMessages_irecv_testany(StaticVector*inbox,RingAllocator*inboxAllocator);
	void init_irecv_testany(RingAllocator*inboxAllocator);
	void destroy_irecv_testany();
	void startNonBlockingReception(int handle);

#endif /* CONFIG_COMM_IRECV_TESTANY */

	void createBuffers();

	void checkDirtyBuffer(RingAllocator*outboxBufferAllocator,int i);
	void cleanDirtyBuffers(RingAllocator*outboxBufferAllocator);
	uint64_t m_linearSweeps;
	void initializeDirtyBuffers(RingAllocator*outboxBufferAllocator);

public:
	/** initialize the message handler
 * 	*/
	void constructor(int*argc,char***argv);

	/**
 *  send a message or more
 */
	void sendMessages(StaticVector*outbox,RingAllocator*outboxBufferAllocator);



	/**
 * receive one or zero message.
 * the others, if any, will be picked up in the next iteration
 */
	void receiveMessages(StaticVector*inbox,RingAllocator*inboxAllocator);

	/** free the ring elements */
	void freeLeftovers();

	/** get the processor name, usually set to the server name */
	string*getName();

	/** get the identifier of the current message passing interface rank */
	int getRank();

	/** get the number of ranks */
	int getSize();

	/** makes a barrier */
	void barrier();

	/** returns the version of the message passing interface standard that is available */
	void version(int*a,int*b);

	void destructor();

	/** write sent message counts to a file */
	void appendStatistics(const char*file);

	string getMessagePassingInterfaceImplementation();

	void setConnections(vector<int>*connections);

	void registerPlugin(ComputeCore*core);
	void resolveSymbols(ComputeCore*core);
};

#endif /* _MessagesHandler */


