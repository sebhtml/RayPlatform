/*
	RayPlatform: a message-passing development framework
    Copyright (C) 2010, 2011, 2012, 2013 Sébastien Boisvert

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


#include "KeyValueStore.h"

#include <RayPlatform/core/ComputeCore.h>

__CreatePlugin(KeyValueStore);

__CreateMessageTagAdapter(KeyValueStore, RAYPLATFORM_MESSAGE_TAG_DOWNLOAD_OBJECT_PART);
__CreateMessageTagAdapter(KeyValueStore, RAYPLATFORM_MESSAGE_TAG_DOWNLOAD_OBJECT_PART_REPLY);

KeyValueStore::KeyValueStore() {

	clear();
}

void KeyValueStore::initialize(Rank rank, int size, RingAllocator * outboxAllocator, StaticVector * inbox, StaticVector * outbox) {

	m_rank = rank;
	m_size = size;
	m_outboxAllocator = outboxAllocator;
	m_inbox = inbox;
	m_outbox = outbox;

	int chunkSize = 33554432; // 32 MiB    4194304 is 4 MiB
	m_memoryAllocator.constructor(chunkSize, "/allocator/KeyValueStore.ram", false);
}

bool KeyValueStore::getKey(const char * key, int keyLength, string & keyObject) {

	#define MAXIMUM_KEY_LENGTH 256
	char buffer[MAXIMUM_KEY_LENGTH];

#ifdef CONFIG_ASSERT
	assert(keyLength + 1 <= MAXIMUM_KEY_LENGTH);
#endif

	if(keyLength + 1 > MAXIMUM_KEY_LENGTH)
		return false;

	memcpy(buffer, key, keyLength);
	buffer[keyLength] = '\0';

	keyObject = buffer;

	return true;
}

/**
 * TODO: remove limit on key length
 */
bool KeyValueStore::insert(const char * key, int keyLength, char * value, int valueLength) {

	string keyObject;

	if(!getKey(key, keyLength, keyObject))
		return false;

	if(m_items.count(keyObject) != 0)
		return false;

	KeyValueStoreItem item(value, valueLength);
	m_items[keyObject] = item;

	cout << "[DEBUG] KeyValueStore registered key " << keyObject << " (length: " << keyLength;
	cout << ")";
	cout << " with value (length: " << valueLength << ")";
	cout << endl;

	return true;
}

bool KeyValueStore::remove(const char * key, int keyLength) {

	string keyObject;
	if(!getKey(key, keyLength, keyObject))
		return false;

	// nothing to remove
	if(m_items.count(keyObject) == 0)
		return true;

	int size = m_items[keyObject].getValueLength();
	char * content = m_items[keyObject].getValue();

	m_memoryAllocator.free(content, size);

	m_items.erase(keyObject);

#ifdef CONFIG_ASSERT
	assert(m_items.count(keyObject) == 0);
#endif ////// CONFIG_ASSERT

	return true;
}

bool KeyValueStore::get(const char * key, int keyLength, char ** value, int * valueLength) {

	string keyObject;

	if(!getKey(key, keyLength, keyObject))
		return false;

	// nothing to remove
	if(m_items.count(keyObject) == 0)
		return false;

	KeyValueStoreItem & item = m_items[keyObject];

	(*value) = item.getValue();
	(*valueLength) = item.getValueLength();

	return true;
}

bool KeyValueStore::push(const char * key, int keyLength, Rank destination) {
	return false;
}

/**
 * This method sends RAYPLATFORM_MESSAGE_TAG_DOWNLOAD_OBJECT_PART to another rank.
 * The chunk is received in a message RAYPLATFORM_MESSAGE_TAG_DOWNLOAD_OBJECT_PART_REPLY.
 *
 * If the new offset is lower than the value length, then the transfer is not complete.
 */
