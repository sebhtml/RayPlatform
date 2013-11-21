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
#include <iomanip>
using namespace std;

#include <string.h>

#ifdef CONFIG_ASSERT
#include <assert.h>
#endif

#include <stdio.h>

#define NO_VALUE -1
#define ACTOR_MODEL_NOBODY NO_VALUE
#define ROUTING_NO_VALUE NO_VALUE

/**
 * We always pad the buffer with actor source and actor destination.
 * If routing is enabled, we also pad with the route source and the route destination.
 * Finally, the buffer may be padded with a checksum too !
 *

 */

Message::Message() {

	initialize();
}

void Message::initialize() {
	m_buffer = NULL;
	//m_count = 0;
	m_bytes = 0;
	m_tag = 0;
	m_source = 0;
	m_destination = 0;
	m_sourceActor = ACTOR_MODEL_NOBODY;
	m_destinationActor = ACTOR_MODEL_NOBODY;

	m_routingSource = ROUTING_NO_VALUE;
	m_routingDestination = ROUTING_NO_VALUE;

	m_miniRankSource = NO_VALUE;
	m_miniRankDestination = NO_VALUE;
}

Message::~Message() {
}

/**
 * buffer must be allocated or else it will CORE DUMP.
 */
Message::Message(void*b,int c,Rank dest,MessageTag tag,Rank source){

	initialize();

	m_buffer=b;
	//m_count=c;
	m_bytes = c * sizeof(MessageUnit);
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
	return (MessageUnit*)m_buffer;
}

char * Message::getBufferBytes() {
	return (char*) m_buffer;
}

int Message::getCount() const{
	return m_bytes / sizeof(MessageUnit);
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
	//uint8_t shortTag=getTag();

	cout<<"Source: "<<getSource()<<" Destination: "<<getDestination();

	/*
	if(isActorModelMessage(0)) {
		cout << " ActorModel: Yes.";
	} else {
		cout <<" Tag: "<<MESSAGE_TAGS[shortTag];
	}
	*/

	cout<<" RealTag: "<<getTag();
	cout<<" Count: "<<getCount();

	/*
	if(getCount() > 0){
		cout<<" Overlay: "<<getBuffer()[0];
	}
	*/

	cout << " Bytes: " << m_bytes;
	cout << " SourceActor: " << m_sourceActor;
	cout << " DestinationActor: " << m_destinationActor;
	cout << " RoutingSource: " << m_routingSource;
	cout << " RoutingDestination: " << m_routingDestination;
	cout << " MiniRankSource: " << m_miniRankSource;
	cout << " MiniRankDestination: " << m_miniRankDestination;

	printBuffer(getBufferBytes(), getNumberOfBytes());

	cout << endl;
}

void Message::printBuffer(const char * buffer, int bytes) const {

	cout << " Buffer: " << (void*) buffer;
	cout << " with " << bytes << " bytes : ";

	for(int i = 0 ; i < bytes ; ++i) {

		char byte = buffer[i];
		/*
		 * this does not work
		cout << " 0x" << hex << setfill('0') << setw(2);
		cout << byte;
		cout << dec;
		*/

		/*
		 * C solution
		 * \see http://stackoverflow.com/questions/8060170/printing-hexadecimal-characters-in-c
		 * \see http://stackoverflow.com/questions/10599068/how-do-i-print-bytes-as-hexadecimal
		 */
		printf(" 0x%02x", byte & 0xff);
		cout << dec;
	}
	cout << endl;
}

void Message::setBuffer(void *buffer){
	m_buffer = buffer;
}

void Message::setTag(MessageTag tag){
	m_tag=tag;
}

void Message::setCount(int count){
	//m_count=count;
	m_bytes = count * sizeof(MessageUnit);
}

void Message::setSource(Rank source){
	m_source=source;
}

void Message::setDestination(Rank destination){
	m_destination=destination;
}

