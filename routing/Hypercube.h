/*
 	RayPlatform
    Copyright (C) 2010, 2011, 2012  Sébastien Boisvert

	http://DeNovoAssembler.SourceForge.Net/

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

#ifndef _Hypercube_h
#define _Hypercube_h

#include <routing/GraphImplementation.h>
#include <vector>
#include <stdint.h>
using namespace std;

// for alphabetSize = 2 and 4096 vertices
// we have 2^12 = 4096
// each vertex has (2-1)*10 = 10 edges
// in that case, the maximum index will be 12*2+11=35.
//
// for alphabetSize = 32
// we have 32^2 =1024
//
// each vertex has (32-1)*2 = 62 edges
// in that case, the maximum index will be 2*32+31=95
//
#define __NUMBER_OF_STORED_LOAD_VALUES 256

/**
 * Hypercube
 * n must be a power of 2 to be a hypercube.
 * Otherwise, it is an other polytope.
 * A hypercube is a convex regular polytope.
 * This class in fact implements a generalized hypercube,
 * usually called a convex regular polytope.
 * \author Sébastien Boisvert
 */
class Hypercube : public GraphImplementation{

	uint64_t m_loadValues[__NUMBER_OF_STORED_LOAD_VALUES];

	/** the user-provided degree */
	int m_degree;

	vector<Tuple> m_graphToVertex;

	int m_alphabetSize;

	int m_wordLength;

	void configureGraph(int n);

	int getPower(int base,int exponent);

/** convert a number to a vertex */
	void convertToVertex(int i,Tuple*tuple);

/** convert a vertex to base 10 */
	int convertToBase10(Tuple*vertex);

	void printVertex(Tuple*a);
	bool isAPowerOf(int n,int base);

	int getMaximumOverlap(Tuple*a,Tuple*b);

	Rank computeNextRankInRoute(Rank source,Rank destination,Rank rank);


	Rank computeNextRankInRouteWithRoundRobin(Rank source,Rank destination,Rank rank);
	int m_currentPosition;
	int m_currentSymbol;
	bool computeConnection(Rank source,Rank destination);


	uint64_t getLoad(int position,int symbol);
	void setLoad(int position,int symbol,uint64_t value);

protected:

	void computeRoute(Rank a,Rank b,vector<Rank>*route);
	Rank getNextRankInRoute(Rank source,Rank destination,Rank rank);
	bool isConnected(Rank source,Rank destination);
public:

	void makeConnections(int n);
	void makeRoutes();

	void setDegree(int degree);

	bool isValid(int n);
	void printStatus(Rank rank);
	void start();
};

#endif
