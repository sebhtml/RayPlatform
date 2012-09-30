/*
 	Ray
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

//#define CONFIG_ROUTING_VERBOSITY

/**
 * \brief Message router implementation
 *
 * \author Sébastien Boisvert 2011-11-04
 * \reviewedBy Elénie Godzaridis 2011-11-05
*/

#include <communication/MessageRouter.h>
#include <string.h> /* for memcpy */
#include <assert.h>
#include <core/OperatingSystem.h>
#include <core/ComputeCore.h>
#include <time.h> /* for time */
using namespace std;

/* 
 * According to the MPI standard, MPI_TAG_UB is >= 32767 (2^15-1) 
 * Therefore, the tag must be >=0 and <= 32767 
 * In most applications, there will be not that much tag
 * values.
 * 2^14 = 16384
 * So that MPI tag in applications using RayPlatform can be
 * from 0 to 16383.
 * Internally, RayPlatform adds 16384 to the application tag.
 * A routing tag is >= 16384 and the corresponding real tag is
 * tag - 16384.
 */
#define __ROUTING_TAG_BASE 16384 

#define __ROUTING_SOURCE 0
#define __ROUTING_DESTINATION 1
#define __ROUTING_OFFSET(count) (count-1)

/*
#define ASSERT
*/

/**
 * route outcoming messages
 */
void MessageRouter::routeOutcomingMessages(){
	int numberOfMessages=m_outbox->size();

	if(numberOfMessages==0)
		return;

	for(int i=0;i<numberOfMessages;i++){
		Message*aMessage=m_outbox->at(i);

		MessageTag communicationTag=aMessage->getTag();

		#ifdef CONFIG_ROUTING_VERBOSITY
		uint8_t printableTag=communicationTag;
		cout<<"[routeOutcomingMessages] tag= "<<MESSAGE_TAGS[printableTag]<<" value="<<communicationTag<<endl;
		#endif

		// - first, the message may have been already routed when it was received (also
		// in a routed version). In this case, nothing must be done.
		if(isRoutingTag(communicationTag)){
			#ifdef CONFIG_ROUTING_VERBOSITY
			cout<<"["<<__func__<<"] Message has already a routing tag."<<endl;
			#endif
			continue;
		}

		// at this point, the message has no routing information yet.
		Rank trueSource=aMessage->getSource();
		Rank trueDestination=aMessage->getDestination();

		// if it is reachable, no further routing is required
		if(m_graph.isConnected(trueSource,trueDestination)){

			#ifdef CONFIG_ROUTING_VERBOSITY
			cout<<"["<<__func__<<"] Rank "<<trueSource<<" can reach "<<trueDestination<<" without routing"<<endl;
			#endif

			continue;
		}
	
		// re-route the message by re-writing the tag
		MessageTag routingTag=getRoutingTag(communicationTag);
		aMessage->setTag(routingTag);

		MessageUnit*buffer=aMessage->getBuffer();

		// we need space for routing information
		// also, if this is a control message sent to all, we need
		// to allocate new buffers.
		// There is no problem at rewritting buffers that have non-null buffers,
		// but it is useless if the buffer is used once.
		// So numberOfMessages==m_size is just an optimization.
		if(aMessage->getBuffer()==NULL||numberOfMessages==m_size){

			#ifdef ASSERT
			assert(aMessage->getCount()==0||numberOfMessages==m_size);
			#endif

			buffer=(MessageUnit*)m_outboxAllocator->allocate(MAXIMUM_MESSAGE_SIZE_IN_BYTES);

			#ifdef ASSERT
			assert(buffer!=NULL);
			#endif

			// copy the old stuff too
			if(aMessage->getBuffer()!=NULL)
				memcpy(buffer,aMessage->getBuffer(),aMessage->getCount()*sizeof(MessageUnit));

			aMessage->setBuffer(buffer);
		}

		// routing information is stored in 64 bits
		int newCount=aMessage->getCount()+1;
		aMessage->setCount(newCount);
		
		#ifdef ASSERT
		assert(buffer!=NULL);
		#endif

		setSourceInBuffer(buffer,newCount,trueSource);
		setDestinationInBuffer(buffer,newCount,trueDestination);

		#ifdef ASSERT
		assert(getSourceFromBuffer(aMessage->getBuffer(),aMessage->getCount())==trueSource);
		assert(getDestinationFromBuffer(aMessage->getBuffer(),aMessage->getCount())==trueDestination);
		#endif

		Rank nextRank=m_graph.getNextRankInRoute(trueSource,trueDestination,m_rank);
		aMessage->setDestination(nextRank);

		#ifdef CONFIG_ROUTING_VERBOSITY
		cout<<"["<<__func__<<"] relayed message, trueSource="<<trueSource<<" trueDestination="<<trueDestination<<" to intermediateSource "<<nextRank<<endl;
		#endif
	}

	// check that all messages are routable
	#ifdef ASSERT
	for(int i=0;i<numberOfMessages;i++){
		Message*aMessage=m_outbox->at(i);
		if(!m_graph.isConnected(aMessage->getSource(),aMessage->getDestination()))
			cout<<aMessage->getSource()<<" and "<<aMessage->getDestination()<<" are not connected !"<<endl;
		assert(m_graph.isConnected(aMessage->getSource(),aMessage->getDestination()));
	}
	#endif
}

