/*
 	RayPlatform: a message-passing development framework
    Copyright (C) 2010, 2011, 2012 Sébastien Boisvert

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

#include "MessagesHandler.h"
#include "MessageRouter.h"

/* for decoding message tags */
#include <RayPlatform/memory/allocator.h>
#include <RayPlatform/core/OperatingSystem.h>
#include <RayPlatform/core/ComputeCore.h>

#include <fstream>
#include <assert.h>
#include <iostream>
#include <sstream>
#include <string.h>
using namespace std;

// #define COMMUNICATION_IS_VERBOSE
//#define CONFIG_DEBUG_MPI_RANK
//#define CONFIG_DEBUG_MINI_RANK_COMMUNICATION

#define CONFIG_MESSAGE_QUEUE_RETRY_WARNING 1024

/**
 *
 */
void MessagesHandler::sendAndReceiveMessagesForRankProcess(ComputeCore**cores,int miniRanksPerRank,bool*communicate){

	int deadMiniRanks=0;

	for(int i=0;i<miniRanksPerRank;i++){

		ComputeCore*core=cores[i];
		MessageQueue*outbox=core->getBufferedOutbox();

#ifdef CONFIG_USE_LOCKING
		outbox->lock();
#endif /* CONFIG_USE_LOCKING */

		if(outbox->isDead())
			deadMiniRanks++;

		#ifdef CONFIG_DEBUG_MPI_RANK
		cout<<"[MessagesHandler] mini-rank # "<<i<<" has outbox messages"<<endl;
		#endif

/*
 * The RingAllocator outbox allocator is not thread safe -- the MessagesHandler should 
 * have its own outbox allocator. Also, it is unclear at this point how the mini-rank should manage
 * its buffers. Perhaps it should also use the RingAllocator, but without anything related to dirty
 * buffers, just a rotating staircase round robin strategy.
 *
 * Messages are sent using the buffer system of the mini-rank.
 */
		if(outbox->hasContent()){
			sendMessagesForMiniRank(outbox,core->getBufferedOutboxAllocator(),miniRanksPerRank);
		}

#ifdef CONFIG_USE_LOCKING
		outbox->unlock();
#endif /* CONFIG_USE_LOCKING */

/*
 * The message reception is interleaved
 * with the send operations.
 *
 * This is the secret sauce of "mini-ranks" implementation in
 * RayPlatform.
 *
 * The obvious model would be to do:
 *
 * while(1){
 * 	receive();
 * 	send();
 * }
 *
 * However, receive() is O(1) because it receives at most 1 message
 * during 1 single call.
 *
 * A call to send() requires a loop over all the mini-ranks. Therefore,
 * send() is O(<number of cores>).
 *
 * The trick is therefore to do this instead:
 *
 * while(1){
 * 	for each mini-rank)owever, receive() is O(1) because it receives at most 1 message
 * 	during 1 single call.
 *
 * 	A call to send() requires a loop over all the mini-ranks. Therefore,
 * 	send() is O(<number of cores>).
 *
 * 	The trick is therefore to do this instead:
 *
 * 	while(1){
 * 		for(each mini-rank X){
 * 			send message of mini-rank X
 * 			receive()
 * 		}
 * 	}
 *
 */
		receiveMessagesForMiniRanks(cores,miniRanksPerRank);
	}

	#ifdef CONFIG_DEBUG_MPI_RANK
	cout<<"[RankProcess::receiveMessages]"<<endl;
	#endif

/* 
 * Since all mini-ranks died, it is no longer necessasry to do
 * the communication.
 */
	if(deadMiniRanks==miniRanksPerRank)
		(*communicate)=false;
}

/*
 * send messages,
 *
 * Regarding m_dirtyBufferSlots:
 *
 * this is the maximum number of dirty buffers
 * it should be at least the number of allocated
 * buffer in a RayPlatform virtual machine tick.
 *
 * For very large jobs, this might need to be increased
 * in order to avoid the situation in which
 * all the buffer for the outbox are dirty/used.
 */
