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

#include "Message.h"
#include "mpi_tags.h"

#include <RayPlatform/cryptography/crypto.h>

#include <iostream>
using namespace std;

#include <string.h>

#ifdef CONFIG_ASSERT
#include <assert.h>
#endif

#define ACTOR_MODEL_NOBODY -1

/**
 * We always pad the buffer with actor source and actor destination.
 * If routing is enabled, we also pad with the route source and the route destination.
 * Finally, the buffer may be padded with a checksum too !
 *

 */
#define MESSAGE_META_DATA_ACTOR_SOURCE		0
#define MESSAGE_META_DATA_ACTOR_DESTINATION	4
#define MESSAGE_META_DATA_ROUTE_SOURCE	 	8
#define MESSAGE_META_DATA_ROUTE_DESTINATION	12
#define MESSAGE_META_DATA_CHECKSUM	 	16


Message::Message() {

	initialize();
}

void Message::initialize() {
	m_buffer = NULL;
	m_count = 0;
	m_tag = 0;
	m_source = 0;
	m_destination = 0;
	m_sourceActor = ACTOR_MODEL_NOBODY;
	m_destinationActor = ACTOR_MODEL_NOBODY;
}

Message::~Message() {
}

/** buffer must be allocated or else it will CORE DUMP. */
Message::Message(MessageUnit*b,int c,Rank dest,MessageTag tag,Rank source){

	initialize();

	m_buffer=b;
	m_count=c;
	m_destination=dest;
	m_tag=tag;
	m_source=source;

	/*
	cout << "DEBUG in constructor -> ";
	printActorMetaData();
	cout << endl;
	*/
}

MessageUnit*Message::getBuffer(){
	return m_buffer;
}

int Message::getCount() const{
	return m_count;
}

Rank Message::getDestination() const{
	return m_destination;
}

MessageTag Message::getTag() const{
	return m_tag;
}

//Message::Message(){}

int Message::getSource() const{
	return m_source;
}

void Message::print(){
	uint8_t shortTag=getTag();

	cout<<"Source: "<<getSource()<<" Destination: "<<getDestination()<<" Tag: "<<MESSAGE_TAGS[shortTag];
	cout<<" RealTag: "<<getTag();
	cout<<" Count: "<<getCount();

	if(m_count > 0){
		cout<<" Overlay: "<<m_buffer[0];
	}
}

void Message::setBuffer(MessageUnit*buffer){
	m_buffer = buffer;
}

void Message::setTag(MessageTag tag){
	m_tag=tag;
}

void Message::setCount(int count){
	m_count=count;
}

void Message::setSource(Rank source){
	m_source=source;
}

void Message::setDestination(Rank destination){
	m_destination=destination;
}

bool Message::isActorModelMessage() const {

	return ( m_sourceActor != ACTOR_MODEL_NOBODY
		       	&& m_destinationActor != ACTOR_MODEL_NOBODY );
}


int Message::getDestinationActor() const {
	return m_destinationActor;
}

int Message::getSourceActor() const {
	return m_sourceActor;
}

void Message::setSourceActor(int sourceActor) {
	m_sourceActor = sourceActor;
}

void Message::setDestinationActor(int destinationActor) {
	m_destinationActor = destinationActor;
}

void Message::saveActorMetaData() {


#ifdef CONFIG_ASSERT
	int bytes = getCount() * sizeof(MessageUnit);
	uint32_t checksumBefore = computeCyclicRedundancyCode32((uint8_t*)getBuffer(), bytes);
#endif
	//cout << "DEBUG saveActorMetaData tag " << getTag() << endl;

	int offset = getCount() * sizeof(MessageUnit);

	// actor metadata must be the first to be saved.
	offset -= 0;

	char * memory = (char*) getBuffer();

	memcpy(memory + offset + MESSAGE_META_DATA_ACTOR_SOURCE, &m_sourceActor, sizeof(int));
	memcpy(memory + offset + MESSAGE_META_DATA_ACTOR_DESTINATION, &m_destinationActor, sizeof(int));
	//printActorMetaData();

	m_count += 1; // add 1 uint64_t

	/*
	cout << "DEBUG after saveActorMetaData ";
	printActorMetaData();
	cout << endl;
	*/

#ifdef CONFIG_ASSERT
	uint32_t checksumAfter = computeCyclicRedundancyCode32((uint8_t*)getBuffer(), bytes);

	assert(checksumBefore == checksumAfter);
#endif
}

