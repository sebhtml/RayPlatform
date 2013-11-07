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

#ifndef FileReaderHeader
#define FileReaderHeader

#include <string>
#include <fstream>
using namespace std;

/**
 * This is a wrapper around ifstream.
 * However, the supported operations are currently limited to
 * reading lines.
 *
 * \author Sébastien Boisvert
 */
class FileReader {

private:
	int m_startInBuffer;
	int m_availableBytes;

	int m_bufferSize;
	char * m_buffer;
	bool m_active;
	ifstream m_reader;

	int getAvailableBytes();
	char * getAvailableBuffer();
	void read();
public:

	FileReader();
	~FileReader();

	void open(const char * file);
	void getline(char * buffer, int size);
	void close();
	bool eof();
	bool isValid();
};

#endif