void MessagesHandler::sendMessagesForMiniRank(MessageQueue*outbox,RingAllocator*outboxBufferAllocator,
	int miniRanksPerRank){

	// initialize the dirty buffer counters
	// this is done only once
	if(outboxBufferAllocator->getDirtyBuffers()==NULL)
		outboxBufferAllocator->initializeDirtyBuffers();

	outboxBufferAllocator->cleanDirtyBuffers();

	// send messages.
	while(outbox->hasContent()){
		Message message;
		Message*aMessage=&message;
		outbox->pop(aMessage);

		int miniRankDestination=aMessage->getDestination();
		int miniRankSource=aMessage->getSource();

		Rank destination=miniRankDestination/miniRanksPerRank;

		#if 0
		Rank source=miniRankSource/miniRanksPerRank;
		#endif

		void*buffer=aMessage->getBuffer();
		int count=aMessage->getCount();
		MessageTag tag=aMessage->getTag();

		#ifdef ASSERT
		assert(destination>=0);
		if(destination>=m_size){
			cout<<"Tag="<<tag<<" Destination="<<destination<<endl;
		}
		assert(destination<m_size);
		assert(!(buffer==NULL && count>0));

		// this assertion is invalid when using checksum calculation.
		//assert(count<=(int)(MAXIMUM_MESSAGE_SIZE_IN_BYTES/sizeof(MessageUnit)));
		#endif /* ASSERT */

/*
 * It would obviously be better if the copy could be avoid 
 * by using the original buffer directly.
 */
		MessageUnit*theBuffer=(MessageUnit*)outboxBufferAllocator->allocate(MAXIMUM_MESSAGE_SIZE_IN_BYTES);
		memcpy(theBuffer,buffer,count*sizeof(MessageUnit));

/*
 * Store minirank information too at the end.
 */
		theBuffer[count++]=miniRankSource;
		theBuffer[count++]=miniRankDestination;

		#ifdef CONFIG_DEBUG_MINI_RANK_COMMUNICATION
		cout<<"SEND Source= "<<miniRankSource<<" Destination= "<<miniRankDestination<<" Tag= "<<tag<<endl;
		#endif /* CONFIG_DEBUG_MINI_RANK_COMMUNICATION */


		MPI_Request dummyRequest;
		MPI_Request*request=&dummyRequest;

/*
 * Check if the buffer is already registered.
 */
		int bufferHandle=outboxBufferAllocator->getBufferHandle(theBuffer);
		bool registeredAlready=outboxBufferAllocator->isRegistered(bufferHandle);
/*
 * Register the buffer as being dirty.
 */

		if(!registeredAlready)
			request=outboxBufferAllocator->registerBuffer(theBuffer);

		#ifdef ASSERT
		assert(request!=NULL);
		assert(count>=2);
		#endif

		//  MPI_Isend
		//      Synchronous nonblocking. 
		MPI_Isend(theBuffer,count,m_datatype,destination,tag,MPI_COMM_WORLD,request);

		#if 0
		// if the message was re-routed, we don't care
		// we only fetch the tag for statistics
		uint8_t shortTag=tag;

		/** update statistics */
		m_messageStatistics[destination*RAY_MPI_TAG_DUMMY+shortTag]++;
		#endif

/*
 * We can free the dummy request because the buffer is already registered.
 * TODO: instead, there should be a reference count for each buffer.
 */
		if(registeredAlready)
			MPI_Request_free(request);

		m_sentMessages++;
	}
}

#ifdef CONFIG_COMM_PERSISTENT

/*	
 * receiveMessages is implemented as recommanded by Mr. George Bosilca from
the University of Tennessee (via the Open-MPI mailing list)

De: George Bosilca <bosilca@…>
Reply-to: Open MPI Developers <devel@…>
À: Open MPI Developers <devel@…>
Sujet: Re: [OMPI devel] Simple program (103 lines) makes Open-1.4.3 hang
Date: 2010-11-23 18:03:04

If you know the max size of the receives I would take a different approach. 
Post few persistent receives, and manage them in a circular buffer. 
Instead of doing an MPI_Iprobe, use MPI_Test on the current head of your circular buffer. 
Once you use the data related to the receive, just do an MPI_Start on your request.
This approach will minimize the unexpected messages, and drain the connections faster. 
Moreover, at the end it is very easy to MPI_Cancel all the receives not yet matched.

 */
void MessagesHandler::pumpMessageFromPersistentRing(StaticVector*inbox,RingAllocator*inboxAllocator){

	/* persistent communication is not enabled by default */
	int flag=0;
	MPI_Status status;
	MPI_Request*request=m_ring+m_head;
	MPI_Test(request,&flag,&status);

	if(flag){
		// get the length of the message
		// it is not necessary the same as the one posted with MPI_Recv_init
		// that one was a lower bound
		MessageTag  tag=status.MPI_TAG;
		Rank source=status.MPI_SOURCE;

		int count;
		MPI_Get_count(&status,m_datatype,&count);
		MessageUnit*filledBuffer=(MessageUnit*)m_buffers+m_head*MAXIMUM_MESSAGE_SIZE_IN_BYTES/sizeof(MessageUnit);

		// copy it in a safe buffer
		// internal buffers are all of length MAXIMUM_MESSAGE_SIZE_IN_BYTES
		MessageUnit*incoming=(MessageUnit*)inboxAllocator->allocate(MAXIMUM_MESSAGE_SIZE_IN_BYTES);

		memcpy(incoming,filledBuffer,count*sizeof(MessageUnit));

		// the request can start again
		MPI_Start(m_ring+m_head);
	
		// add the message in the internal inbox
		// this inbox is not the real inbox
		Message aMessage(incoming,count,m_rank,tag,source);

		inbox->push_back(aMessage);
	}

	// advance the ring.
	m_head++;

	if(m_head == m_ringSize)
		m_head = 0;
}