void Message::printActorMetaData() {
	cout << "DEBUG printActorMetaData tag= " << getTag();
	cout << " m_sourceActor = " << getSourceActor();
	cout << " m_destinationActor = " << getDestinationActor() << " ";
	cout << " bytes= " << m_count * sizeof(MessageUnit);

}

void Message::loadActorMetaData() {
/*
	cout << "DEBUG before loadActorMetaData -> ";
	printActorMetaData();
	cout << endl;
*/
	// m_count already contains the actor metadata.
	int offset = getCount() * sizeof(MessageUnit);
	offset -= 2 * sizeof(int);

	char * memory = (char*) getBuffer();

	memcpy(&m_sourceActor, memory + offset + MESSAGE_META_DATA_ACTOR_SOURCE, sizeof(int));
	memcpy(&m_destinationActor, memory + offset + MESSAGE_META_DATA_ACTOR_DESTINATION, sizeof(int));


	/*
	cout << "DEBUG loadActorMetaData m_sourceActor= " << m_sourceActor;
	cout << " m_destinationActor= " << m_destinationActor << endl;
*/
	// remove 1 uint64_t
	
	m_count -= 1;
/*
	cout << "DEBUG after loadActorMetaData -> ";
	printActorMetaData();
	cout << endl;
	*/
}

int Message::getMetaDataSize() const {
	return 4 * sizeof(int);
}

void Message::setRoutingSource(int source) {

	m_routingSource = source;
}

void Message::setRoutingDestination(int destination) {

	m_routingDestination = destination;
}

void Message::saveRoutingMetaData() {

	/*
	cout << "DEBUG saveRoutingMetaData m_routingSource " << m_routingSource;
	cout << " m_routingDestination " << m_routingDestination << endl;
*/
	int offset = getCount() * sizeof(MessageUnit);
	char * memory = (char*) getBuffer();

	// the count already include the actor addresses
	// this is stupid, but hey, nobody aside me is touching this code
	// with a ten-foot stick
	offset -= 2 * sizeof(int);

	memcpy(memory + offset + MESSAGE_META_DATA_ROUTE_SOURCE, &m_routingSource, sizeof(int));
	memcpy(memory + offset + MESSAGE_META_DATA_ROUTE_DESTINATION, &m_routingDestination, sizeof(int));
	//printActorMetaData();


	m_count += 1; // add 1 uint64_t

	//cout << "DEBUG saved routing metadata at offset " << offset << " new count " << m_count << endl;
	//displayMetaData();
}


int Message::getRoutingSource() const {

	return m_routingSource;
}

int Message::getRoutingDestination() const {

	return m_routingDestination;
}

void Message::loadRoutingMetaData() {
	//cout << "DEBUG loadRoutingMetaData count " << getCount() << endl;

	int offset = getCount() * sizeof(MessageUnit);

	// m_count contains actor metadata *AND* routing metadata (if necessary)
	offset -= 2 * sizeof(int);
	offset -= 2 * sizeof(int);

	char * memory = (char*) getBuffer();

	memcpy(&m_routingSource, memory + offset + MESSAGE_META_DATA_ROUTE_SOURCE, sizeof(int));
	memcpy(&m_routingDestination, memory + offset + MESSAGE_META_DATA_ROUTE_DESTINATION, sizeof(int));

	m_count -= 1;

	/*
	cout << "DEBUG loadRoutingMetaData ";
	cout << "loaded m_routingSource ";
	cout << m_routingSource;
	cout << " m_routingDestination " << m_routingDestination;
	cout << " offset " << offset ;
	printActorMetaData();
	cout << endl;
	*/

	//displayMetaData();

	// these can not be negative, otherwise
	// this method would not have been called now.
#ifdef CONFIG_ASSERT
	assert(m_routingSource >= 0);
	assert(m_routingDestination >= 0);
#endif
}

void Message::displayMetaData() {
	Message * aMessage = this;

	cout << " DEBUG displayMetaData count " << aMessage->getCount();
	cout << " tag " << getTag() << endl;

	int offset = getCount() * sizeof(MessageUnit) - 2 * 2 * sizeof(int);

	for(int i = 0 ; i < 4 ; ++i) {
		cout << " [" << i << " -> " << ((int*)aMessage->getBuffer())[offset + i] << "]";
	}
	cout << endl;

}
