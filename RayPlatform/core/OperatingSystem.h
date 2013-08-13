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

#ifndef _OperatingSystem_h
#define _OperatingSystem_h

/* EXIT_SUCCESS 0 (defined in stdlib.h) */
#define EXIT_NEEDS_ARGUMENTS 5
#define EXIT_NO_MORE_MEMORY 42


/** only this file knows the operating system */
#include <string>
#include <vector>
using namespace std;

#include <stdint.h>

/** show memory usage */
uint64_t getMemoryUsageInKiBytes();

string getOperatingSystem();

void showDate();

/**
 * get the process identifier 
 */
int portableProcessId();

uint64_t getMilliSeconds();

void showMemoryUsage(int rank);

uint64_t getMicroseconds();

uint64_t getThreadMicroseconds();

/** create a directory */
void createDirectory(const char*directory);

bool fileExists(const char*file);

void printTheSeconds(int seconds,ostream*stream);

void getDirectoryFiles(string & file, vector<string> & files);

#endif
