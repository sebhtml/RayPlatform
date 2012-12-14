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

#include "Torus.h"

#include <iostream>
using namespace std;

#ifdef ASSERT
#include <assert.h>
#endif /* ASSERT */

#define __INVALID_ALPHABET_SIZE -1

/**
 * TODO: move this upstream
 */
int Torus::getPower(int base,int exponent){
	int a=1;
	while(exponent--)
		a*=base;
	return a;
}

/**
 * TODO: move this upstream
 *  convert a number to a vertex
 */
void Torus::convertToVertex(int i,Tuple*tuple){
	for(int power=0;power<m_dimension;power++){
		int value=(i%getPower(m_radix,power+1))/getPower(m_radix,power);
		tuple->m_digits[power]=value;
	}
}

/**
 * TODO: move this upstream
 */
bool Torus::isAPowerOf(int n,int base){
	int remaining=n;
	
	while(remaining>1){
		if(remaining%base != 0)
			return false;
		remaining/=base;
	}
	return true;
}

void Torus::configureGraph(int vertices){

	cout<<"[Torus] configureGraph vertices= "<<vertices<<" degree="<<m_degree<<endl;

	int theRadix=2;

	/* try to modify the base such that radix^dimension= vertices */
	/* and that meets the provided degree. */

	for(int radix=vertices-1;radix>=2;radix--){
		int dimension=1;

		while(vertices > getPower(radix,dimension)){
			dimension++;
		}

		if(vertices == getPower(radix,dimension)){

/*
 * For each dimension we can go left and right.
 */
			int degree=2*dimension;

			if(degree==m_degree){
				cout<<"[Torus] found a torus topology that matches the provided degree "<<m_degree<<endl;

				theRadix=radix;

				break;

			}
		}
	}


	if(!isAPowerOf(vertices,theRadix)){
		cout<<"Error: "<<vertices<<" is not a power of "<<theRadix<<", can not use the torus."<<endl;
		m_radix=__INVALID_ALPHABET_SIZE;
		return;
	}

	int dimension=1;

	while(vertices > getPower(theRadix,dimension)){
		dimension++;
	}

	#ifdef ASSERT
	assert(vertices ==getPower(theRadix,dimension));
	#endif

	m_radix=theRadix; // this is radix
	m_dimension=dimension; // this is wordLength
	m_size=vertices; // this is the number of vertices

	if(m_verbose)
		cout<<"[Torus] Vertices: "<<m_size<<" Radix: "<<m_radix<<" Dimension: "<<m_dimension<<" Degree: "<<m_degree<<endl;

	start();
}

uint64_t* Torus::getLoadValueCell(int position,int symbol){
	return m_loadValues+position*m_radix+symbol;
}

void Torus::setLoad(int position,int symbol,uint64_t value){

#if 0
	cout<<"[DEVEL] setLoad position="<<position<<" symbol="<<symbol<<" value="<<value<<endl;
#endif

	*(getLoadValueCell(position,symbol))=value;
}

uint64_t Torus::getLoad(int position,int symbol){
	
	#ifdef ASSERT
	assert(position<m_dimension);
	assert(symbol<m_radix);
	#endif /* ASSERT */

	return *(getLoadValueCell(position,symbol));
}

void Torus::makeConnections(int vertices){

	configureGraph(vertices);

	if(m_verbose){
		cout<<"[Torus::makeConnections] using "<<m_dimension<<" for diameter with base ";
		cout<<m_radix<<endl;
		cout<<"[Torus::makeConnections] The MPI graph has "<<m_size<<" vertices"<<endl;
		cout<<"[Torus::makeConnections] The torus has "<<m_size<<" vertices"<<endl;
	}

	// create empty sets.
	for(Rank i=0;i<m_size;i++){
		set<Rank> b;
		m_outcomingConnections.push_back(b);
		m_incomingConnections.push_back(b);
	}

	// populate vertices
	for(Rank i=0;i<m_size;i++){
		Tuple a;
		convertToVertex(i,&a);
		m_graphToVertex.push_back(a);
	}

	// make all connections.
	for(Rank i=0;i<m_size;i++){
		for(Rank j=0;j<m_size;j++){
			if(computeConnection(i,j)){
				m_outcomingConnections[i].insert(j);
				m_incomingConnections[j].insert(i);
			}
		}
	}
}

/* 
 * base m_radix to base 10 
 * TODO: this should be moved upstream 
 */
int Torus::convertToBase10(Tuple*vertex){
	int a=0;
	for(int i=0;i<m_dimension;i++){
		a+=vertex->m_digits[i]*getPower(m_radix,i);
	}
	return a;
}

/**
 * TODO: this should be moved upstream
 */