#endif /* CONFIG_COMM_PERSISTENT */

void MessagesHandler::receiveMessagesForMiniRanks(ComputeCore**cores,int miniRanksPerRank){

	// the code here will probe from rank source
	// with MPI_Iprobe

	#ifdef COMMUNICATION_IS_VERBOSE
	cout<<"call to probeAndRead source="<<source<<""<<endl;
	#endif /* COMMUNICATION_IS_VERBOSE */

	int flag=0;
	MPI_Status status;
	int source=MPI_ANY_SOURCE;
	int tag=MPI_ANY_TAG;

	MPI_Iprobe(source,tag,MPI_COMM_WORLD,&flag,&status);

	// nothing to receive...
	if(!flag)
		return;

/* 
 * The code below reads at most one message.
 */

	MPI_Datatype datatype=MPI_UNSIGNED_LONG_LONG;
	int actualTag=status.MPI_TAG;
	Rank actualSource=status.MPI_SOURCE;
	int count=-1;
	MPI_Get_count(&status,datatype,&count);

	#ifdef ASSERT
	assert(count >= 0);
	assert(count >= 2);// we need mini-rank numbers !
	#endif


	MPI_Recv(m_staticBuffer,count,datatype,actualSource,actualTag,MPI_COMM_WORLD,&status);

/*
 * Get the mini-rank source and mini-rank destination.
 */
	int miniRankSource=m_staticBuffer[count-2];
	int miniRankDestination=m_staticBuffer[count-1];

	#ifdef CONFIG_DEBUG_MINI_RANK_COMMUNICATION
	cout<<"RECEIVE Source= "<<miniRankSource<<" Destination= "<<miniRankDestination<<" Tag= "<<actualTag<<endl;
	#endif /* CONFIG_DEBUG_MINI_RANK_COMMUNICATION */

	count-=2;

	int miniRankIndex=miniRankDestination%miniRanksPerRank;

	#ifdef CONFIG_DEBUG_MPI_RANK
	cout<<"[MessagesHandler::probeAndRead] received message from "<<miniRanksPerRank<<" to "<<miniRankDestination;
	cout<<" tag is "<<actualTag<<endl;
	#endif

	MessageUnit*incoming=NULL;

	ComputeCore*core=cores[miniRankIndex];

/*
 * We can not receive a message if the inbox is not
 * empty.
 * Update: this is not true because we push the message
 * directly in a MessageQueue.
 */

	MessageQueue*inbox=core->getBufferedInbox();

#ifdef CONFIG_USE_LOCKING
/*
 * Lock the core and distribute the message.
 */

	#ifdef CONFIG_DEBUG_MPI_RANK
	cout<<"Rank tries to lock the inbox of minirank # "<<miniRankIndex<<endl;
	#endif

	inbox->lock();
#endif /* CONFIG_USE_LOCKING */

	if(count > 0){
		incoming=(MessageUnit*)core->getBufferedInboxAllocator()->allocate(count*sizeof(MessageUnit));

		#ifdef ASSERT
		assert(incoming!=NULL);
		#endif

		memcpy(incoming,m_staticBuffer,count*sizeof(MessageUnit));
	}

	Message aMessage(incoming,count,miniRankDestination,actualTag,miniRankSource);

/*
 * Try to push the message. If it does not work, just try again.
 * It should work eventually.
 */
	int ticks=0;

	while(!inbox->push(&aMessage)){

		if(ticks%CONFIG_MESSAGE_QUEUE_RETRY_WARNING ==0)
			cout<<"[MessagesHandler] Warning: inbox message queue is full, will retry in a bit... (#"<<ticks<<")"<<endl;

		ticks++;
	}

#ifdef CONFIG_USE_LOCKING
	inbox->unlock();
#endif /* CONFIG_USE_LOCKING */

	#ifdef ASSERT
	// this assertion is not valid for mini-ranks.
	//assert(aMessage.getDestination() == m_rank);
	#endif

	m_receivedMessages++;

}

#ifdef CONFIG_COMM_IRECV_TESTANY

/*
 * Implementation of the communication layer using
 * MPI_Irecv, MPI_Isend, and MPI_Testany
 *
 * According to Pavan Balaji (MPICH2) and Jeff Squyres (Open-MPI),
 * this should produce the best performance.
 */

/**
 * Starts a non-blocking reception
 */
