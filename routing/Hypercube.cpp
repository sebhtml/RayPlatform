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

#include <routing/Hypercube.h>
#include <iostream>
using namespace std;

#ifdef ASSERT
#include <assert.h>
#endif /* ASSERT */

#define __INVALID_ALPHABET_SIZE -1

/**
 * TODO: move this upstream
 */
int Hypercube::getPower(int base,int exponent){
	int a=1;
	while(exponent--)
		a*=base;
	return a;
}

/**
 * TODO: move this upstream
 *  convert a number to a vertex
 */
void Hypercube::convertToVertex(int i,Tuple*tuple){
	for(int power=0;power<m_wordLength;power++){
		int value=(i%getPower(m_alphabetSize,power+1))/getPower(m_alphabetSize,power);
		tuple->m_digits[power]=value;
	}
}

/**
 * TODO: move this upstream
 */
bool Hypercube::isAPowerOf(int n,int base){
	int remaining=n;
	
	while(remaining>1){
		if(remaining%base != 0)
			return false;
		remaining/=base;
	}
	return true;
}

/**
 * only implemented for the hypercube with alphabet {0,1}
 */
void Hypercube::configureGraph(int n){

	#if 0
	// test some cases
	n=1024;
	m_degree=62;
	#endif

	int base=2;

	// Given alphabetSize and wordLength,
	//
	// we have 
	//      numberOfVertices := alphabetSize^wordLength  (1)
	//
	//       <=>
	//
	//      wordLength := log(numberOfVertices)/log(alphabetSize) (1.1)
	//
	// and
	//      degree := (alphabetSize-1)*wordLength        (2)
	//
	//       <=>
	//
	//	wordLength := degree / (alphabetSize-1)      (2.1)
	//
	//	Now we have:
	//
	//	log(numberOfVertices)/log(alphabetSize) = degree / (alphabetSize-1)
	//
	//	or
	//
	//	(alphabetSize -1) * log(numberOfVertices) = degree * log(alphabetSize)
	//
	// The alphabetSize of a hypercube is 2.
	//
	// However, we can build similar things to a hypercube
	// with a different alphabetSize
	//
	// alphabetSize=2, 1024 vertices
	//
	//
	//
	// we add an edge if u and v have exactly
	// 1 different symbol in their tuple representation

	// use the user-provided degree, if any
	//
	// TODO: try to use the provided degree
	// see equations above...
	#if 0
	if(isAPowerOf(n,m_degree) && false){
		maxBase=m_degree;
	}
	#endif

	#if 0
	while(maxBase>=n)
		maxBase/=2;
	#endif

	#if 0
	for(int i=maxBase;i>=2;i--){
		if(isAPowerOf(n,i)){
			base=i;
			break;
		}
	}
	#endif

	/* try to modify the base such that base^exponent = vertices */
	/* and that meets the provided degree. */

	for(int alphabetSize=n-1;alphabetSize>=2;alphabetSize--){
		int wordLength=1;

		while(n > getPower(alphabetSize,wordLength)){
			wordLength++;
		}

		if(n == getPower(alphabetSize,wordLength)){
			int degree=(alphabetSize-1)*wordLength;
			if(degree==m_degree){
				cout<<"[Hypercube] found a hypercube topology that matches the provided degree "<<m_degree<<endl;

				base=alphabetSize;
			}
		}
	}


	if(!isAPowerOf(n,base)){
		cout<<"Error: "<<n<<" is not a power of 2, can not use the hypercube."<<endl;
		m_alphabetSize=__INVALID_ALPHABET_SIZE;
		return;
	}

	int digits=1;

	while(n > getPower(base,digits)){
		digits++;
	}

	#ifdef ASSERT
	assert(n==getPower(base,digits));
	#endif

	m_alphabetSize=base; // this is alphabetSize
	m_wordLength=digits; // this is wordLength
	m_size=n; // this is the number of vertices

	cout<<"[Hypercube] Size: "<<m_size<<" AlphabetSize: "<<m_alphabetSize<<" WordLength: "<<m_wordLength<<" Degree: "<<m_degree<<endl;
	start();
}