void Torus::printVertex(Tuple*a){
	for(int i=0;i<m_dimension;i++){
		if(i!=0)
			cout<<",";
		int value=a->m_digits[i];
		cout<<value;
	}
}

/**
 * TODO: remove me
 */
void Torus::computeRoute(Rank a,Rank b,vector<Rank>*route){
}

Rank Torus::getNextRankInRoute(Rank source,Rank destination,Rank current){

#if 0
	cout<<"[DEVEL] getNextRankInRoute source="<<source<<" destination="<<destination<<" current="<<current<<endl;
#endif

	#ifdef ASSERT
	assert(destination!=current); // we don't need any routing...
	assert(source>=0);
	assert(destination>=0);
	assert(current>=0);
	assert(source<m_size);
	assert(destination<m_size);
	assert(current<m_size);
	#endif

	//Tuple*sourceVertex=&(m_graphToVertex[source]);
	Tuple*destinationVertex=&(m_graphToVertex[destination]);
	Tuple*currentVertex=&(m_graphToVertex[current]);
	
	int NO_VALUE=-1;

	int bestPosition=NO_VALUE;
	int bestSymbol=0;
	uint64_t bestLoad=0;

	// select the next that has the lowest load.
	for(int position=0;position<m_dimension;position++){

		//int sourceSymbol=sourceVertex[i];
		int currentSymbol=currentVertex->m_digits[position];
		int destinationSymbol=destinationVertex->m_digits[position];

		// we don't need to test this one 
		// because the bit is already correct
		if(currentSymbol==destinationSymbol)
			continue;

/*
 * What is the distance in this dimension on the left ?
 */
		int leftDistance=getDistance(currentSymbol,destinationSymbol);

		#ifdef ASSERT
		assert(leftDistance!=0);
		assert(leftDistance>0);
		assert(leftDistance>=1);
		#endif

/*
 * Likewise, what is the distance in this dimension on the right ?
 */
		int rightDistance=getDistance(destinationSymbol,currentSymbol);

		#ifdef ASSERT
		assert(rightDistance!=0);
		assert(rightDistance>0);
		assert(rightDistance>=1);
		#endif

		if(leftDistance<=rightDistance){

			int nextSymbol=decrementSymbol(currentSymbol);

			#ifdef ASSERT
			assert(getMinimumDistance(currentSymbol,nextSymbol)==1);
			assert(getMinimumDistance(nextSymbol,currentSymbol)==1);
			#endif

			// at this point, the symbol needs to be changed
			uint64_t load=getLoad(position,nextSymbol);

			// select this edge if it has a lower load 
			// or if it is the first one we are testing.
			if(bestPosition==NO_VALUE||load<bestLoad){
				bestPosition=position;
				bestSymbol=nextSymbol;
				bestLoad=load;
			}
		}

		if(rightDistance<=leftDistance){

			int nextSymbol=incrementSymbol(currentSymbol);

			#ifdef ASSERT
			assert(getMinimumDistance(currentSymbol,nextSymbol)==1);
			assert(getMinimumDistance(nextSymbol,currentSymbol)==1);
			#endif

			// at this point, the symbol needs to be changed
			uint64_t load=getLoad(position,nextSymbol);

			// select this edge if it has a lower load 
			// or if it is the first one we are testing.
			if(bestPosition==NO_VALUE||load<bestLoad){
				bestPosition=position;
				bestSymbol=nextSymbol;
				bestLoad=load;
			}
		}
	}

	#ifdef ASSERT
	assert(bestPosition!=NO_VALUE);
	assert(bestPosition>=0);
	assert(bestPosition<m_dimension);
	assert(bestSymbol>=0);
	assert(bestSymbol<m_radix);
	assert(bestLoad>=0);
	#endif

	// here, we know where we want to go
	//
	// This is the agenda:
	//                 
	//                  * we are here
	//                  *
	//                  *
	// source -> ... -> current -> next -> ... -> destination

	// we just need to update next with the choice having
	// the lowest load

	Tuple next=*currentVertex;

	next.m_digits[bestPosition]=bestSymbol;

#if 0
	cout<<"[DEVEL] upstream setLoad position="<<bestPosition<<" symbol="<<bestSymbol<<" value="<<bestLoad+1;
	cout<<" current:";
	printVertex(currentVertex);
	cout<<" next:";
	printVertex(&next);
	cout<<endl;
#endif

	// we increment the load by 1
	setLoad(bestPosition,bestSymbol,bestLoad+1);

	Rank nextRank=convertToBase10(&next);

	#ifdef ASSERT
	assert(current!=nextRank);
	assert(computeConnection(current,nextRank) == true);
	#endif

	return nextRank;
}