bool Message::isActorModelMessage(int size) const {

	// source actor is actually a MPI rank !
	// this method is only important to the destination actor.
	if(false && m_sourceActor < size)
		return false;

	// destination actor is actually a MPI rank
	if(m_destinationActor < size)
		return false;

	// otherwise, it is an actor message !

	return true;
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

	// actor model metadata is the first to be saved.
	// So we don't have anything to remove from offset.

	// write rank numbers for these.
	// MPI ranks actually have actor names too !
	if(m_sourceActor < 0 && m_destinationActor < 0) {
		m_sourceActor = m_source;
		m_destinationActor = m_destination;
	}

#ifdef CONFIG_ASSERT
	int bytes = getNumberOfBytes();
	uint32_t checksumBefore = computeCyclicRedundancyCode32((uint8_t*)getBuffer(), bytes);

	if(m_sourceActor < 0) {
		cout << "Error m_sourceActor " << m_sourceActor;
		printActorMetaData();
	}
	assert(m_sourceActor >= 0 || m_sourceActor == ACTOR_MODEL_NOBODY);
	assert(m_destinationActor >= 0 || m_destinationActor == ACTOR_MODEL_NOBODY);

	// check that the overwrite worked.
	/*
	assert(m_sourceActor != ACTOR_MODEL_NOBODY);
	assert(m_destinationActor != ACTOR_MODEL_NOBODY);
	*/
#endif

	//cout << "DEBUG saveActorMetaData tag " << getTag() << endl;

	int offset = getNumberOfBytes();

	// actor metadata must be the first to be saved.
	offset -= 0;

	char * memory = (char*) getBuffer();

	memcpy(memory + offset + MESSAGE_META_DATA_ACTOR_SOURCE, &m_sourceActor, sizeof(int));
	memcpy(memory + offset + MESSAGE_META_DATA_ACTOR_DESTINATION, &m_destinationActor, sizeof(int));
	//printActorMetaData();

	//setCount(m_count + 1);
	//m_count += 1; // add 1 uint64_t

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
	cout << " bytes= " << getNumberOfBytes();

}

void Message::loadActorMetaData() {
/*
	cout << "DEBUG before loadActorMetaData -> ";
	printActorMetaData();
	cout << endl;
*/
	// the buffer contains the actor metadata. and routing metadata.
	int offset = getNumberOfBytes();
	//offset -= 2 * sizeof(int);
	//offset -= 2 * sizeof(int);

	char * memory = (char*) getBuffer();

	memcpy(&m_sourceActor, memory + offset + MESSAGE_META_DATA_ACTOR_SOURCE, sizeof(int));
	memcpy(&m_destinationActor, memory + offset + MESSAGE_META_DATA_ACTOR_DESTINATION, sizeof(int));


	/*
	cout << "DEBUG loadActorMetaData m_sourceActor= " << m_sourceActor;
	cout << " m_destinationActor= " << m_destinationActor << endl;
*/
	// remove 1 uint64_t

	//setNumberOfBytes(getNumberOfBytes() - 2 * sizeof(int));
	//setCount(m_count - 1);
	//m_count -= 1;
/*
	cout << "DEBUG after loadActorMetaData -> ";
	printActorMetaData();
	cout << endl;
	*/

#ifdef CONFIG_ASSERT
	if(!(m_sourceActor >= 0 || m_sourceActor == ACTOR_MODEL_NOBODY)) {
		cout << "Error: m_sourceActor " << m_sourceActor << endl;
		print();
	}

	assert(m_sourceActor >= 0 || m_sourceActor == ACTOR_MODEL_NOBODY);
	assert(m_destinationActor >= 0 || m_destinationActor == ACTOR_MODEL_NOBODY);
#endif

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

#ifdef CONFIG_ASSERT
	assert(m_routingSource >= 0 || m_routingSource == ROUTING_NO_VALUE);
	assert(m_routingDestination >= 0 || m_routingDestination == ROUTING_NO_VALUE);
#endif

	/*
	cout << "DEBUG saveRoutingMetaData m_routingSource " << m_routingSource;
	cout << " m_routingDestination " << m_routingDestination << endl;
*/
	int offset = getNumberOfBytes();
	char * memory = (char*) getBuffer();

	// the count already includes the actor addresses
	// this is stupid, but hey, nobody aside me is touching this code
	// with a ten-foot stick
	// 2013-11-18: this was fixed by grouping the  changes to count in saveMetaData().
	//offset -= 2 * sizeof(int);

	memcpy(memory + offset + MESSAGE_META_DATA_ROUTE_SOURCE, &m_routingSource, sizeof(int));
	memcpy(memory + offset + MESSAGE_META_DATA_ROUTE_DESTINATION, &m_routingDestination, sizeof(int));
	//printActorMetaData();

	//setNumberOfBytes(getNumberOfBytes() + 2 * sizeof(int));
	//m_count += 1; // add 1 uint64_t

	//cout << "DEBUG saved routing metadata at offset " << offset << " new count " << m_count << endl;
	//displayMetaData();

#ifdef CONFIG_ASSERT
	//assert(getNumberOfBytes() <= (int)(MAXIMUM_MESSAGE_SIZE_IN_BYTES + 4 * sizeof(int)));
#endif

}