void MessagesHandler::startNonBlockingReception(int handle){
	
	#ifdef ASSERT
	assert(handle<m_numberOfNonBlockingReceives);
	assert(handle>=0);
	#endif /* ASSERT */

	void*buffer=m_receptionBuffers+handle*m_bufferSize;

	MPI_Request*request=m_requests+handle;

	#ifdef ASSERT
	assert(buffer!=NULL);
	assert(request!=NULL);
	#endif /* ASSERT */

	MPI_Irecv(buffer,m_bufferSize/sizeof(MessageUnit),m_datatype,
		MPI_ANY_SOURCE,MPI_ANY_TAG,MPI_COMM_WORLD,request);
}

void MessagesHandler::init_irecv_testany(RingAllocator*inboxAllocator){

	m_bufferSize=inboxAllocator->getSize();
	m_numberOfNonBlockingReceives=m_size+10;

	m_requests=(MPI_Request*)__Malloc(sizeof(MPI_Request)*m_numberOfNonBlockingReceives,
		"CONFIG_COMM_IRECV_TESTANY",false);
	m_receptionBuffers=(uint8_t*)__Malloc(m_bufferSize*m_numberOfNonBlockingReceives,
		"CONFIG_COMM_IRECV_TESTANY",false);
	
	#ifdef ASSERT
	assert(m_requests!=NULL);
	#endif

	for(int i=0;i<m_numberOfNonBlockingReceives;i++)
		startNonBlockingReception(i);
}

void MessagesHandler::destroy_irecv_testany(){

	if(m_requests==NULL)
		return;

	for(int i=0;i<m_numberOfNonBlockingReceives;i++){
		MPI_Cancel(m_requests+i);
		MPI_Request_free(m_requests+i);
	}

	__Free(m_requests,"CONFIG_COMM_IRECV_TESTANY",false);
	m_requests=NULL;
	__Free(m_receptionBuffers,"CONFIG_COMM_IRECV_TESTANY",false);
	m_receptionBuffers=NULL;

}

void MessagesHandler::receiveMessages_irecv_testany(StaticVector*inbox,RingAllocator*inboxAllocator){

	if(m_requests==NULL)
		init_irecv_testany(inboxAllocator);

	int indexOfCompletedRequest=MPI_UNDEFINED;
	int hasCompleted=0;
	MPI_Status status;

	#ifdef ASSERT
	int returnValue=
	#endif /* ASSERT */

	MPI_Testany(m_numberOfNonBlockingReceives,m_requests,
		&indexOfCompletedRequest,&hasCompleted,&status);

	#ifdef ASSERT
	assert(returnValue==MPI_SUCCESS);
	#endif /* ASSERT */

	if(hasCompleted==0)
		return;

	MPI_Datatype datatype=MPI_UNSIGNED_LONG_LONG;
	MessageTag actualTag=status.MPI_TAG;
	Rank actualSource=status.MPI_SOURCE;
	int count=-1;
	MPI_Get_count(&status,datatype,&count);

	uint8_t*populatedBuffer=m_receptionBuffers+indexOfCompletedRequest*m_bufferSize;

	#ifdef ASSERT
	assert(count >= 0);
	#endif

	MessageUnit*incoming=NULL;
	if(count > 0){
		incoming=(MessageUnit*)inboxAllocator->allocate(count*sizeof(MessageUnit));
		memcpy(incoming,populatedBuffer,count*sizeof(MessageUnit));
	}

	Message aMessage(incoming,count,m_rank,actualTag,actualSource);
	inbox->push_back(aMessage);

	#ifdef ASSERT
	assert(aMessage.getDestination() == m_rank);
	#endif

	m_receivedMessages++;

	startNonBlockingReception(indexOfCompletedRequest);
}

#endif /* CONFIG_COMM_IRECV_TESTANY */

#ifdef CONFIG_COMM_IPROBE_ROUND_ROBIN

void MessagesHandler::roundRobinReception(StaticVector*inbox,RingAllocator*inboxAllocator){

	/* otherwise, we use MPI_Iprobe + MPI_Recv */
	/* probe and read a message */

	int operations=0;
	while(operations<m_size){

		probeAndRead(m_connections[m_currentRankIndexToTryToReceiveFrom],MPI_ANY_TAG,
			inbox,inboxAllocator);

		// advance the head
		m_currentRankIndexToTryToReceiveFrom++;
		if(m_currentRankIndexToTryToReceiveFrom==m_peers){
			m_currentRankIndexToTryToReceiveFrom=0;
		}

		// increment operations
		operations++;

		// only 1 MPI_Iprobe is called for any source
		#ifdef CONFIG_COMM_IPROBE_ANY_SOURCE
		break;
		#endif /* CONFIG_COMM_IPROBE_ANY_SOURCE */

		#ifdef CONFIG_COMM_IPROBE_ROUND_ROBIN
		#ifdef CONFIG_ONE_IPROBE_PER_TICK
		break;
		#endif /* CONFIG_ONE_IPROBE_PER_TICK */
		#endif /* CONFIG_COMM_IPROBE_ROUND_ROBIN */


		// we have read a message
		if(inbox->size()>0){
			break;
		}
	}

	#ifdef COMMUNICATION_IS_VERBOSE
	if(inbox->size() > 0){
		cout<<"[RoundRobin] received a message from "<<m_currentRankIndexToTryToReceiveFrom<<endl;
	}
	#endif /* COMMUNICATION_IS_VERBOSE */

}

