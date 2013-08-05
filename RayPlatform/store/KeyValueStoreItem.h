/*
 * Author: Sébastien Boisvert
 * Project: RayPlatform
 * Licence: LGPL3
 */



#ifndef KeyValueStoreItem_Header
#define KeyValueStoreItem_Header

class KeyValueStoreItem {

	char * m_value;
	int m_size;


public:

	KeyValueStoreItem();

	KeyValueStoreItem(char * value, int valueLength);
	char * getValue();
	int getValueLength();
};

#endif // KeyValueStoreItem_Header