void Hypercube::setLoad(int position,int symbol,uint64_t value){
	m_loadValues[position*m_alphabetSize+symbol]=value;
}

uint64_t Hypercube::getLoad(int position,int symbol){
	
	#ifdef ASSERT
	assert(position<m_wordLength);
	assert(symbol<m_alphabetSize);
	#endif /* ASSERT */

	return m_loadValues[position*m_alphabetSize+symbol];
}

void Hypercube::makeConnections(int n){

	configureGraph(n);

	if(m_verbose){
		cout<<"[Hypercube::makeConnections] using "<<m_wordLength<<" for diameter with base ";
		cout<<m_alphabetSize<<endl;
		cout<<"[Hypercube::makeConnections] The MPI graph has "<<m_size<<" vertices"<<endl;
		cout<<"[Hypercube::makeConnections] The hypercube has "<<m_size<<" vertices"<<endl;
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
 * base m_alphabetSize to base 10 
 * TODO: this should be moved upstream 
 */
int Hypercube::convertToBase10(Tuple*vertex){
	int a=0;
	for(int i=0;i<m_wordLength;i++){
		a+=vertex->m_digits[i]*getPower(m_alphabetSize,i);
	}
	return a;
}

/**
 * TODO: this should be moved upstream
 */
void Hypercube::printVertex(Tuple*a){
	for(int i=0;i<m_wordLength;i++){
		if(i!=0)
			cout<<",";
		int value=a->m_digits[i];
		cout<<value;
	}
}

/**
 * TODO: remove me
 */
void Hypercube::computeRoute(Rank a,Rank b,vector<Rank>*route){}

// use a round-robin algorithm.
//#define __ROUND_ROBIN__
Rank Hypercube::getNextRankInRoute(Rank source,Rank destination,Rank rank){

	#ifdef __ROUND_ROBIN__

	return computeNextRankInRouteWithRoundRobin(source,destination,rank);

	#else

	return computeNextRankInRoute(source,destination,rank);

	#endif /* __ROUND_ROBIN__ */
}

/** with de Bruijn routing, no route are pre-computed at all */
void Hypercube::makeRoutes(){
	/* compute relay points */
	computeRelayEvents();
}

/** to get the next rank,
 * we need to shift the current one time on the left
 * then, we find the maximum overlap
 * between the current and the destination
 *
 * This value is the index of the digit in destination
 * that we want to append to the next rank in the route
 *
 *	// example:
	//
	// base = 16
	// digits = 3
	//
	// source = (0,4,2)
	// destination = (9,8,7)
	//
	// the path is
	//
	// (0,4,2) -> (4,2,9) -> (2,9,8) -> (9,8,7)
	//
	// so for sure we have to shift the current by one on the left
	//
	// then the problem is how to choose which symbol to add 
	//
	// let's say that
	//
	// current = (4,2,9)
	//
	// then we should return (2,9,8) rapidly
	//
	// source=(0,2,2)
	// destination=(2,2,1)
	//
	// (0,2,2) -> (2,2,1)	

	// here we need to choose a digit from the destination
	// and append it to the next
	// case 1 destination can be obtained with 1 shift, overlap is 2
	// case 2 destination can be obtained with 2 shifts, overlap is 1
	// case 3 ...
	// case m_digits destination can be obtained with m_digits shifts, overlap is 0

 */
Rank Hypercube::computeNextRankInRoute(Rank source,Rank destination,Rank current){

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
	int bestSymbol=NO_VALUE;
	uint64_t bestLoad=NO_VALUE;

	// select the next that has the lowest load.
	for(int position=0;position<m_wordLength;position++){
		//int sourceSymbol=sourceVertex[i];
		int currentSymbol=currentVertex->m_digits[position];
		int destinationSymbol=destinationVertex->m_digits[position];


		// we don't need to test this one 
		// because the bit is already correct
		if(currentSymbol==destinationSymbol)
			continue;
	
		// at this point, the symbol needs to be changed
		uint64_t load=getLoad(position,destinationSymbol);

		// select this edge if it has a lower load 
		// or if it is the first one we are testing.
		if(bestPosition==NO_VALUE||load<bestLoad){
			bestPosition=position;
			bestSymbol=destinationSymbol;
			bestLoad=load;
		}
	}

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

	// the next will be populated in the loop
	Tuple next=*currentVertex;


	next.m_digits[bestPosition]=bestSymbol;
	
	// we increate the load by 1
	setLoad(bestPosition,bestSymbol,bestLoad+1);

	Rank nextRank=convertToBase10(&next);

	#ifdef ASSERT
	assert(current!=nextRank);
	#endif

	return nextRank;
}

/**
 * TODO: move this upstream
 */
bool Hypercube::isConnected(Rank source,Rank destination){
	if(source==destination)
		return true;

	return m_outcomingConnections[source].count(destination)==1;
}

/** 
 * just verify the edge property
 * also, we allow any vertex to communicate with itself
 * regardless of the property
 */
bool Hypercube::computeConnection(Rank source,Rank destination){

	if(source==destination) // no difference
		return false;

	// otherwise, we look for the de Bruijn property
	Tuple*sourceVertex=&(m_graphToVertex[source]);
	Tuple*destinationVertex=&(m_graphToVertex[destination]);

	int differences=0;

	for(int i=0;i<m_wordLength;i++){
		if(sourceVertex->m_digits[i]!=destinationVertex->m_digits[i])
			differences++;

		if(differences>1) // more than 1 difference
			return false;
	}

	return differences==1;
}

/**
 * TODO: move this upstream, instead, add a protected m_valid field.
 */
bool Hypercube::isValid(int n){
	configureGraph(n);

	return m_alphabetSize!=__INVALID_ALPHABET_SIZE;
}

/**
 * TODO: move this upstream
 */
void Hypercube::setDegree(int degree){
	m_degree=degree;
}

void Hypercube::printStatus(Rank rank){
	cout<<"[Hypercube] Load values:"<<endl;
	cout<<"AlphabetSize: "<<m_alphabetSize<<endl;
	cout<<"WordLength: "<<m_wordLength<<endl;
	cout<<"Self: ";
	Tuple*self=&(m_graphToVertex[rank]);

	printVertex(self);
	cout<<endl;

	for(int i=0;i<m_wordLength;i++){

		int selfSymbol=self->m_digits[i];

		for(int j=0;j<m_alphabetSize;j++){

			if(selfSymbol==j)// nothing to report
				continue;

			uint64_t load=getLoad(i,j);
			Tuple copy=*self;
			copy.m_digits[i]=j;
			cout<<"  ";
			printVertex(self);
			cout<<" ("<<rank<<")";
			cout<<" -> ";
			printVertex(&copy);

			Rank other=convertToBase10(&copy);
			cout<<" ("<<other<<")";
			cout<<" Load: "<<load<<endl;
			
		}
	}
}

void Hypercube::start(){
	for(int i=0;i<m_wordLength;i++)
		for(int j=0;j<m_alphabetSize;j++)
			setLoad(i,j,0);

	m_currentPosition=0;
	m_currentSymbol=0;
}

Rank Hypercube::computeNextRankInRouteWithRoundRobin(Rank source,Rank destination,Rank current){

	#ifdef ASSERT
	assert(destination!=current); // we don't need any routing...
	assert(source>=0);
	assert(destination>=0);
	assert(current>=0);
	assert(source<m_size);
	assert(destination<m_size);
	assert(current<m_size);
	#endif

	#ifdef ASSERT
	Tuple*sourceVertex=&(m_graphToVertex[source]);
	#endif

	Tuple*destinationVertex=&(m_graphToVertex[destination]);
	Tuple*currentVertex=&(m_graphToVertex[current]);
	
	int NO_VALUE=-1;
	int bestPosition=NO_VALUE;
	int bestSymbol=NO_VALUE;

	#if 0 /* to disable round-robin */
	m_currentPosition=0;
	m_currentSymbol=0;
	#endif


	// first slice of round-robin
	for(int position=m_currentPosition;position<m_wordLength;position++){

		if(bestPosition!=NO_VALUE)// we found something
			break;

		int currentSymbol=currentVertex->m_digits[position];
		int destinationSymbol=destinationVertex->m_digits[position];

		if(destinationSymbol==currentSymbol)// it is already correct.
			continue;

		int symbolStart=0;

		if(position==m_currentPosition)// for the first round, we start at m_currentSymbol
			symbolStart=m_currentSymbol;

		//symbolStart=0;// TODO: remove me

		for(int symbol=symbolStart;symbol<m_alphabetSize;symbol++){

			if(bestPosition!=NO_VALUE)// we found something
				break;

			if(symbol!=destinationSymbol)// this is not the correct symbol
				continue;

			// we found something to do!
			bestPosition=position;
			bestSymbol=symbol;
		}

	}

	// second slice of round-robin
	// we also try the symbols below m_currentSymbol for m_currentPosition
	// to complete the round robin
	for(int position=0;position<=m_currentPosition;position++){

		if(bestPosition!=NO_VALUE)// we found something
			break;

		int currentSymbol=currentVertex->m_digits[position];
		int destinationSymbol=destinationVertex->m_digits[position];

		if(destinationSymbol==currentSymbol)// it is already correct.
			continue;

		int maximumSymbol=m_alphabetSize;

		if(position==m_currentPosition)// we need to complete the round-robin
			maximumSymbol=m_currentSymbol;

		//maximumSymbol=m_alphabetSize;// TODO: remove me

		for(int symbol=0;symbol<maximumSymbol;symbol++){

			if(bestPosition!=NO_VALUE)// we found something
				break;

			if(symbol!=destinationSymbol)// this is not the correct symbol
				continue;

			// we found something to do!
			bestPosition=position;
			bestSymbol=symbol;
		}

	}

	#ifdef ASSERT
	if(bestPosition==NO_VALUE){
		cout<<"Error: found no eligible position."<<endl;
		cout<<"m_currentPosition: "<<m_currentPosition<<endl;
		cout<<"m_currentSymbol: "<<m_currentSymbol<<endl;
		cout<<"m_alphabetSize: "<<m_alphabetSize<<endl;
		cout<<"m_wordLength: "<<m_wordLength<<endl;

		cout<<"Source: "<<source<<" <=> ";
		printVertex(sourceVertex);
		cout<<endl;
		cout<<"Current: "<<current<<" <=> ";
		printVertex(currentVertex);
		cout<<endl;
		cout<<"Destination: "<<destination<<" <=> ";
		printVertex(destinationVertex);
		cout<<endl;
	}
	assert(bestPosition!=NO_VALUE);
	assert(bestSymbol!=NO_VALUE);
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

	// the next will be populated in the loop
	Tuple next=*currentVertex;

	next.m_digits[bestPosition]=bestSymbol;
	
	uint64_t bestLoad=getLoad(bestPosition,bestSymbol);

	// we increate the load by 1
	setLoad(bestPosition,bestSymbol,bestLoad+1);

	Rank nextRank=convertToBase10(&next);

	#ifdef ASSERT
	assert(current!=nextRank);
	#endif

	// update information for round-robin
	
	m_currentPosition=bestPosition;
	m_currentSymbol=bestSymbol;
	m_currentSymbol++;
	if(m_currentSymbol==m_alphabetSize){
		m_currentSymbol=0;
		m_currentPosition++;
	}
	if(m_currentPosition==m_wordLength)
		m_currentPosition=0;


	return nextRank;
}
