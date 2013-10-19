/*
    Copyright 2013 Sébastien Boisvert
    Copyright 2013 Université Laval
    Copyright 2013 Centre Hospitalier Universitaire de Québec

    This file is part of the actor model implementation in RayPlatform.

    RayPlatform is free software: you can redistribute it and/or modify
    it under the terms of the GNU Lesser General Public License as published by
    the Free Software Foundation, version 3 of the License.

    RayPlatform is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public License
    along with RayPlatform.  If not, see <http://www.gnu.org/licenses/>.
*/

// \author Sébastien Boisvert
// License: GPLv3
// Université Laval 2013-10-10

#include "Actor.h"

#include <RayPlatform/communication/Message.h>
#include <RayPlatform/core/ComputeCore.h>
#include <RayPlatform/cryptography/crypto.h>

#include <iostream>
using namespace std;

Actor::Actor() {
}

Actor::~Actor() {
}

void Actor::send(int destination, Message * message) {

	message->setSourceActor(getName());
	message->setDestinationActor(destination);

	// if the buffer is not NULL, allocate a RayPlatform
	// buffer and copy stuff in it.
	if(message->getBuffer() != NULL) {

		// this call return MAXIMUM_MESSAGE_SIZE_IN_BYTES + padding
		char * newBuffer = (char*)m_core->getOutboxAllocator()->allocate(42);
		int bytes = message->getNumberOfBytes();
		char * oldBuffer = (char*)message->getBufferBytes();

		memcpy(newBuffer, oldBuffer, bytes * sizeof(char));

		message->setBuffer(newBuffer);
	}

#if 0
	cout << "DEBUG Actor::send CRC32: ";
	cout << computeCyclicRedundancyCode32((uint8_t*) message->getBufferBytes(),
			message->getNumberOfBytes());
	cout << endl;
#endif

	// deleguate the message to ComputeCore..

	m_core->sendActorMessage(message);
}

void Actor::send(int destination, Message & message) {

	send(destination, &message);
}

void Actor::spawn(Actor * actor) {
	
	m_core->spawnActor(actor);
}

void Actor::configureStuff(int name, Playground * kernel) {

	m_name = name;
	m_core = kernel;

	m_dead = false;
}

void Actor::die() {
	m_dead = true;
}

bool Actor::isDead() const {

	return m_dead;
}

int Actor::getName() const {
	return m_name;
}

void Actor::printName() const {

	cout << "/actors/" << getName();
	cout << " -> ";
}

int Actor::getRank() const {
	return m_core->getRank();
}

int Actor::getSize() const {
	return m_core->getSize();
}