bool KeyValueStore::pull(const char * key, int keyLength, Rank source) {

	// the transfer has completed.
	if(m_valueSize != 0 && m_valueSize == m_offset) {

		// reset the states for the next one.

		m_valueSize = 0;
		m_offset = 0;
		m_messageWasSent = false;

		return true;

	} else if(!m_messageWasSent) {

		char * buffer=(char *) m_outboxAllocator->allocate(MAXIMUM_MESSAGE_SIZE_IN_BYTES);

		cout << "[DEBUG] KeyValueStore::pull key= " << key << " m_valueSize= " << m_valueSize;
		cout << " m_offset= " << m_offset;
		cout << endl;

		int outputPosition = dumpMessageHeader(key, m_valueSize, m_offset, buffer);

		int units = outputPosition / sizeof(MessageUnit);
		if(outputPosition % sizeof(MessageUnit))
			units ++;

		Message aMessage((MessageUnit*)buffer, units, source,
			                       RAYPLATFORM_MESSAGE_TAG_DOWNLOAD_OBJECT_PART, m_rank);

		m_outbox->push_back(&aMessage);


		m_messageWasSent = true;

		return false;
	}

	return false;
}

void KeyValueStore::clear() {
	m_rank = 0;
	m_size = 0;
	m_outboxAllocator = NULL;
	m_inbox = NULL;
	m_outbox = NULL;

	m_messageWasSent = false;
	m_valueSize = 0;
	m_offset = 0;

	m_items.clear();

	// free used memory, but don't give it back to the
	// operatign system
	m_memoryAllocator.reset();
}

void KeyValueStore::destroy() {
	clear();

	// give back the memory to the operating system
	m_memoryAllocator.clear();
}

char * KeyValueStore::allocateMemory(int bytes) {
	char * address = (char*)m_memoryAllocator.allocate(bytes);

	return address;
}

int KeyValueStore::dumpMessageHeader(const char* key, int valueLength, int offset, char * buffer) const {

	int outputPosition = 0;

	int size = strlen(key);

	uint32_t realValueLength = valueLength;
	uint32_t realOffset = offset;

	memcpy(buffer + outputPosition, key, size);
	outputPosition += size;
	buffer[outputPosition++] = '\0';

	size = sizeof(uint32_t);

	memcpy(buffer + outputPosition, &realValueLength, size);
	outputPosition += size;
	memcpy(buffer + outputPosition, &realOffset, size);
	outputPosition += size;

	return outputPosition;
}

/*
 * The header contains:
 *
 * * the key, strlen(buffer) is the length of this key
 * * the length of the value (0 if the information is not known by the sender) uint32_t
 * * the offset (uint32_t) at buffer + length of key
 */
void KeyValueStore::call_RAYPLATFORM_MESSAGE_TAG_DOWNLOAD_OBJECT_PART(Message * message) {

	char * buffer = (char*)message->getBuffer();
	int position = 0;

	// this will stop at the first '\0'
	string key = buffer + position;

	int size = key.length();
	position += size;

	uint32_t valueLength = 0;
	size = sizeof(uint32_t);
	memcpy(&valueLength, buffer + position, size);
	position += size;

	uint32_t offset = 0;
	memcpy(&offset, buffer + position, size);
	position += size;

#ifdef CONFIG_ASSERT
	assert(buffer != NULL);
#endif /////// CONFIG_ASSERT

	char * responseBuffer = (char *) m_outboxAllocator->allocate(MAXIMUM_MESSAGE_SIZE_IN_BYTES);


	char * value = NULL;
	int actualValueLength = 0;

	// if the key is inside, we respond with
	if(m_items.count(key) > 0) {

		get(key.c_str(), key.length(), &value, &actualValueLength);
	}

	// we need a header even when the key is not found
	// in that case, we assign a value length of 0 in the response.
	//
	int outputPosition = dumpMessageHeader(key.c_str(), actualValueLength, offset, responseBuffer);

	if(value != NULL) {

		int bytesToCopy = actualValueLength = offset;

		int remainingBytes = MAXIMUM_MESSAGE_SIZE_IN_BYTES - outputPosition;

		if(remainingBytes < bytesToCopy)
			bytesToCopy = remainingBytes;

		cout << "[DEBUG] KeyValueStore::call_RAYPLATFORM_MESSAGE_TAG_DOWNLOAD_OBJECT_PART key= " << key << " offset= " << offset;
		cout << " actualValueLength= " << actualValueLength;
		cout << " bytesToCopy= " << bytesToCopy;
		cout << endl;

		size = remainingBytes;
		memcpy(responseBuffer + outputPosition, value, size);
		outputPosition += size;
	}

#ifdef CONFIG_ASSERT
	assert(outputPosition <= MAXIMUM_MESSAGE_SIZE_IN_BYTES);
#endif

	int units = outputPosition / sizeof(MessageUnit);
	if(outputPosition % sizeof(MessageUnit))
		units ++;

	Message aMessage((MessageUnit*) buffer, units, message->getSource(),
			                       RAYPLATFORM_MESSAGE_TAG_DOWNLOAD_OBJECT_PART_REPLY, message->getDestination());

	// and hop we go, the message is now on the message bus
	// let's hope that the message reaches its destination safely.

	m_outbox->push_back(&aMessage);
}

