/*
 	RayPlatform
    Copyright (C) 2010, 2011, 2012  Sébastien Boisvert

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

#include <communication/MessagesHandler.h>

/* for decoding message tags */
#include <communication/MessageRouter.h> 
#include <memory/allocator.h>
#include <core/OperatingSystem.h>
#include <core/ComputeCore.h>

#include <fstream>
#include <assert.h>
#include <iostream>
#include <sstream>
#include <string.h>
using namespace std;

// #define COMMUNICATION_IS_VERBOSE

#define __NOT_SET -1

/**
 * TODO Instead of a true/false state, increase and decrease requests
 * using a particular buffer. Otherwise, there may be a problem when 
 * a buffer is re-used several times for many requests.
 */
void MessagesHandler::checkDirtyBuffer(RingAllocator*outboxBufferAllocator,int index){

	#ifdef ASSERT
	assert(m_numberOfDirtyBuffers>0);
	#endif

	if(m_dirtyBuffers[index].m_buffer==NULL)// this entry is empty...
		return;

	// check the buffer and free it if it is finished.
	MPI_Status status;
	MPI_Request*request=&(m_dirtyBuffers[index].m_messageRequest);

	int flag=0;

	MPI_Test(request,&flag,&status);

	if(!flag)// this slot is not ready
		return;

	#ifdef ASSERT
	assert( flag );
	#endif /* ASSERT */

	void*buffer=m_dirtyBuffers[index].m_buffer;
	outboxBufferAllocator->salvageBuffer(buffer);
	m_numberOfDirtyBuffers--;

	#ifdef COMMUNICATION_IS_VERBOSE
	cout<<"From checkDirtyBuffer flag= "<<flag<<endl;
	#endif /* COMMUNICATION_IS_VERBOSE */

	#ifdef ASSERT
	assert(*request == MPI_REQUEST_NULL);
	#endif /* ASSERT */

	m_dirtyBuffers[index].m_buffer=NULL;
}

void MessagesHandler::cleanDirtyBuffers(RingAllocator*outboxBufferAllocator){

/**
 * don't do any linear sweep if we still have plenty of free cells
 */
	if(m_numberOfDirtyBuffers<m_minimumNumberOfDirtyBuffersForSweep)
		return;

	#ifdef ASSERT
	assert(m_numberOfDirtyBuffers>0);
	#endif

	m_linearSweeps++;

	// update the dirty buffer list.
	for(int i=0;i<m_dirtyBufferSlots;i++){
		if(m_numberOfDirtyBuffers==0)
			return;

		checkDirtyBuffer(outboxBufferAllocator,i);
	}

	if(m_numberOfDirtyBuffers>=m_minimumNumberOfDirtyBuffersForWarning){
		cout<<"[MessagesHandler] Warning: dirty buffers are still dirty after linear sweep."<<endl;
		printDirtyBuffers();
	}
}

void MessagesHandler::printDirtyBuffers(){

	cout<<"[MessagesHandler] Dirty buffers: "<<m_numberOfDirtyBuffers<<"/";
	cout<<m_dirtyBufferSlots<<endl;

	for(int i=0;i<m_dirtyBufferSlots;i++){
		cout<<"DirtyBuffer # "<<i<<"    State: ";
		if(m_dirtyBuffers[i].m_buffer==NULL){
			cout<<"Available"<<endl;
		}else{
			cout<<"Dirty"<<endl;

			MessageTag tag=m_dirtyBuffers[i].m_messageTag;
			Rank destination=m_dirtyBuffers[i].m_destination;
			Rank routingSource=m_rank;
			Rank routingDestination=destination;

			bool isRoutingTagValue=false;

/* TODO we don't have easy access to this information */
#if 0
			RoutingTag routingTag=tag;

			if(isRoutingTag(tag)){
				tag=getMessageTagFromRoutingTag(routingTag);
				routingSource=getSourceFromRoutingTag(routingTag);
				routingDestination=getDestinationFromRoutingTag(routingTag);

				isRoutingTagValue=true;
			}
#endif

			uint8_t index=tag;
			cout<<" MessageTag: "<<MESSAGE_TAGS[index]<<" ("<<(int)index<<") ";
			if(index<tag){
				cout<<"[this is a routing tag]"<<endl;
			}
			cout<<" Source: "<<m_rank<<endl;
			cout<<" Destination: "<<destination<<endl;

			if(isRoutingTagValue){
				cout<<" RoutingSource: "<<routingSource<<endl;
				cout<<" RoutingDestination: "<<routingDestination<<endl;
			}
		}
	}
}