/**
 * route incoming messages 
 * \returns true if rerouted something.
 */
bool MessageRouter::routeIncomingMessages(){
	int numberOfMessages=m_inbox->size();

	#ifdef CONFIG_ROUTING_VERBOSITY
	cout<<"["<<__func__<<"] inbox.size= "<<numberOfMessages<<endl;
	#endif

	// we have no message
	if(numberOfMessages==0)
		return false;

	// otherwise, we have exactly one precious message.
	
	Message*aMessage=m_inbox->at(0);
	MessageTag tag=aMessage->getTag();
	MessageUnit*buffer=aMessage->getBuffer();
	int count=aMessage->getCount();

	#ifdef CONFIG_ROUTING_VERBOSITY
	uint8_t printableTag=tag;
	cout<<"[routeIncomingMessages] tag= "<<MESSAGE_TAGS[printableTag]<<" value="<<tag<<endl;
	#endif

	// if the message has no routing tag, then we can safely receive it as is
	if(!isRoutingTag(tag)){
		// nothing to do
		#ifdef CONFIG_ROUTING_VERBOSITY
		cout<<"["<<__func__<<"] message has no routing tag, nothing to do"<<endl;
		#endif

		return false;
	}

	Rank trueSource=getSourceFromBuffer(buffer,count);
	Rank trueDestination=getDestinationFromBuffer(buffer,count);

	// this is the final destination
	// we have received the message
	// we need to restore the original information now.
	if(trueDestination==m_rank){
		#ifdef CONFIG_ROUTING_VERBOSITY
		cout<<"["<<__func__<<"] message has reached destination, must strip routing information"<<endl;
		#endif

		// we must update the original source and original tag
		aMessage->setSource(trueSource);
		
		// the original destination is already OK
		#ifdef ASSERT
		assert(aMessage->getDestination()==m_rank);
		#endif

		MessageTag trueTag=getMessageTagFromRoutingTag(tag);
		aMessage->setTag(trueTag);

		#ifdef CONFIG_ROUTING_VERBOSITY
		cout<<"[routeIncomingMessages] real tag= "<<trueTag<<endl;
		#endif

		// remove the routing stuff
		int newCount=aMessage->getCount()-1;

		#ifdef ASSERT
		assert(newCount>=0);
		#endif

		aMessage->setCount(newCount);

		// set the buffer to NULL if there is no data
		if(newCount==0)
			aMessage->setBuffer(NULL);

		return false;
	}

	#ifdef ASSERT
	assert(m_rank!=trueDestination);
	#endif

	// at this point, we know that we need to forward
	// the message to another peer
	Rank nextRank=m_graph.getNextRankInRoute(trueSource,trueDestination,m_rank);

	#ifdef CONFIG_ROUTING_VERBOSITY
	cout<<"["<<__func__<<"] message has been sent to the next one, trueSource="<<trueSource<<" trueDestination= "<<trueDestination;
	cout<<" Previous= "<<aMessage->getSource()<<" Current= "<<m_rank<<" Next= "<<nextRank<<endl;
	#endif
		
	// we forward the message
	relayMessage(aMessage,nextRank);

	// propagate the information about routing
	return true;
}

/**
 * forward a message to follow a route
 */
void MessageRouter::relayMessage(Message*message,Rank destination){
	
	int count=message->getCount();

	// routed messages always have a payload
	#ifdef ASSERT
	assert(count>=1);
	#endif

	MessageUnit*outgoingMessage=(MessageUnit*)m_outboxAllocator->allocate(MAXIMUM_MESSAGE_SIZE_IN_BYTES);

	// copy the data into the new buffer
	memcpy(outgoingMessage,message->getBuffer(),count*sizeof(MessageUnit));
	message->setBuffer(outgoingMessage);

	#ifdef CONFIG_ROUTING_VERBOSITY
	cout<<"[relayMessage] TrueSource="<<getSourceFromBuffer(message->getBuffer(),message->getCount());
	cout<<" TrueDestination="<<getDestinationFromBuffer(message->getBuffer(),message->getCount());
	cout<<" RelaySource="<<m_rank<<" RelayDestination="<<destination<<endl;
	#endif

	// re-route the message
	message->setSource(m_rank);
	message->setDestination(destination);

	#ifdef ASSERT
	assert(m_graph.isConnected(m_rank,destination));
	#endif

	m_outbox->push_back(*message);
}


