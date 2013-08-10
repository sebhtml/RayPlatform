/*
 * Author: Sébastien Boisvert
 * Project: RayPlatform
 * Licence: LGPL3
 */

#ifndef KeyValueStoreItem_Header
#define KeyValueStoreItem_Header

/**
 * A key-value entry in the distributed store.
 *
 * \author Sébastien Boisvert
 */
class KeyValueStoreItem {

	char * m_value;
	int m_valueSize;
	int m_downloadedSize;
	bool m_messageWasSent;
	bool m_ready;

	void markItemAsReady();

public:

	KeyValueStoreItem();

	KeyValueStoreItem(char * value, int valueLength);
	char * getValue();
	int getValueLength() const;

	bool isItemReady() const;

	void sendMessage();
	void receiveMessage();

	bool messageWasSent() const;

	void setValue(char * value);
	void setValueLength(int size);

	void setDownloadedSize(int downloadedSize);

	void startDownload();
};

#endif // KeyValueStoreItem_Header