void MessagesHandler::initializeDirtyBuffers(RingAllocator*outboxBufferAllocator){

	m_dirtyBufferSlots=outboxBufferAllocator->getNumberOfBuffers();

	m_dirtyBuffers=(DirtyBuffer*)__Malloc(m_dirtyBufferSlots*sizeof(DirtyBuffer),
		"m_dirtyBuffers",false);

	for(int i=0;i<m_dirtyBufferSlots;i++){
		m_dirtyBuffers[i].m_buffer=NULL;
	}

	// configure the real-time sweeper.

	m_minimumNumberOfDirtyBuffersForSweep=m_dirtyBufferSlots/4;
	m_minimumNumberOfDirtyBuffersForWarning=m_dirtyBufferSlots/2;

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
	if(m_dirtyBuffers==NULL)
		initializeDirtyBuffers(outboxBufferAllocator);

	cleanDirtyBuffers(outboxBufferAllocator);

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
		if(handle >=0 && m_dirtyBuffers[handle].m_buffer==NULL)
			mustRegister=true;

		/* register the buffer for processing */
		if(mustRegister){
			#ifdef ASSERT
			assert(m_dirtyBuffers[handle].m_buffer==NULL);
			#endif

			m_dirtyBuffers[handle].m_buffer=buffer;
			m_dirtyBuffers[handle].m_destination=destination;
			m_dirtyBuffers[handle].m_messageTag=tag;

			#ifdef ASSERT
			assert(m_dirtyBuffers[handle].m_buffer!=NULL);
			#endif

			request=&(m_dirtyBuffers[handle].m_messageRequest);

			/* this is O(1) */
			outboxBufferAllocator->markBufferAsDirty(buffer);

			m_numberOfDirtyBuffers++;

			// update the maximum number of dirty buffers
			// observed since the beginning.
			if(m_numberOfDirtyBuffers > m_maximumDirtyBuffers)
				m_maximumDirtyBuffers=m_numberOfDirtyBuffers;
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

		// if the message was re-routed, we don't care
		// we only fetch the tag for statistics
		uint8_t shortTag=tag;

		/** update statistics */
		m_messageStatistics[destination*RAY_MPI_TAG_DUMMY+shortTag]++;

		m_sentMessages++;
	}

	outbox->clear();
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

void MessagesHandler::receiveMessages(StaticVector*inbox,RingAllocator*inboxAllocator){
	#if defined CONFIG_COMM_IPROBE_ROUND_ROBIN

// round-robin reception seems to avoid starvation 
/** round robin with Iprobe will increase the latency because there will be a lot of calls to
 MPI_Iprobe that yield no messages at all */

	roundRobinReception(inbox,inboxAllocator);

	#elif defined CONFIG_COMM_PERSISTENT

	// use persistent communication
	pumpMessageFromPersistentRing(inbox,inboxAllocator);

	#elif defined CONFIG_COMM_IPROBE_ANY_SOURCE

	// receive any message
	// it is assumed that MPI is fair
	// otherwise there may be some starvation

	probeAndRead(MPI_ANY_SOURCE,MPI_ANY_TAG,inbox,inboxAllocator);

	#elif defined CONFIG_COMM_IRECV_TESTANY

	receiveMessages_irecv_testany(inbox,inboxAllocator);

	#else

	#error "No communication model is selected."

	#endif
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
	StaticVector*inbox,RingAllocator*inboxAllocator){

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

	MessageUnit*incoming=NULL;
	if(count > 0){
		incoming=(MessageUnit*)inboxAllocator->allocate(count*sizeof(MessageUnit));
	}

	MPI_Recv(incoming,count,datatype,actualSource,actualTag,MPI_COMM_WORLD,&status);

	Message aMessage(incoming,count,m_rank,actualTag,actualSource);
	inbox->push_back(aMessage);

	#ifdef ASSERT
	assert(aMessage.getDestination() == m_rank);
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

	__Free(m_messageStatistics,"RAY_MALLOC_TYPE_MESSAGE_STATISTICS",false);
	m_messageStatistics=NULL;
}

void MessagesHandler::constructor(int*argc,char***argv){

	m_destroyed=false;

	m_sentMessages=0;
	m_receivedMessages=0;
	m_datatype=MPI_UNSIGNED_LONG_LONG;

	MPI_Init(argc,argv);

	char serverName[1000];
	int len;

	/** initialize the message passing interface stack */
	MPI_Get_processor_name(serverName,&len);
	MPI_Comm_rank(MPI_COMM_WORLD,&m_rank);
	MPI_Comm_size(MPI_COMM_WORLD,&m_size);
	initialiseMembers();
	m_processorName=serverName;

	m_numberOfDirtyBuffers=0;

	m_maximumDirtyBuffers=m_numberOfDirtyBuffers;

	m_dirtyBuffers=NULL;

	m_linearSweeps=0;

/**
 * internally, there are N buffers for MPI_Isend. However,
 * these slots become dirty when they are used and become
 * clean again when MPI_Test says so.
 * But we don't want to do too much sweep operations. Instead,
 * we want amortized operations.
 */

	m_minimumNumberOfDirtyBuffersForSweep=__NOT_SET;
	m_minimumNumberOfDirtyBuffersForWarning=__NOT_SET;

	#ifdef CONFIG_COMM_IRECV_TESTANY
	m_requests=NULL;
	#endif
}

void MessagesHandler::createBuffers(){

	/** initialize message statistics to 0 */
	m_messageStatistics=(uint64_t*)__Malloc(RAY_MPI_TAG_DUMMY*m_size*sizeof(uint64_t),"RAY_MALLOC_TYPE_MESSAGE_STATISTICS",false);
	for(int rank=0;rank<m_size;rank++){
		for(int tag=0;tag<RAY_MPI_TAG_DUMMY;tag++){
			m_messageStatistics[rank*RAY_MPI_TAG_DUMMY+tag]=0;
		}
	}

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

void MessagesHandler::barrier(){
	MPI_Barrier(MPI_COMM_WORLD);
}

void MessagesHandler::version(int*a,int*b){
	MPI_Get_version(a,b);
}

void MessagesHandler::appendStatistics(const char*file){
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

	cout<<"[MessagesHandler] Hello, this is the layered communication vessel, status follows."<<endl;
	cout<<"Rank "<<m_rank<<": sent "<<m_sentMessages<<" messages, received "<<m_receivedMessages<<" messages."<<endl;
	cout<<"Rank "<<m_rank<<": the maximum number of dirty buffers was "<<m_maximumDirtyBuffers<<endl;
	cout<<"Rank "<<m_rank<<": "<<m_linearSweeps<<" linear sweep operations (threshold: ";
	cout<<m_minimumNumberOfDirtyBuffersForSweep<<" dirty buffers)"<<endl;
	cout<<"Rank "<<m_rank<<": Active peers (including self): "<<activePeers.size()<<" list:";

	for(int i=0;i<(int)activePeers.size();i++){
		cout<<" "<<activePeers[i];
	}
	cout<<endl;

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

void MessagesHandler::registerPlugin(ComputeCore*core){
	m_plugin=core->allocatePluginHandle();

	core->setPluginName(m_plugin,"MessagesHandler");
	core->setPluginDescription(m_plugin,"MPI wrapper with round-robin policy (bundled with RayPlatform)");
	core->setPluginAuthors(m_plugin,"Sébastien Boisvert");
	core->setPluginLicense(m_plugin,"GNU Lesser General License version 3");

	RAY_MPI_TAG_DUMMY=core->allocateMessageTagHandle(m_plugin);
	core->setMessageTagSymbol(m_plugin,RAY_MPI_TAG_DUMMY,"RAY_MPI_TAG_DUMMY");

	createBuffers();
}

void MessagesHandler::resolveSymbols(ComputeCore*core){
	RAY_MPI_TAG_DUMMY=core->getMessageTagFromSymbol(m_plugin,"RAY_MPI_TAG_DUMMY");
}