#endif /* CONFIG_COMM_IPROBE_ROUND_ROBIN */

/* this code is utilised, 
 * the code for persistent communication is not useful because
 * round-robin policy and persistent communication are likely 
 * not compatible because the number of persistent requests can be a limitation.
 */
void MessagesHandler::probeAndRead(Rank source,MessageTag tag,
	ComputeCore**cores,int miniRanksPerRank){

	// the code here will probe from rank source
	// with MPI_Iprobe

	#ifdef COMMUNICATION_IS_VERBOSE
	cout<<"call to probeAndRead source="<<source<<""<<endl;
	#endif /* COMMUNICATION_IS_VERBOSE */

	int flag=0;
	MPI_Status status;
	MPI_Iprobe(source,tag,MPI_COMM_WORLD,&flag,&status);

	// nothing to receive...
	if(!flag)
		return;

	/* read at most one message */
	MPI_Datatype datatype=MPI_UNSIGNED_LONG_LONG;
	int actualTag=status.MPI_TAG;
	Rank actualSource=status.MPI_SOURCE;
	int count=-1;
	MPI_Get_count(&status,datatype,&count);

	#ifdef ASSERT
	assert(count >= 0);
	#endif

	MessageUnit staticBuffer[MAXIMUM_MESSAGE_SIZE_IN_BYTES/sizeof(MessageUnit)+2];

	MPI_Recv(staticBuffer,count,datatype,actualSource,actualTag,MPI_COMM_WORLD,&status);

/*
 * Get the mini-rank source and mini-rank destination.
 */
	int miniRankSource=staticBuffer[count-2];
	int miniRankDestination=staticBuffer[count-1];

	#ifdef CONFIG_DEBUG_MINI_RANK_COMMUNICATION
	cout<<"RECEIVE Source= "<<miniRankSource<<" Destination= "<<miniRankDestination<<" Tag= "<<actualTag<<endl;
	#endif /* CONFIG_DEBUG_MINI_RANK_COMMUNICATION */

	count-=2;

	int miniRankIndex=miniRankDestination%miniRanksPerRank;

	#ifdef CONFIG_DEBUG_MPI_RANK
	cout<<"[MessagesHandler::probeAndRead] received message from "<<miniRanksPerRank<<" to "<<miniRankDestination;
	cout<<" tag is "<<actualTag<<endl;
	#endif

	MessageUnit*incoming=NULL;

	ComputeCore*core=cores[miniRankIndex];

	if(count > 0){
		incoming=(MessageUnit*)core->getInboxAllocator()->allocate(count*sizeof(MessageUnit));

		memcpy(incoming,staticBuffer,count*sizeof(MessageUnit));
	}

	Message aMessage(incoming,count,miniRankDestination,actualTag,miniRankSource);
	core->getInbox()->push_back(&aMessage);

	#ifdef ASSERT
	// this assertion is not valid for mini-ranks.
	//assert(aMessage.getDestination() == m_rank);
	#endif

	m_receivedMessages++;
}

void MessagesHandler::initialiseMembers(){
	#ifdef CONFIG_COMM_PERSISTENT
	// the ring itself  contain requests ready to receive messages
	m_ringSize=m_size;

	m_ring=(MPI_Request*)__Malloc(sizeof(MPI_Request)*m_ringSize,"RAY_MALLOC_TYPE_PERSISTENT_MESSAGE_RING",false);
	m_buffers=(uint8_t*)__Malloc(MAXIMUM_MESSAGE_SIZE_IN_BYTES*m_ringSize,"RAY_MALLOC_TYPE_PERSISTENT_MESSAGE_BUFFERS",false);
	m_head=0;

	// post a few receives.
	for(int i=0;i<m_ringSize;i++){
		uint8_t*buffer=m_buffers+i*MAXIMUM_MESSAGE_SIZE_IN_BYTES;
		MPI_Recv_init(buffer,MAXIMUM_MESSAGE_SIZE_IN_BYTES/sizeof(MessageUnit),m_datatype,
			MPI_ANY_SOURCE,MPI_ANY_TAG,MPI_COMM_WORLD,m_ring+i);
		MPI_Start(m_ring+i);
	}

	#endif /* CONFIG_COMM_PERSISTENT */
}