int Message::getRoutingSource() const {

	return m_routingSource;
}

int Message::getRoutingDestination() const {

	return m_routingDestination;
}

void Message::loadRoutingMetaData() {
	//cout << "DEBUG loadRoutingMetaData count " << getNumberOfBytes() << endl;

	int offset = getNumberOfBytes();

	// m_count contains actor metadata *AND* routing metadata (if necessary)
	/*
	offset -= 2 * sizeof(int);
	offset -= 2 * sizeof(int);
	*/

	//cout << "Bytes before: " << m_bytes << endl;

	char * memory = (char*) getBuffer();

#ifdef CONFIG_ASSERT
	assert(memory != NULL);
#endif

	//cout << "DEBUG memory " << (void*)memory << " offset " << offset;
	//cout << " MESSAGE_META_DATA_ROUTE_SOURCE " << MESSAGE_META_DATA_ROUTE_SOURCE << endl;

	memcpy(&m_routingSource, memory + offset + MESSAGE_META_DATA_ROUTE_SOURCE, sizeof(int));
	memcpy(&m_routingDestination, memory + offset + MESSAGE_META_DATA_ROUTE_DESTINATION, sizeof(int));

	//setNumberOfBytes(getNumberOfBytes() - 2 * sizeof(int));
	//m_count -= 1;

#if 0
	cout << "DEBUG loadRoutingMetaData ";
	cout << "loaded m_routingSource ";
	cout << m_routingSource;
	cout << " m_routingDestination " << m_routingDestination;
	cout << " offset " << offset ;
	printActorMetaData();
	cout << endl;
#endif

	//displayMetaData();

	// these can not be negative, otherwise
	// this method would not have been called now.
#ifdef CONFIG_ASSERT
	if(!((m_routingSource >= 0 || m_routingSource == ROUTING_NO_VALUE))) {
		print();
	}

	if(!(m_routingDestination >= 0 || m_routingDestination == ROUTING_NO_VALUE)) {

		print();
	}

	assert(m_routingSource >= 0 || m_routingSource == ROUTING_NO_VALUE);
	assert(m_routingDestination >= 0 || m_routingDestination == ROUTING_NO_VALUE);
#endif
}

void Message::displayMetaData() {
	Message * aMessage = this;

	cout << " DEBUG displayMetaData count " << aMessage->getNumberOfBytes();
	cout << " tag " << getTag() << endl;

	int offset = getNumberOfBytes() - 2 * 2 * sizeof(int);

	for(int i = 0 ; i < 4 ; ++i) {
		cout << " [" << i << " -> " << ((int*)aMessage->getBuffer())[offset + i] << "]";
	}
	cout << endl;

}

void Message::setNumberOfBytes(int bytes) {

	m_bytes = bytes;
}

int Message::getNumberOfBytes() const {
	return m_bytes;
}

