/*
 	Ray
    Copyright (C) 2010, 2011  Sébastien Boisvert

	http://DeNovoAssembler.SourceForge.Net/

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, version 3 of the License.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You have received a copy of the GNU General Public License
    along with this program (COPYING).  
	see <http://www.gnu.org/licenses/>

*/

#ifndef _OperatingSystem_h
#define _OperatingSystem_h

/** only this file knows the operating system */
#include <string>
#include <core/constants.h> 
#include <communication/MessagesHandler.h>
using namespace std;

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

void getMicroSeconds(uint64_t*seconds,uint64_t*microSeconds);

uint64_t getMicroSecondsInOne();

/** create a directory */
void createDirectory(const char*directory);

bool fileExists(const char*file);

void showRayVersion(MessagesHandler*messagesHandler,bool fullReport);

void showRayVersionShort();

#endif