/*
 * The header contains:
 *
 * * the key, strlen(buffer) is the length of this key
 * * the length of the value (0 if the information is not known by the sender) uint32_t
 * * the offset (uint32_t) at buffer + length of key
 *
 * The rest of the buffer contains bytes.
 *
 */
void KeyValueStore::call_RAYPLATFORM_MESSAGE_TAG_DOWNLOAD_OBJECT_PART_REPLY(Message * message) {

	char * buffer = (char*)message->getBuffer();

	string key = buffer;

	int inputPosition = 0;
	int size = key.length();
	inputPosition += size;

	uint32_t valueSize = 0;
	uint32_t offset = 0;

	size = sizeof(uint32_t);

	memcpy(&valueSize, buffer + inputPosition, size);
	inputPosition += size;

	memcpy(&offset, buffer + inputPosition, size);
	inputPosition += size;

	// the system does not support values with 0 bytes
	// 0 bytes mean that the key does not exist.
	if(valueSize == 0)
		return;

	// if the offset is 0
	if(offset == 0) {
		char * value = allocateMemory(valueSize);
		insert(key.c_str(), key.length(), value, valueSize);

		m_offset = 0;
		m_valueSize = valueSize;
	}

	char * value = NULL;
	int dummySize = 0;

	// get the local buffer copy
	get(key.c_str(), key.length(), &value, &dummySize);

	int bytesToCopy = valueSize - offset;

	int remainingBytes = MAXIMUM_MESSAGE_SIZE_IN_BYTES - inputPosition;

	if(remainingBytes < bytesToCopy)
		bytesToCopy = remainingBytes;

	// copy into the buffer what we received from
	// a remote rank
	memcpy(value, buffer + inputPosition, bytesToCopy);

	uint32_t newOffset = offset + bytesToCopy;

	// update data for next message
	m_offset = newOffset;
	m_messageWasSent = false;
}

void KeyValueStore::registerPlugin(ComputeCore * core){

	m_core=core;
	m_plugin=core->allocatePluginHandle();

	core->setPluginName(m_plugin, "KeyValueStore");
	core->setPluginDescription(m_plugin, "A key-value store for transporting items between processes");
	core->setPluginAuthors(m_plugin,"Sébastien Boisvert");
	core->setPluginLicense(m_plugin,"GNU Lesser General License version 3");

	__ConfigureMessageTagHandler(KeyValueStore, RAYPLATFORM_MESSAGE_TAG_DOWNLOAD_OBJECT_PART);
	__ConfigureMessageTagHandler(KeyValueStore, RAYPLATFORM_MESSAGE_TAG_DOWNLOAD_OBJECT_PART_REPLY);
}

void KeyValueStore::resolveSymbols(ComputeCore*core){

}
