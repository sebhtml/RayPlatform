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

#ifndef _Torus_h
#define _Torus_h

#include "GraphImplementation.h"

#include <vector>
#include <stdint.h>
using namespace std;

#define __NUMBER_OF_STORED_LOAD_VALUES 256

/**
 * Torus, also known as k-ary n-cube
 *
 * Parameters:
 *
 * - dimension
 * - radix (number of vertices per dimension)
 * - vertices = radix ^ dimension
 * - degree =:
 *
 * \see http://www.doc.ic.ac.uk/~phjk/AdvancedCompArchitecture/2001-02/Lectures.old/Ch06/node23.html
 * \see http://www.doc.ic.ac.uk/~phjk/AdvancedCompArchitecture/2001-02/Lectures.old/Ch06/node24.html
 * \see en.wikipedia.org/wiki/Torus#n-dimensional_torus
 * \see http://en.wikipedia.org/wiki/Hypercube
 *
 * \author Sébastien Boisvert
 */
class Torus : public GraphImplementation{

/**
 * Load values.
 */
	uint64_t m_loadValues[__NUMBER_OF_STORED_LOAD_VALUES];

/** 
 * The user-provided degree.
 */
	int m_degree;

	vector<Tuple> m_graphToVertex;

	int m_radix;

	int m_dimension;

	void configureGraph(int n);

	int getPower(int base,int exponent);

/** convert a number to a vertex */
	void convertToVertex(int i,Tuple*tuple);

/** convert a vertex to base 10 */
	int convertToBase10(Tuple*vertex);

	void printVertex(Tuple*a);
	bool isAPowerOf(int n,int base);

	bool computeConnection(Rank source,Rank destination);

	uint64_t getLoad(int position,int symbol);
	void setLoad(int position,int symbol,uint64_t value);

	int incrementSymbol(int symbol);
	int decrementSymbol(int symbol);
	int getDistance(int end,int start);
	int getMinimumDistance(int a,int b);

	uint64_t* getLoadValueCell(int position,int symbol);
protected:

	void computeRoute(Rank a,Rank b,vector<Rank>*route);
	Rank getNextRankInRoute(Rank source,Rank destination,Rank rank);
	bool isConnected(Rank source,Rank destination);

public:

	void makeConnections(int vertices);
	void makeRoutes();

	void setDegree(int degree);

	bool isValid(int vertices);
	void printStatus(Rank rank);
	void start();
};

#endif