/** with de Bruijn routing, no route are pre-computed at all */
void Torus::makeRoutes(){
	/* compute relay points */

	computeRelayEvents();
}

int Torus::getDistance(int end,int start){

/*
 * We could also add m_radix only if symbol-1 < 0, but out-of-order execution is
 * faster without branching.
 */

	return (end-start+m_radix)%m_radix;
}

int Torus::incrementSymbol(int symbol){

/*
 * We could also add m_radix only if symbol-1 < 0, but out-of-order execution is
 * faster without branching.
 */

	return (symbol+1)%m_radix;
}

int Torus::decrementSymbol(int symbol){

/*
 * We could also add m_radix only if symbol-1 < 0, but out-of-order execution is
 * faster without branching.
 */
	return (symbol-1+m_radix)%m_radix;
}

/**
 * TODO: move this upstream
 */
bool Torus::isConnected(Rank source,Rank destination){
	if(source==destination)
		return true;

	return m_outcomingConnections[source].count(destination)==1;
}

int Torus::getMinimumDistance(int a,int b){

	int left=getDistance(a,b);
	int right=getDistance(b,a);

	if(left<right)
		return left;

	return right;
}

/** 
 * Just verify the edge property
 * Also, we allow any vertex to communicate with itself
 * regardless of the property.
 */
bool Torus::computeConnection(Rank source,Rank destination){

/*
 * Not connected:   3,4 (23) -> 3,2 (13) Load: 150
 *
 */
	if(source==destination) // no difference
		return false;

	Tuple*sourceVertex=&(m_graphToVertex[source]);
	Tuple*destinationVertex=&(m_graphToVertex[destination]);

	int differencesOf1=0;
	int same=0;

/* 
 * Count the number if dimensions that differ by only 1.
 */
	for(int i=0;i<m_dimension;i++){

		int sourceSymbol=sourceVertex->m_digits[i];
		int destinationSymbol=destinationVertex->m_digits[i];

		if(sourceSymbol==destinationSymbol){
			same++;
			continue;
		}

		if(incrementSymbol(sourceSymbol)==destinationSymbol || decrementSymbol(sourceSymbol)==destinationSymbol)
			differencesOf1++;
	}

	bool connected=false;

	if((differencesOf1+same) == m_dimension && differencesOf1==1)
		connected=true;

#if 0
	if(connected){
		cout<<"[DEVEL] ";
		printVertex(sourceVertex);
		cout<<" ("<<source<<")";
		cout<<" and ";
		printVertex(destinationVertex);
		cout<<" ("<<destination<<")";
		cout<<" are connected."<<endl;
	}
#endif

	return connected;
}

/**
 * TODO: move this upstream, instead, add a protected m_valid field.
 */
bool Torus::isValid(int vertices){
	configureGraph(vertices);

	return m_radix!=__INVALID_ALPHABET_SIZE;
}

/**
 * TODO: move this upstream
 */
void Torus::setDegree(int degree){
	m_degree=degree;
}

void Torus::printStatus(Rank rank){

	cout<<"[Torus] Load values:"<<endl;
	cout<<"Radix: "<<m_radix<<endl;
	cout<<"Dimension: "<<m_dimension<<endl;
	cout<<"Self: ";
	Tuple*self=&(m_graphToVertex[rank]);

	printVertex(self);
	cout<<endl;

	for(int position=0;position<m_dimension;position++){

		int selfSymbol=self->m_digits[position];

		for(int otherSymbol=0;otherSymbol<m_radix;otherSymbol++){

			if(selfSymbol==otherSymbol)// nothing to report
				continue;

			uint64_t load=getLoad(position,otherSymbol);

			if(load==0)
				continue;

			Tuple copy=*self;
			copy.m_digits[position]=otherSymbol;
			cout<<"  ";
			printVertex(self);
			cout<<" ("<<rank<<")";
			cout<<" -> ";
			printVertex(&copy);

			Rank other=convertToBase10(&copy);
			cout<<" ("<<other<<")";
			cout<<" Load: "<<load<<endl;
			
			#ifdef ASSERT
			if(computeConnection(rank,other)!=true){
				cout<<"Error: position="<<position<<" otherSymbol= "<<otherSymbol<<", load="<<load<<" but the edge does not exist."<<endl;
			}
			assert(computeConnection(rank,other) == true);
			#endif
		}
	}
}

void Torus::start(){
	for(int i=0;i<m_dimension;i++)
		for(int j=0;j<m_radix;j++)
			setLoad(i,j,0);

	#ifdef ASSERT

	for(int i=0;i<m_dimension;i++)
		for(int j=0;j<m_radix;j++)
			assert(getLoad(i,j)==0);
	#endif
}