void MessagesHandler::freeLeftovers(){

	#ifdef CONFIG_COMM_IRECV_TESTANY
	destroy_irecv_testany();
	#endif /* CONFIG_COMM_IRECV_TESTANY */

	#ifdef CONFIG_COMM_PERSISTENT

	for(int i=0;i<m_ringSize;i++){
		MPI_Cancel(m_ring+i);
		MPI_Request_free(m_ring+i);
	}

	__Free(m_ring,"RAY_MALLOC_TYPE_PERSISTENT_MESSAGE_RING",false);
	m_ring=NULL;
	__Free(m_buffers,"RAY_MALLOC_TYPE_PERSISTENT_MESSAGE_BUFFERS",false);
	m_buffers=NULL;

	#endif /* CONFIG_COMM_PERSISTENT */

	#if 0
	__Free(m_messageStatistics,"RAY_MALLOC_TYPE_MESSAGE_STATISTICS",false);
	m_messageStatistics=NULL;
	#endif
}

void MessagesHandler::constructor(int*argc,char***argv){

	m_destroyed=false;

	m_sentMessages=0;
	m_receivedMessages=0;
	m_datatype=MPI_UNSIGNED_LONG_LONG;

	#ifdef CONFIG_MINI_RANKS_disabled
	
	int provided;
	MPI_Init_thread(argc,argv, MPI_THREAD_FUNNELED, &provided);
	bool threads_ok = provided >= MPI_THREAD_FUNNELED;

	cout<<"Level provided: ";

	if(provided==MPI_THREAD_SINGLE)
		cout<<"MPI_THREAD_SINGLE";
	else if(provided==MPI_THREAD_FUNNELED)
		cout<<"MPI_THREAD_FUNNELED";
	else if(provided==MPI_THREAD_SERIALIZED)
		cout<<"MPI_THREAD_SERIALIZED";
	else if(provided==MPI_THREAD_MULTIPLE)
		cout<<"MPI_THREAD_MULTIPLE";
	cout<<endl;

	#ifdef ASSERT
	assert(threads_ok);
	#endif

	#else
	// same as MPI_Init_thread with MPI_THREAD_SINGLE
	// in some MPI libraries, MPI_THREAD_SINGLE and MPI_THREAD_FUNNELED are the same.
	MPI_Init(argc,argv);
	#endif

	char serverName[1000];
	int len;

	/** initialize the message passing interface stack */
	MPI_Get_processor_name(serverName,&len);
	MPI_Comm_rank(MPI_COMM_WORLD,&m_rank);
	MPI_Comm_size(MPI_COMM_WORLD,&m_size);
	initialiseMembers();
	m_processorName=serverName;



	#ifdef CONFIG_COMM_IRECV_TESTANY
	m_requests=NULL;
	#endif

	createBuffers();
}

void MessagesHandler::createBuffers(){

	#if 0
	/** initialize message statistics to 0 */
	m_messageStatistics=(uint64_t*)__Malloc(RAY_MPI_TAG_DUMMY*m_size*sizeof(uint64_t),"RAY_MALLOC_TYPE_MESSAGE_STATISTICS",false);
	for(int rank=0;rank<m_size;rank++){
		for(int tag=0;tag<RAY_MPI_TAG_DUMMY;tag++){
			m_messageStatistics[rank*RAY_MPI_TAG_DUMMY+tag]=0;
		}
	}
	#endif

	#ifdef CONFIG_COMM_IPROBE_ROUND_ROBIN
	// start with the first connection
	m_currentRankIndexToTryToReceiveFrom=0;
	#endif /* CONFIG_COMM_IPROBE_ROUND_ROBIN */

	// initialize connections
	for(int i=0;i<m_size;i++)
		m_connections.push_back(i);

	#ifdef CONFIG_COMM_IPROBE_ROUND_ROBIN
	cout<<"[MessagesHandler] Will use "<<m_connections.size()<<" connections for round-robin reception."<<endl;
	#endif

	m_peers=m_connections.size();
}


void MessagesHandler::destructor(){
	if(!m_destroyed){
		MPI_Finalize();
		m_destroyed=true;
	}

	freeLeftovers();
}

string*MessagesHandler::getName(){
	return &m_processorName;
}

int MessagesHandler::getRank(){
	return m_rank;
}

int MessagesHandler::getSize(){
	return m_size;
}

void MessagesHandler::version(int*a,int*b){
	MPI_Get_version(a,b);
}

