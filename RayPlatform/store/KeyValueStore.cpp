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
#include <RayPlatform/cryptography/crypto.h>

#include <string.h> /* for memcpy */

//#define KeyValueStore_DEBUG_NOW "Yes"

__CreatePlugin(KeyValueStore);

__CreateMessageTagAdapter(KeyValueStore, RAYPLATFORM_MESSAGE_TAG_DOWNLOAD_OBJECT_PART);
__CreateMessageTagAdapter(KeyValueStore, RAYPLATFORM_MESSAGE_TAG_DOWNLOAD_OBJECT_PART_REPLY);

KeyValueStore::KeyValueStore() {

	clear();
}


void KeyValueStore::pullRemoteKey(const string & key, const Rank & rank, KeyValueStoreRequest & request) {

#ifdef KeyValueStore_DEBUG_NOW
	cout << "[DEBUG] pullRemoteKey Request: key= " << key << " rank= " << rank << endl;
#endif

	request.initialize(key, rank);
	request.setTypeToPullRequest();

}

void KeyValueStore::initialize(Rank rank, int size, RingAllocator * outboxAllocator, StaticVector * inbox, StaticVector * outbox) {

	m_rank = rank;
	m_size = size;
	m_outboxAllocator = outboxAllocator;
	m_inbox = inbox;
	m_outbox = outbox;

	int chunkSize = 33554432; // 32 MiB    4194304 is 4 MiB
	m_memoryAllocator.constructor(chunkSize, "/allocator/KeyValueStore.DRAM", false);
}

