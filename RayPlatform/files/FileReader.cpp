/*
    Copyright 2013 Sébastien Boisvert
    Copyright 2013 Université Laval
    Copyright 2013 Centre Hospitalier Universitaire de Québec

    This file is part of RayPlatform.

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

#include "FileReader.h"

#include <iostream>
using namespace std;

#include <stdio.h>
#include <stdlib.h>
#include <string.h>


FileReader::FileReader() {

	//m_bufferSize = 2097152;
	m_bufferSize = 524288;

	m_buffer = NULL;
	m_startInBuffer = 0;
	m_availableBytes = 0;

	m_active = false;
}

FileReader::~FileReader() {
	if(m_active == true) {
		close();
	}
}

void FileReader::open(const char * file) {

	if(m_active == true)
		return;

	m_reader.open(file);

#if 0
	cout << "DEBUG open " << file << endl;
#endif

	m_active = true;
}

int FileReader::getAvailableBytes() {

	return m_availableBytes;
}

char * FileReader::getAvailableBuffer() {
	return m_buffer + m_startInBuffer;
}

void FileReader::getline(char * buffer, int size) {

#if 0
	cout << "DEBUG getline" << endl;
#endif

	if(m_active == false)
		return;

	if(size > getAvailableBytes())
		this->read();

	char * availableBuffer = getAvailableBuffer();

	int availableBytes = getAvailableBytes();

	int i = 0;
	while(i < availableBytes && i < size
			&& availableBuffer[i] != '\n') {

		i++;
	}

	if(availableBuffer[i] == '\n')
		i++;

	memcpy(buffer, availableBuffer, i);

	buffer[i] = '\0';

#if 0
	cout << "DEBUG m_startInBuffer " << m_startInBuffer;
	cout << " m_availableBytes " << m_availableBytes;
	cout << endl;
#endif

#if 0
	cout << "DEBUG getline result: " << i << " bytes <result>" << buffer << "</result>" << endl;
#endif

	m_startInBuffer += i;
	m_availableBytes -= i;

	//m_reader.getline(buffer, size);
}

void FileReader::read() {

	if(m_buffer == NULL) {
		m_buffer = (char * ) malloc(m_bufferSize);

	}

	int source = m_startInBuffer;
	int destination = 0;

	while(source < m_bufferSize) {

		m_buffer[destination++] = m_buffer[source++];
	}

	m_startInBuffer = 0;

	int bytes = m_bufferSize - getAvailableBytes();

	char * destinationBuffer = m_buffer + getAvailableBytes();

#if 0
	cout << "DEBUG read " << bytes << " bytes" << endl;
#endif

	m_reader.read(destinationBuffer, bytes);

	int bytesRead = m_reader.gcount();

	m_availableBytes += bytesRead;

	//cout << "DEBUG got " << bytesRead << " bytes" << endl;
}

void FileReader::close() {

	if(!m_active)
		return;

	m_reader.close();

	if(m_buffer != NULL) {
		free(m_buffer);
		m_buffer = NULL;
	}

	m_active = false;
}

bool FileReader::eof() {

	if(!m_active)
		return true;

	if(m_availableBytes == 0 && m_reader.eof())
		return true;

	return false;

	//return m_reader.eof();
}

bool FileReader::isValid() {

	return m_reader.good();
}