void MessagesHandler::appendStatistics(const char*file){
	#if 0
	/** add an entry for RAY_MPI_TAG_GOOD_JOB_SEE_YOU_SOON_REPLY
 * 	because this message is sent after writting the current file
 *
 * 	actually, this RAY_MPI_TAG_GOOD_JOB_SEE_YOU_SOON_REPLY is not accounted for because
 * 	it is sent after printing the statistics.
 */

	cout<<"MessagesHandler::appendStatistics file= "<<file<<endl;

	ofstream fp;
	fp.open(file,ios_base::out|ios_base::app);
	vector<int> activePeers;
	for(int destination=0;destination<m_size;destination++){
		bool active=false;
		for(int tag=0;tag<RAY_MPI_TAG_DUMMY;tag++){
			uint64_t count=m_messageStatistics[destination*RAY_MPI_TAG_DUMMY+tag];

			if(count==0)
				continue;

			active=true;

			fp<<m_rank<<"\t"<<destination<<"\t"<<MESSAGE_TAGS[tag]<<"\t"<<count<<"\n";
		}
		if(active)
			activePeers.push_back(destination);
	}
	fp.close();

	#endif 

	cout<<"[MessagesHandler] Hello, this is the layered communication vessel, status follows."<<endl;
	cout<<"Rank "<<m_rank<<": sent "<<m_sentMessages<<" messages, received "<<m_receivedMessages<<" messages."<<endl;

	#if 0
	outboxBufferAllocator->printStatus()

	cout<<"Rank "<<m_rank<<": Active peers (including self): "<<activePeers.size()<<" list:";

	for(int i=0;i<(int)activePeers.size();i++){
		cout<<" "<<activePeers[i];
	}
	cout<<endl;
	#endif

	cout<<"[MessagesHandler] Over and out."<<endl;
}

string MessagesHandler::getMessagePassingInterfaceImplementation(){
	ostringstream implementation;

	#ifdef MPICH2
        implementation<<"MPICH2 (MPICH2) "<<MPICH2_VERSION;
	#endif

	#ifdef OMPI_MPI_H
        implementation<<"Open-MPI (OMPI_MPI_H) "<<OMPI_MAJOR_VERSION<<"."<<OMPI_MINOR_VERSION<<"."<<OMPI_RELEASE_VERSION;
	#endif

	if(implementation.str().length()==0)
		implementation<<"Unknown";
	return implementation.str();
}

// set connections to probe from
// we assume that connections are sorted.
void MessagesHandler::setConnections(vector<int>*connections){

	bool hasSelf=false;
	for(int i=0;i<(int)connections->size();i++){
		if(connections->at(i)==m_rank){
			hasSelf=true;
			break;
		}
	}

	m_connections.clear();

	// we have to make sure to insert the self link
	// at the good place.

	for(int i=0;i<(int)connections->size();i++){

		Rank otherRank=connections->at(i);

		// add the self rank
		if(!hasSelf){
			if(m_rank < otherRank){
				m_connections.push_back(m_rank);
				hasSelf=true;
			}
		}

		m_connections.push_back(otherRank);
	}

	// if we have not added the self link yet,
	// it is because it is greater.
	if(!hasSelf){
		m_connections.push_back(m_rank);
		hasSelf=true;
	}

	cout<<"[MessagesHandler] after runtime re-configuration, will use "<<m_connections.size()<<" connections for round-robin reception."<<endl;

	m_peers=m_connections.size();
}

/*
 * send messages,
 *
 * Regarding m_dirtyBufferSlots:
 *
 * this is the maximum number of dirty buffers
 * it should be at least the number of allocated
 * buffer in a RayPlatform virtual machine tick.
 *
 * For very large jobs, this might need to be increased
 * in order to avoid the situation in which
 * all the buffer for the outbox are dirty/used.
 */
void MessagesHandler::sendMessages(StaticVector*outbox,RingAllocator*outboxBufferAllocator){

	// initialize the dirty buffer counters
	// this is done only once
	if(outboxBufferAllocator->getDirtyBuffers()==NULL)
		outboxBufferAllocator->initializeDirtyBuffers();

	outboxBufferAllocator->cleanDirtyBuffers();

	// send messages.
	for(int i=0;i<(int)outbox->size();i++){
		Message*aMessage=((*outbox)[i]);
		Rank destination=aMessage->getDestination();
		void*buffer=aMessage->getBuffer();
		int count=aMessage->getCount();
		MessageTag tag=aMessage->getTag();

		#ifdef ASSERT
		assert(destination>=0);
		if(destination>=m_size){
			cout<<"Tag="<<tag<<" Destination="<<destination<<endl;
		}
		assert(destination<m_size);
		assert(!(buffer==NULL && count>0));

		// this assertion is invalid when using checksum calculation.
		//assert(count<=(int)(MAXIMUM_MESSAGE_SIZE_IN_BYTES/sizeof(MessageUnit)));
		#endif /* ASSERT */

		MPI_Request dummyRequest;

		MPI_Request*request=&dummyRequest;

		int handle=-1;

		/* compute the handle, this is O(1) */
		if(buffer!=NULL)
			handle=outboxBufferAllocator->getBufferHandle(buffer);

		bool mustRegister=false;

		// this buffer is not registered.
		if(handle >=0 && !outboxBufferAllocator->isRegistered(handle))
			mustRegister=true;

		/* register the buffer for processing */
		if(mustRegister){
			request=outboxBufferAllocator->registerBuffer(buffer);
		}

		#ifdef ASSERT
		assert(request!=NULL);
		#endif

		//  MPI_Isend
		//      Synchronous nonblocking. 
		MPI_Isend(buffer,count,m_datatype,destination,tag,MPI_COMM_WORLD,request);

		// if the buffer is NULL, we free the request right now
		// because we there is no buffer to protect.

		if(!mustRegister){
			#ifdef COMMUNICATION_IS_VERBOSE
			cout<<" From sendMessages"<<endl;
			#endif

			MPI_Request_free(request);

			#ifdef ASSERT
			assert(*request==MPI_REQUEST_NULL);
			#endif
		}

		#if 0
		// if the message was re-routed, we don't care
		// we only fetch the tag for statistics
		uint8_t shortTag=tag;

		/** update statistics */
		m_messageStatistics[destination*RAY_MPI_TAG_DUMMY+shortTag]++;

		#endif

		m_sentMessages++;
	}

	outbox->clear();
}


