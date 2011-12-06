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

#ifndef _Derivative_h
#define _Derivative_h

#include <vector>
#include <fstream>
#include <core/OperatingSystem.h>
#include <map>
#include <stdint.h>
using namespace std;

class Derivative{
	map<int,vector<int> > m_data;

	vector<uint64_t> m_timeValues;
	vector<int> m_xValues;

public:
	Derivative();
	void addX(int x);
	int getLastSlope();
	void printStatus(const char*mode,int modeIdentifier);
	void printEstimatedTime(int n);
	void clear();

	void writeFile(ostream*f);
};

#endif