void Message::runAssertions(int size, bool routing, bool miniRanks) {

#ifdef CONFIG_ASSERT

	assert(m_source >= 0);
	assert(m_source < size);

	assert(m_destination >= 0);
	assert(m_destination < size);

	assert(m_sourceActor >= 0);
	assert(m_destinationActor >= 0);

	if(miniRanks) {
		if(!(m_miniRankSource >= 0)) {
			print();
		}

		assert(m_miniRankSource >= 0);

		if(!(m_miniRankSource < size )) {

			cout << "Error" << endl;
			print();
			cout << "m_miniRankSource " << m_miniRankSource;
			cout << " size " << size << endl;

		}
		assert(m_miniRankSource < size );


		assert(m_miniRankDestination >= 0);
		assert(m_miniRankDestination < size);


	} else {
		assert(m_miniRankSource == NO_VALUE);
		assert(m_miniRankDestination == NO_VALUE);
		assert(m_miniRankDestination == NO_VALUE);
	}

	if(routing) {
		assert(m_routingSource >= 0);
		assert(m_routingSource < size );

		assert(m_routingDestination >= 0 );
		assert(m_routingDestination < size);
	} else {

		if(!(m_routingSource == NO_VALUE)) {
			print();
		}
		assert( m_routingSource == ROUTING_NO_VALUE);
		assert( m_routingSource == NO_VALUE);

		assert( m_routingDestination == ROUTING_NO_VALUE);
		assert( m_routingDestination == NO_VALUE);
	}

#endif

}

void Message::saveMetaData() {

	//runAssertions(size);

#ifdef CONFIG_ASSERT

	int bytes = getNumberOfBytes();
	uint32_t checksumBefore = computeCyclicRedundancyCode32((uint8_t*)getBuffer(), bytes);
#endif

	this->saveMiniRankMetaData();
	this->saveActorMetaData();
	this->saveRoutingMetaData();

#ifdef CONFIG_ASSERT
	assert(getNumberOfBytes() >= 0);
	assert(getNumberOfBytes() <= MAXIMUM_MESSAGE_SIZE_IN_BYTES);

	uint32_t checksumAfter = computeCyclicRedundancyCode32((uint8_t*)getBuffer(), bytes);

	assert(checksumBefore == checksumAfter);
#endif

	// we need this for the transfer.
	setNumberOfBytes(getNumberOfBytes() + MESSAGE_META_DATA_SIZE);
}

void Message::loadMetaData() {

	// we just needed this for the transfer.
	setNumberOfBytes(getNumberOfBytes() - MESSAGE_META_DATA_SIZE);

	//cout << "loadMetaData MESSAGE_META_DATA_SIZE " << MESSAGE_META_DATA_SIZE << endl;

	//cout << "loadMetaData bytes: " << getNumberOfBytes() << endl;

#ifdef CONFIG_ASSERT
	assert(getNumberOfBytes() >= 0);
	if(!(getNumberOfBytes() <= MAXIMUM_MESSAGE_SIZE_IN_BYTES)) {
		cout << "Error: " << getNumberOfBytes() << endl;
	}
	assert(getNumberOfBytes() <= MAXIMUM_MESSAGE_SIZE_IN_BYTES);
#endif

	this->loadRoutingMetaData();
	this->loadActorMetaData();
	this->loadMiniRankMetaData();

}

void Message::saveMiniRankMetaData() {

	int sourceMiniRank = getSourceMiniRank();
	int destinationMiniRank = getDestinationMiniRank();

	memcpy(getBufferBytes() + getNumberOfBytes() + MESSAGE_META_DATA_MINIRANK_SOURCE,
			&sourceMiniRank, sizeof(sourceMiniRank));

	memcpy(getBufferBytes() + getNumberOfBytes() + MESSAGE_META_DATA_MINIRANK_DESTINATION,
			&destinationMiniRank, sizeof(destinationMiniRank));
}

void Message::loadMiniRankMetaData() {

	int sourceMiniRank = -1;
	int destinationMiniRank = -1;

	memcpy(
			&sourceMiniRank,
			getBufferBytes() + getNumberOfBytes() + MESSAGE_META_DATA_MINIRANK_SOURCE,
			sizeof(sourceMiniRank));

	memcpy(
			&destinationMiniRank,
			getBufferBytes() + getNumberOfBytes() + MESSAGE_META_DATA_MINIRANK_DESTINATION,
			sizeof(destinationMiniRank));

	setMiniRanks(sourceMiniRank, destinationMiniRank);
}

void Message::setMiniRanks(int source, int destination) {

	m_miniRankSource = source;
	m_miniRankDestination = destination;
}

int Message::getSourceMiniRank() {
	return m_miniRankSource;
}

int Message::getDestinationMiniRank() {
	return m_miniRankDestination;
}