/**
 * a tag is a routing tag is its routing bit is set to 1
 */
bool MessageRouter::isEnabled(){
	return m_enabled;
}

MessageRouter::MessageRouter(){
	m_enabled=false;
}

void MessageRouter::enable(StaticVector*inbox,StaticVector*outbox,RingAllocator*outboxAllocator,Rank rank,
	string prefix,int numberOfRanks,string type,int degree){

	m_graph.buildGraph(numberOfRanks,type,rank==MASTER_RANK,degree);

	m_size=numberOfRanks;

	cout<<endl;

	cout<<"[MessageRouter] Enabled message routing"<<endl;

	m_inbox=inbox;
	m_outbox=outbox;
	m_outboxAllocator=outboxAllocator;
	m_rank=rank;
	m_enabled=true;

	if(m_rank==MASTER_RANK)
		m_graph.writeFiles(prefix);

	m_deletionTime=0;
}

bool MessageRouter::hasCompletedRelayEvents(){

	int duration=16;

	if(m_deletionTime==0){
		m_deletionTime=time(NULL);
		cout<<"[MessageRouter] Rank "<<m_rank<<" will die in "<<duration<<" seconds,";
		cout<<" will not route anything after that point."<<endl;
	}

	time_t now=time(NULL);

	return now >= (m_deletionTime+duration);
}

/*
 * just add a magic number
 */
MessageTag MessageRouter::getRoutingTag(MessageTag tag){
	#ifdef ASSERT
	assert(tag>=0);
	assert(tag<32768);
	#endif

	return tag+__ROUTING_TAG_BASE;
}

ConnectionGraph*MessageRouter::getGraph(){
	return &m_graph;
}

bool MessageRouter::isRoutingTag(MessageTag tag){
	return tag>=__ROUTING_TAG_BASE;
}

MessageTag MessageRouter::getMessageTagFromRoutingTag(MessageTag tag){

	#ifdef ASSERT
	assert(isRoutingTag(tag));
	assert(tag>=__ROUTING_TAG_BASE);
	#endif

	tag-=__ROUTING_TAG_BASE;

	#ifdef ASSERT
	assert(tag>=0);
	#endif

	return tag;
}

Rank MessageRouter::getSourceFromBuffer(MessageUnit*buffer,int count){
	#ifdef ASSERT
	assert(count>=1);
	assert(buffer!=NULL);
	#endif

	uint32_t*routingInformation=(uint32_t*)(buffer+__ROUTING_OFFSET(count));

	Rank rank=routingInformation[__ROUTING_SOURCE];

	#ifdef ASSERT
	assert(rank>=0);
	assert(rank<m_size);
	#endif

	return rank;
}

Rank MessageRouter::getDestinationFromBuffer(MessageUnit*buffer,int count){

	#ifdef ASSERT
	assert(count>=1);
	assert(buffer!=NULL);
	#endif

	uint32_t*routingInformation=(uint32_t*)(buffer+__ROUTING_OFFSET(count));

	Rank rank=routingInformation[__ROUTING_DESTINATION];

	#ifdef ASSERT
	assert(rank>=0);
	assert(rank<m_size);
	#endif

	return rank;
}

void MessageRouter::setSourceInBuffer(MessageUnit*buffer,int count,Rank source){

	#ifdef CONFIG_ROUTING_VERBOSITY
	cout<<"[setSourceInBuffer] buffer="<<buffer<<" source="<<source<<endl;
	#endif

	#ifdef ASSERT
	assert(count>=1);
	assert(buffer!=NULL);
	assert(source>=0);
	assert(source<m_size);
	#endif

	uint32_t*routingInformation=(uint32_t*)(buffer+__ROUTING_OFFSET(count));

	routingInformation[__ROUTING_SOURCE]=source;
}

void MessageRouter::setDestinationInBuffer(MessageUnit*buffer,int count,Rank destination){

	#ifdef CONFIG_ROUTING_VERBOSITY
	cout<<"[setDestinationInBuffer] buffer="<<buffer<<" destination="<<destination<<endl;
	#endif

	#ifdef ASSERT
	assert(count>=1);
	assert(buffer!=NULL);
	assert(destination>=0);
	assert(destination<m_size);
	#endif

	uint32_t*routingInformation=(uint32_t*)(buffer+__ROUTING_OFFSET(count));

	routingInformation[__ROUTING_DESTINATION]=destination;
}


