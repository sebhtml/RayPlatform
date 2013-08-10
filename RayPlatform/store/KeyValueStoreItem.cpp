/*
 * Author: Sébastien Boisvert
 * Project: RayPlatform
 * Licence: LGPL3
 */

#include "KeyValueStoreItem.h"

KeyValueStoreItem::KeyValueStoreItem() {

}

KeyValueStoreItem::KeyValueStoreItem(char * value, int valueSize) {
	m_value = value;
	m_valueSize = valueSize;
	m_downloadedSize = valueSize;

	m_ready = true;
}

char * KeyValueStoreItem::getValue() {
	return m_value;
}

int KeyValueStoreItem::getValueLength() const {
	return m_valueSize;
}

void KeyValueStoreItem::markItemAsReady() {

#ifdef CONFIG_ASSERT
	assert(m_downloadedSize == m_valueSize);
	assert( (m_value == NULL && m_valueSize == 0 && m_downloadedSize == 0) || (m_value != NULL && m_valueSize != 0));
#endif

	m_ready = true;
}

bool KeyValueStoreItem::isItemReady() const {
	return m_ready == true;
}

void KeyValueStoreItem::setDownloadedSize(int downloadedSize) {

#ifdef CONFIG_ASSERT
	assert(downloadedSize <= m_valueSize);
#endif

	m_downloadedSize = m_valueSize;
}

void KeyValueStoreItem::sendMessage() {

#ifdef CONFIG_ASSERT
	assert(m_messageWasSent == false);
#endif

	m_messageWasSent = true;
	m_ready = false;
}

void KeyValueStoreItem::receiveMessage() {

#ifdef CONFIG_ASSERT
	assert(m_messageWasSent == true);
#endif

	m_messageWasSent = false;

	if(m_downloadedSize == m_valueSize)
		markItemAsReady();
}

bool KeyValueStoreItem::messageWasSent() const {
	return m_messageWasSent;
}

void KeyValueStoreItem::setValue(char * value) {
	m_value = value;
}

void KeyValueStoreItem::setValueLength(int valueSize) {
	m_valueSize = valueSize;
}

void KeyValueStoreItem::startDownload() {

	m_ready = false;
	m_messageWasSent = false;
}