bool KeyValueStore::getStringKey(const char * key, int keyLength, string & keyObject) {

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

bool KeyValueStore::insertLocalKey(const string & key, char * value, int valueLength) {

	return insertLocalKeyWithLength(key.c_str(), key.length(), value, valueLength);
}

/**
 * TODO: remove limit on key length
 */
bool KeyValueStore::insertLocalKeyWithLength(const char * key, int keyLength, char * value, int valueLength) {

	string keyObject = "";

	if(!getStringKey(key, keyLength, keyObject))
		return false;

	if(m_items.count(keyObject) != 0)
		return false;

	KeyValueStoreItem item(value, valueLength);
	m_items[keyObject] = item;

#ifdef KeyValueStore_DEBUG_NOW
	cout << "[DEBUG] KeyValueStore registered key " << keyObject << " (length: " << keyLength;
	cout << ")";
	cout << " with value (length: " << valueLength << ")";
	cout << endl;
#endif /* KeyValueStore_DEBUG_NOW */

	return true;
}

bool KeyValueStore::removeLocalKey(const string & key) {

	string keyObject = key;

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

bool KeyValueStore::getLocalKey(const string & key, char * & value, int & valueLength) {

	return getLocalKeyWithLength(key.c_str(), key.length(), &value, &valueLength);
}

bool KeyValueStore::getLocalKeyWithLength(const char * key, int keyLength, char ** value, int * valueLength) {

	string keyObject;
	if(!getStringKey(key, keyLength, keyObject))
		return false;

	// nothing to remove
	if(m_items.count(keyObject) == 0)
		return false;

	KeyValueStoreItem & item = m_items[keyObject];


	if(!item.isItemReady())
		return false;

	(*value) = item.getValue();
	(*valueLength) = item.getValueLength();

	return true;
}

bool KeyValueStore::pushLocalKeyWithLength(const char * key, int keyLength, Rank destination) {

	// not implemented.

	return false;
}

KeyValueStoreItem * KeyValueStore::getLocalItemFromKey(const char * key, int keyLength) {

	string keyObject;
	if(!getStringKey(key, keyLength, keyObject))
		return NULL;

	if(m_items.count(keyObject) == 0)
		return NULL;

	return &(m_items[keyObject]);
}

/**
 * This method sends RAYPLATFORM_MESSAGE_TAG_DOWNLOAD_OBJECT_PART to another rank.
 * The chunk is received in a message RAYPLATFORM_MESSAGE_TAG_DOWNLOAD_OBJECT_PART_REPLY.
 *
 * If the new offset is lower than the value length, then the transfer is not complete.
 */
bool KeyValueStore::pullRemoteKeyWithLength(const char * key, int keyLength, Rank source) {

	KeyValueStoreItem * item = getLocalItemFromKey(key, keyLength);

	if(item == NULL) {

		// insert an item with an empty value
		insertLocalKeyWithLength(key, keyLength, NULL, 0);

		item = getLocalItemFromKey(key, keyLength);

		// this will mark the key as not ready.
		item->startDownload();

#ifdef KeyValueStore_DEBUG_NOW
		cout << "[DEBUG] KeyValueStore marked key " << key << " for download." << endl;
#endif

		return false;
	}

	// the transfer has completed.
	if(item != NULL && item->isItemReady()){

#if 0
		cout << "[DEBUG] item is ready to be used." << endl;
#endif

		return true;

	} else if(item != NULL && !item->messageWasSent()) {

		char * buffer=(char *) m_outboxAllocator->allocate(MAXIMUM_MESSAGE_SIZE_IN_BYTES);

		uint32_t valueSize = item->getValueLength();
		uint32_t offset = item->getOffset();

#ifdef KeyValueStore_DEBUG_NOW
		cout << "[DEBUG] KeyValueStore::pullRemoteKeyWithLength key= " << key << " valueSize= " << valueSize;
		cout << " offset= " << offset;
		cout << endl;
#endif /* KeyValueStore_DEBUG_NOW */

		int outputPosition = 0;

		outputPosition += dumpMessageHeader(key, valueSize, offset, buffer);

		int units = outputPosition / sizeof(MessageUnit);
		if(outputPosition % sizeof(MessageUnit))
			units ++;

		Message aMessage((MessageUnit*)buffer, units, source,
			                       RAYPLATFORM_MESSAGE_TAG_DOWNLOAD_OBJECT_PART, m_rank);

		m_outbox->push_back(&aMessage);

		// this will mark the item as being being currently fetched.
		item->sendMessage();

#ifdef KeyValueStore_DEBUG_NOW
		cout << "[DEBUG] message RAYPLATFORM_MESSAGE_TAG_DOWNLOAD_OBJECT_PART sent to " << source;
		cout << endl;
#endif
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

int KeyValueStore::loadMessageHeader(string & key, uint32_t & valueSize, uint32_t & offset, const char * buffer) const {

	// read the message header
	key = buffer;

	int inputPosition = 0;
	int size = key.length();
	inputPosition += size;
	inputPosition ++; // for the \0

	size = sizeof(uint32_t);
	memcpy(&valueSize, buffer + inputPosition, size);
	inputPosition += size;

	memcpy(&offset, buffer + inputPosition, size);
	inputPosition += size;

#ifdef KeyValueStore_DEBUG_NOW
	cout << "[DEBUG] loadMessageHeader key= " << key << " valueSize= " << valueSize;
	cout << " offset= " << offset << " headerSize= " << inputPosition << endl;
#endif

	return inputPosition;
}

int KeyValueStore::dumpMessageHeader(const char* key, uint32_t valueLength, uint32_t offset, char * buffer) const {

	int outputPosition = 0;

	int size = strlen(key);

	memcpy(buffer + outputPosition, key, size);
	outputPosition += size;
	buffer[outputPosition++] = '\0';

	size = sizeof(uint32_t);

	memcpy(buffer + outputPosition, &valueLength, size);
	outputPosition += size;
	memcpy(buffer + outputPosition, &offset, size);
	outputPosition += size;

#ifdef KeyValueStore_DEBUG_NOW
	cout << "[DEBUG] dumpMessageHeader key= " << key << " valueLength= " << valueLength;
	cout << " offset= " << offset << " HeaderSize= " << outputPosition;
	cout << endl;
#endif



	return outputPosition;
}

/*
 * The header contains:
 *
 * * the key, strlen(buffer) is the length of this key
 * * the length of the value (0 if the information is not known by the sender) uint32_t
 * * the offset (uint32_t) at buffer + length of key
 *
 * TODO: we need something to indicate that the object does not exist
 * here. add this in the header...
 */
void KeyValueStore::call_RAYPLATFORM_MESSAGE_TAG_DOWNLOAD_OBJECT_PART(Message * message) {

#ifdef KeyValueStore_DEBUG_NOW
	cout << "***" << endl;
	cout << "call_RAYPLATFORM_MESSAGE_TAG_DOWNLOAD_OBJECT_PART" << endl;
#endif

	char * buffer = (char*)message->getBuffer();

#ifdef CONFIG_ASSERT
	assert(buffer != NULL);
#endif /////// CONFIG_ASSERT

	// load the message header
	string key = "";
	uint32_t valueLength = 0;
	uint32_t offset = 0;

	int position = 0;
	position += loadMessageHeader(key, valueLength, offset, buffer + position);

	char * responseBuffer = (char *) m_outboxAllocator->allocate(MAXIMUM_MESSAGE_SIZE_IN_BYTES);

	char * value = NULL;
	int actualValueLength = 0;

	// if the key is inside, we respond with
	if(m_items.count(key) > 0) {

		KeyValueStoreItem * item = getLocalItemFromKey(key.c_str(), key.length());
		value = item->getValue();
		actualValueLength = item->getValueLength();
	}

	// TODO: if object does not exist, a message will be returned regardless
	// with size 0, which should not be the case.


	// we need a header even when the key is not found
	// in that case, we assign a value length of 0 in the response.
	//
	int outputPosition = 0;

#ifdef KeyValueStore_DEBUG_NOW
	cout << "[DEBUG] actualValueLength= " << actualValueLength << endl;
#endif

	outputPosition += dumpMessageHeader(key.c_str(), actualValueLength, offset, responseBuffer);

	if(value != NULL) {

		int bytesToCopy = actualValueLength - offset;

		int remainingBytes = MAXIMUM_MESSAGE_SIZE_IN_BYTES - outputPosition;

		if(remainingBytes < bytesToCopy)
			bytesToCopy = remainingBytes;

#ifdef KeyValueStore_DEBUG_NOW
		cout << "[DEBUG] KeyValueStore::call_RAYPLATFORM_MESSAGE_TAG_DOWNLOAD_OBJECT_PART key= " << key << " offset= " << offset;
		cout << " actualValueLength= " << actualValueLength;
		cout << " bytesToCopy= " << bytesToCopy;
		cout << " source= " << message->getSource();
		cout << endl;
#endif /* KeyValueStore_DEBUG_NOW */

		int size = bytesToCopy;
		memcpy(responseBuffer + outputPosition, value + offset, size);
		outputPosition += size;
	}

#ifdef CONFIG_ASSERT
	assert(outputPosition <= MAXIMUM_MESSAGE_SIZE_IN_BYTES);
#endif

	int units = outputPosition / sizeof(MessageUnit);
	if(outputPosition % sizeof(MessageUnit))
		units ++;

	Message aMessage((MessageUnit*) responseBuffer, units, message->getSource(),
			                       RAYPLATFORM_MESSAGE_TAG_DOWNLOAD_OBJECT_PART_REPLY, message->getDestination());

#ifdef KeyValueStore_DEBUG_NOW
	cout << "DEBUG reply RAYPLATFORM_MESSAGE_TAG_DOWNLOAD_OBJECT_PART_REPLY " << units << " units ";
	cout << outputPosition << " bytes" << endl;
#endif

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

#ifdef KeyValueStore_DEBUG_NOW
	cout << "***" << endl;
	cout << "[DEBUG] received call_RAYPLATFORM_MESSAGE_TAG_DOWNLOAD_OBJECT_PART_REPLY" << endl;
	cout << " units " << message->getCount() << endl;
#endif

	char * buffer = (char*)message->getBuffer();

#ifdef CONFIG_ASSERT
	assert(buffer != NULL);
#endif /////// CONFIG_ASSERT


	string key = "";
	uint32_t valueLength = 0;
	uint32_t offset = 0;

	int inputPosition = 0;

	inputPosition += loadMessageHeader(key, valueLength, offset, buffer);

	KeyValueStoreItem * item = getLocalItemFromKey(key.c_str(), key.length());

#ifdef CONFIG_ASSERT
	assert(item != NULL);
#endif

	// if the offset is 0
	if(offset == 0) {

#ifdef CONFIG_ASSERT
		assert(item->getValue() == NULL);
		assert(item->getValueLength() == 0);
#endif

		if(valueLength != 0) {

			char * value = allocateMemory(valueLength);
			item->setValue(value);
			item->setValueLength(valueLength);
		}

		// it is 0 bytes, the meta-data of the object
		// are already registered correctly
		//
		// This was done when the first message of type
		// RAYPLATFORM_MESSAGE_TAG_DOWNLOAD_OBJECT_PART
		// was sent.
	}

	// TODO check if the object is invalid (does not exist on remote host)

	// if we have 0 bytes to copy, we have nothing left to do.
	if(item->getValueLength() == 0) {
		item->receiveMessage();
		return;
	}

	// get the local buffer copy

	char * value = item->getValue();

#ifdef CONFIG_ASSERT
	int dummySize = item->getValueLength();

	assert(value != NULL);
	assert(dummySize == valueLength);

#ifdef KeyValueStore_DEBUG_NOW
	cout << "DEBUG dummySize= " << dummySize << endl;
#endif
#endif

	int bytesToCopy = valueLength - offset;

	int remainingBytes = MAXIMUM_MESSAGE_SIZE_IN_BYTES - inputPosition;

	if(remainingBytes < bytesToCopy)
		bytesToCopy = remainingBytes;

#ifdef KeyValueStore_DEBUG_NOW
	cout << "[DEBUG] memcpy destination= " << offset;
	cout << " inputPosition= " << inputPosition << " bytesToCopy= " << bytesToCopy << endl;
#endif

	// copy into the buffer what we received from
	// a remote rank
	memcpy(value + offset, buffer + inputPosition, bytesToCopy);

#ifdef CONFIG_ASSERT
	assert(offset + bytesToCopy <= item->getValueLength());
#endif

	uint32_t newOffset = offset + bytesToCopy;

	item->setDownloadedSize(newOffset);

#ifdef KeyValueStore_DEBUG_NOW
	cout << "[DEBUG] setDownloadedSize " << newOffset << endl;
#endif

	// update data for next message
	item->receiveMessage();

	// the progression will occur with a call to pullRemoteKey by the end-user
	// code.
}

bool KeyValueStore::test(KeyValueStoreRequest & request) {

	const string & key = request.getKey();
	const Rank & rank = request.getRank();

#if 0
	cout << "[DEBUG] test Request: key= " << key << " rank= " << rank << endl;
#endif

	if(request.isAPullRequest()) {

		if(pullRemoteKeyWithLength(key.c_str(), key.length(), rank)) {

			return true;
		}

	}

	return false;
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