void MessagesHandler::receiveMessages(StaticVector*inbox,RingAllocator*inboxAllocator){
	// the code here will probe from rank source
	// with MPI_Iprobe

	#ifdef COMMUNICATION_IS_VERBOSE
	cout<<"call to probeAndRead source="<<source<<""<<endl;
	#endif /* COMMUNICATION_IS_VERBOSE */
	
	int source=MPI_ANY_SOURCE;
	int tag=MPI_ANY_TAG;

	int flag=0;
	MPI_Status status;
	MPI_Iprobe(source,tag,MPI_COMM_WORLD,&flag,&status);

	// nothing to receive...
	if(!flag)
		return;

	/* read at most one message */
	MPI_Datatype datatype=MPI_UNSIGNED_LONG_LONG;
	int actualTag=status.MPI_TAG;
	Rank actualSource=status.MPI_SOURCE;
	int count=-1;
	MPI_Get_count(&status,datatype,&count);

	#ifdef ASSERT
	assert(count >= 0);
	#endif

	MessageUnit*incoming=NULL;
	if(count > 0){
		incoming=(MessageUnit*)inboxAllocator->allocate(count*sizeof(MessageUnit));
	}

	MPI_Recv(incoming,count,datatype,actualSource,actualTag,MPI_COMM_WORLD,&status);

	Message aMessage(incoming,count,m_rank,actualTag,actualSource);
	inbox->push_back(&aMessage);

	#ifdef ASSERT
	assert(aMessage.getDestination() == m_rank);
	#endif

	m_receivedMessages++;

}

void MessagesHandler::sendMessagesForComputeCore(StaticVector*outbox,MessageQueue*bufferedOutbox){

#ifdef CONFIG_USE_LOCKING
	bufferedOutbox.lock();
#endif /* CONFIG_USE_LOCKING */

	int messages=outbox->size();

/*
 * TODO: I am not sure that it is safe to give our own buffer to
 * the other thread. If the ring is large enough and the communication
 * is steady, probably.
 */
	for(int i=0;i<messages;i++){
		Message*message=(*outbox)[i];

/*
 * If the queue is full, we just retry endlessly, it will work eventually.
 * If not, something is wrong.
 */
		int ticks=0;

		while(!bufferedOutbox->push(message)){

			if(ticks%CONFIG_MESSAGE_QUEUE_RETRY_WARNING ==0)
				cout<<"[MessagesHandler] Warning: outbox message queue is full, will retry in a bit... (#"<<ticks<<")"<<endl;

			ticks++;
		}
	}

#ifdef CONFIG_USE_LOCKING
	bufferedOutbox->unlock();
#endif /* CONFIG_USE_LOCKING */

}

void MessagesHandler::receiveMessagesForComputeCore(StaticVector*inbox,RingAllocator*inboxAllocator,
	MessageQueue*bufferedInbox){

#ifdef CONFIG_USE_LOCKING
	bufferedInbox->lock();
#endif /* CONFIG_USE_LOCKING */

/*
 * We need to copy the buffer in our own buffer here
 * because otherwise this is not thread safe.
 */
	if(bufferedInbox->hasContent()){
		Message message;
		bufferedInbox->pop(&message);

		int count=message.getCount();
		MessageUnit*incoming=(MessageUnit*)inboxAllocator->allocate(count*sizeof(MessageUnit));

/*
 * Copy the data, this is slow. 
 */
		memcpy(incoming,message.getBuffer(),count*sizeof(MessageUnit));

/*
 * Update the buffer so that the code is thread-safe.
 */
		message.setBuffer(incoming);

		inbox->push_back(&message);
	}

#ifdef CONFIG_USE_LOCKING
	m_bufferedInbox.unlock();
#endif /* CONFIG_USE_LOCKING */

}

