/*
 	Ray
    Copyright (C) 2011, 2012  Sébastien Boisvert

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

/* the code in this file is based on my understanding (Sébastien Boisvert) of
 * the content of this page about Google sparse hash map.
 * \see http://google-sparsehash.googlecode.com/svn/trunk/doc/implementation.html
 *
 * 	double hashing is based on page 529 
 * 	of 
 * 	Donald E. Knuth
 * 	The Art of Computer Programming, Volume 3, Second Edition
 *
 * Furthermore, it implements incremental resizing for real-time communication with low latency.
 */

#ifndef _MyHashTable_H
#define _MyHashTable_H

/**
 * The precision of probe statistics.
 */
#define MAX_SAVED_PROBE 16

#include <stdint.h>
#include <time.h>
#include "structures/MyHashTableGroup.h"
#include <memory/ChunkAllocatorWithDefragmentation.h>
#include <iostream>
#include <assert.h>
#include <string.h> /* for strcpy */
#include <memory/allocator.h> /* for __Malloc */
using namespace std;

/**
 * this hash table is specific to DNA
 * uses open addressing with double hashing
 *
 * class KEY must have these methods:
 *
 * hash_function_1()
 * hash_function_2()
 * print()
 *
 * class VALUE must have these methods:
 *
 * getKey()
 *
 * the number of buckets can not be exceeded. 
 * based on the description at
 * \see http://google-sparsehash.googlecode.com/svn/trunk/doc/implementation.html
 *
 * all operations are constant-time or so.
 *
 * probing depth is usually low because incremental resizing is triggered at load >= 70.0%
 * and double hashing eases exploration of bucket landscapes.
 *
 * \author Sébastien Boisvert
 * \date 2012-08-07 This class was reviewed by David Weese <weese@campus.fu-berlin.de>
 */
template<class KEY,class VALUE>
class MyHashTable{
	/** is this table verbose */
	bool m_verbose;

	/** currently doing incremental resizing ? */
	bool m_resizing;

	/** auxiliary table for incremental resizing */
	MyHashTable*m_auxiliaryTableForIncrementalResize;

	/** current bucket to transfer */
	uint64_t m_currentBucketToTransfer;

/**
 * the maximum acceptable load factor
 * beyond that value, the table is resized.
 */
	double m_maximumLoadFactor;

	/** 
 * 	chunk allocator
 */
	ChunkAllocatorWithDefragmentation m_allocator;

	/** 
 * 	the  maximum probes required in probing
 */
	int m_probes[MAX_SAVED_PROBE];

	/**
 * Message-passing interface rank
 */
	int m_rank;

	/**
 * the number of buckets in the theater */
	uint64_t m_totalNumberOfBuckets;

	/**
         * Number of utilised buckets 
         */
	uint64_t m_utilisedBuckets;

/**
 * 	number of inserted elements
 * contains the sum of elements in the main table + the number of elements in the new
 * table but not in the old one
 * when not resizing, m_utilisedBuckets and m_size are the same
 * when resizing, m_size may be larger than m_utilisedBuckets
 */
	uint64_t m_size;

	/**
 * 	groups of buckets
 */
	MyHashTableGroup<KEY,VALUE>*m_groups;

	/**
 * number of buckets per group
 */
	int m_numberOfBucketsInGroup;

	/**
 * 	number of groups 
 */
	int m_numberOfGroups;

/**
 * If the load factor is too high,
 * double the size, and regrow everything in the new one.
 */
	void growIfNecessary();

/**
 * resize the hash table
 */
	void resize();

/**
 * the type of memory allocation 
 */
	char m_mallocType[100];

/**
 * show the allocation events ?
 */
	bool m_showMalloc;


/*
 * find a key, but specify if the auxiliary table should be searched also */
	VALUE*findKey(KEY*key,bool checkAuxiliary);

public:
	
	void toggleVerbosity();

	/** unused constructor  */
	MyHashTable();

	/** unused constructor  */
	~MyHashTable();

	/**
 * 	is the seat available
 */
	bool isAvailable(uint64_t a);

	/**
 * build the buckets and the hash */
	void constructor(uint64_t buckets,const char*mallocType,bool showMalloc,int rank,
		int bucketsPerGroup,double loadFactorThreshold);

	/**
 * find a seat given a key */
	VALUE*find(KEY*key);

	/**
 * insert a key */
	VALUE*insert(KEY*key);

/**
 * 	find a bucket given a key
 * 	This uses double hashing for open addressing.
 */
	void findBucketWithKey(KEY*key,uint64_t*probe,int*group,int*bucketInGroup);

	/**
 * burn the theater */
	void destructor();

	/**
 * 	get the number of occupied buckets 
 */
	uint64_t size();

	/**
 * 	get a bucket directly
 * 	will fail if empty
 */
	VALUE*at(uint64_t a);
	
	/** get the number of buckets */
	uint64_t capacity();

	/**
 * 	print the statistics of the hash table including,
 * 	but not necessary limited to:
 * 	- the load factor
 */
	void printStatistics();

	void printProbeStatistics();

/**
 * callback function for immediate defragmentation
 */
	void defragment();

	void completeResizing();

	bool needsToCompleteResizing();
};

/* get a bucket */
template<class KEY,class VALUE>
VALUE*MyHashTable<KEY,VALUE>::at(uint64_t bucket){
	int group=bucket/m_numberOfBucketsInGroup;
	int bucketInGroup=bucket%m_numberOfBucketsInGroup;

	#ifdef ASSERT
	assert(m_groups[group].getBit(bucketInGroup)==1);
	#endif

	VALUE*e=m_groups[group].getBucket(bucketInGroup,&m_allocator);

	return e;
}

/** returns the number of buckets */
template<class KEY,class VALUE>
uint64_t MyHashTable<KEY,VALUE>::capacity(){
	return m_totalNumberOfBuckets;
}

/** makes the table grow if necessary */
template<class KEY,class VALUE>
void MyHashTable<KEY,VALUE>::growIfNecessary(){
	if(m_resizing)
		return;

	/**  check if a growth is necessary */
	double loadFactor=(0.0+m_utilisedBuckets)/m_totalNumberOfBuckets;
	if(loadFactor<m_maximumLoadFactor)
		return;

	/** double the size */
	uint64_t newSize=m_totalNumberOfBuckets*2;

	/** nothing to do because shrinking is not implemented */
	if(newSize<m_totalNumberOfBuckets)
		return;

	/** can not resize because newSize is too small */
	/** this case actually never happens */
	if(newSize<m_utilisedBuckets)
		return;

	m_currentBucketToTransfer=0;
	m_resizing=true;

	if(m_verbose){
		cout<<"Beginning resizing operations"<<endl;
		printProbeStatistics();
	}

	#ifdef ASSERT
	assert(m_auxiliaryTableForIncrementalResize==NULL);
	#endif

	m_auxiliaryTableForIncrementalResize=new MyHashTable();

	/** build a new larger table */
	m_auxiliaryTableForIncrementalResize->constructor(newSize,m_mallocType,m_showMalloc,m_rank,
		m_numberOfBucketsInGroup,m_maximumLoadFactor);

	//cout<<"Rank "<<m_rank<<" MyHashTable must grow now "<<m_totalNumberOfBuckets<<" -> "<<newSize<<endl;
	//printStatistics();

	#ifdef ASSERT
	assert(newSize>m_totalNumberOfBuckets);
	#endif
}

/** 
 * resize the whole hash table 
 */
template<class KEY,class VALUE>
void MyHashTable<KEY,VALUE>::resize(){
	#ifdef ASSERT
	assert(m_resizing==true);
	assert(m_utilisedBuckets>0);
	#endif

 /*
 *	Suppose the main table has N slots.
 *	Then, the auxiliary has 2*N slots.
 *
 *	At each call to insert(), we call resize().
 *	Each call to resize() will transfert T items.
 *
 * 	In the worst case, the main table has exactly N items to transfert (all buckets are full).
 * 	Thus, this would require N/T calls to resize()
 *
 * 	resize() is actually called when insert() is called, so
 *	at completion of the transfer, the auxiliary table would have
 *
 *	N/T   (elements inserted directly in the auxiliary table)
 *	N     (elements copied from the main table to the auxiliary table)
 *
 *	Thus, when the transfert is completed, the auxiliary table has 
 *
 *	at most N+N/T elements.
 *
 * 	We have 
 *
 * 	N+N/T <= 2N		(equation 1.)
 * 	N+N/T -N  <= 2N -N
 * 	N/T <= N
 * 	N/T*1/N <= N * 1/N
 * 	1/T <= 1
 * 	1/T * T <= 1 * T
 * 	1 <= T
 *
 * 	So,  basically, as long as T >= 1, we are fine.
 *
 * 	But in our case, resize is triggered when the main table has at least
 * 	0.7*N
 *
 * 	So, we need 0.7*N/T calls to resizes.
 *
 * 	Upon completion, the auxiliary table will have
 *
 * 	0.7*N/T + 0.7*N
 *
 * 	The equation is then:
 *
 * 	0.7*N/T + 0.7*N <= 0.7*2*N 	equation 2.
 *
 * 	(we want the process to complete before auxiliary triggers its own incremental resizing !)
 *
 * 	(divide everything by 0.7)
 *
 * 	N/T+N<=2N
 *
 * 	(same equation as equation 1.)
 *
 * 	thus, T>=1
 *
 * 	Here, we choose T=2
 * 	*/

	int toProcess=2;

	int i=0;
	while(i<toProcess && m_currentBucketToTransfer<capacity()){
		/** if the bucket is available, it means that it is empty */
		if(!isAvailable(m_currentBucketToTransfer)){
			VALUE*entry=at(m_currentBucketToTransfer);

			#ifdef ASSERT
			assert(entry!=NULL);
			#endif

			KEY keyValue=entry->getKey();
			KEY*key=&keyValue;

			#ifdef ASSERT
			uint64_t probe;
			int group;
			int bucketInGroup;
			findBucketWithKey(key,&probe,&group,&bucketInGroup);
			uint64_t globalBucket=group*m_numberOfBucketsInGroup+bucketInGroup;
			
			if(globalBucket!=m_currentBucketToTransfer)
				cout<<"Expected: "<<m_currentBucketToTransfer<<" Actual: "<<globalBucket<<endl;
			assert(globalBucket==m_currentBucketToTransfer);

			if(find(key)==NULL)
				cout<<"Error: can not find the content of global bucket "<<m_currentBucketToTransfer<<endl;
			assert(find(key)!=NULL);
			#endif

			/** remove the key too */
			/* actually, can not remove anything because otherwise it will make the double hashing 
 * 			fails */

			/** save the size before for further use */
			#ifdef ASSERT
			uint64_t sizeBefore=m_auxiliaryTableForIncrementalResize->m_utilisedBuckets;

			/* the auxiliary should not contain the key already . */
			if(m_auxiliaryTableForIncrementalResize->find(key)!=NULL)
				cout<<"Moving globalBucket "<<m_currentBucketToTransfer<<" but the associated key is already in auxiliary table."<<endl;
			assert(m_auxiliaryTableForIncrementalResize->find(key)==NULL);
			#endif

			/* this pointer  will remain valid until the next insert. */
			VALUE*insertedEntry=m_auxiliaryTableForIncrementalResize->insert(key);
		
			/** affect the value */
			(*insertedEntry)=*entry;
	
			/** assert that we inserted something somewhere */
			#ifdef ASSERT
			if(m_auxiliaryTableForIncrementalResize->m_utilisedBuckets!=sizeBefore+1)
				cout<<"Expected: "<<sizeBefore+1<<" Actual: "<<m_auxiliaryTableForIncrementalResize->m_utilisedBuckets<<endl;
			assert(m_auxiliaryTableForIncrementalResize->m_utilisedBuckets==sizeBefore+1);
			#endif
		}
		i++;
		m_currentBucketToTransfer++;
	}

	/* we transfered everything */
	if(m_currentBucketToTransfer==capacity()){
		//cout<<"Rank "<<m_rank<<": MyHashTable incremental resizing is complete."<<endl;
		/* make sure the old table is now empty */
		#ifdef ASSERT
		//assert(size()==0);
		//the old table still has its stuff.
		#endif

		/** destroy the current table */
		destructor();
	
		/** copy important stuff  */
		m_groups=m_auxiliaryTableForIncrementalResize->m_groups;
		m_totalNumberOfBuckets=m_auxiliaryTableForIncrementalResize->m_totalNumberOfBuckets;
		m_allocator=m_auxiliaryTableForIncrementalResize->m_allocator;
		m_numberOfGroups=m_auxiliaryTableForIncrementalResize->m_numberOfGroups;
		m_utilisedBuckets=m_auxiliaryTableForIncrementalResize->m_utilisedBuckets;
		m_size=m_auxiliaryTableForIncrementalResize->m_size;

		#ifdef ASSERT
		assert(m_size == m_utilisedBuckets);
		assert(m_auxiliaryTableForIncrementalResize->m_resizing == false);
		assert(m_size == m_utilisedBuckets);
		#endif

		/** copy probe profiles */
		for(int i=0;i<MAX_SAVED_PROBE;i++)
			m_probes[i]=m_auxiliaryTableForIncrementalResize->m_probes[i];
	
		/** indicates the caller that things changed places */

		//printStatistics();
		delete m_auxiliaryTableForIncrementalResize;
		m_auxiliaryTableForIncrementalResize=NULL;
		m_resizing=false;

		if(m_verbose){
			cout<<"Completed resizing operations"<<endl;
			printProbeStatistics();
		}
	}
}

/** return the number of elements
 * contains the number of elements in the main table + those in the second that are not in the first */
template<class KEY,class VALUE>
uint64_t MyHashTable<KEY,VALUE>::size(){
	return m_size;
}


template<class KEY,class VALUE>
void MyHashTable<KEY,VALUE>::toggleVerbosity(){
	m_verbose=!m_verbose;
}

/** build the hash table given a number of buckets 
 * this is private actually 
 * */
template<class KEY,class VALUE>
void MyHashTable<KEY,VALUE>::constructor(uint64_t buckets,const char*mallocType,bool showMalloc,int rank,
	int bucketsPerGroup,double loadFactorThreshold){

	// adjust the buckets.
	uint64_t check=2;
	while(check<buckets)
		check*=2;
	
	if(check!=buckets){
		cout<<"Warning: the number of buckets must be a power of 2"<<endl;
		buckets=check;
		cout<<"Changed buckets to "<<buckets<<endl;
	}

	/** this is the maximum acceptable load factor. */
	/** based on Figure 42 on page 531 of
 * 	The Art of Computer Programming, Second Edition, by Donald E. Knuth
 */

	/* TODO: why does the code crash when loadFactorThreshold < 0.5 ? */
	/* probably has something to do with resizing */
	if(loadFactorThreshold<0.5)
		loadFactorThreshold=0.5;

	if(loadFactorThreshold>1)
		loadFactorThreshold=1;

	m_maximumLoadFactor=loadFactorThreshold; 
	m_numberOfBucketsInGroup=bucketsPerGroup;

	m_auxiliaryTableForIncrementalResize=NULL;
	m_resizing=false;

	/** set the message-passing interface rank number */
	m_rank=rank;

	m_verbose=false;
	
	m_allocator.constructor(sizeof(VALUE),showMalloc);
	strcpy(m_mallocType,mallocType);

	m_showMalloc=showMalloc;

	/* use the provided number of buckets */
	/* the number of buckets is a power of 2 */
	m_totalNumberOfBuckets=buckets;

	m_size=0;

	m_utilisedBuckets=0;

	/* the number of buckets in a group
 * 	this is arbitrary I believe... 
 * 	in my case, 64 is the number of bits in a uint64_t
 * 	*/
	m_numberOfBucketsInGroup=bucketsPerGroup;

	/**
 * 	compute the required number of groups 
 */
	m_numberOfGroups=(m_totalNumberOfBuckets-1)/m_numberOfBucketsInGroup+1;

	/**
 * 	Compute the required number of bytes
 */
	int requiredBytes=m_numberOfGroups*sizeof(MyHashTableGroup<KEY,VALUE>);

	/** sanity check */
	#ifdef ASSERT
	if(requiredBytes<=0)
		cout<<"Groups="<<m_numberOfGroups<<" RequiredBytes="<<requiredBytes<<"  BucketsPower2: "<<m_totalNumberOfBuckets<<endl;
	assert(requiredBytes>=0);
	#endif

	/** allocate groups */
	m_groups=(MyHashTableGroup<KEY,VALUE>*)__Malloc(requiredBytes,
		mallocType,showMalloc);

	#ifdef ASSERT
	assert(m_groups!=NULL);
	#endif

	/** initialize groups */
	for(int i=0;i<m_numberOfGroups;i++)
		m_groups[i].constructor(m_numberOfBucketsInGroup,&m_allocator);

	/*  set probe profiles to 0 */
	for(int i=0;i<MAX_SAVED_PROBE;i++)
		m_probes[i]=0;
}

/** return yes if the bucket is empty, no otherwise */
template<class KEY,class VALUE>
bool MyHashTable<KEY,VALUE>::isAvailable(uint64_t bucket){
	int group=bucket/m_numberOfBucketsInGroup;
	int bucketInGroup=bucket%m_numberOfBucketsInGroup;
	
	#ifdef ASSERT
	assert(bucket<m_totalNumberOfBuckets);
	assert(group<m_numberOfGroups);
	assert(bucketInGroup<m_numberOfBucketsInGroup);
	#endif

	return m_groups[group].getBit(bucketInGroup)==0;
}

/** empty constructor, I use constructor instead  */
template<class KEY,class VALUE>
MyHashTable<KEY,VALUE>::MyHashTable(){}

/** empty destructor */
template<class KEY,class VALUE>
MyHashTable<KEY,VALUE>::~MyHashTable(){}

/**
 * find a bucket
 *
 * using probing, assuming a number of buckets that is a power of 2 
 * "But what is probing ?", you may ask.
 * This stuff is pretty simple actually.
 * Given an array of N elements and a key x and an hash
 * function HASH_FUNCTION, one simple way to get the bucket is
 *
 *  bucket =   HASH_FUNCTION(x)%N
 *
 * If a collision occurs (another key already uses bucket),
 * probing allows one to probe another bucket.
 *
 *  bucket =   ( HASH_FUNCTION(x) + probe(i) )%N
 *          
 *             where i is initially 0
 *             when there is a collision, increment i and check the new  bucket
 *             repeat until satisfied.
 */
template<class KEY,class VALUE>
void MyHashTable<KEY,VALUE>::findBucketWithKey(KEY*key,uint64_t*probe,int*group,int*bucketInGroup){
	/** 
 * 	double hashing is based on 
 * 	page 529 
 * 	of 
 * 	Donald E. Knuth
 * 	The Art of Computer Programming, Volume 3, Second Edition
 */
	(*probe)=0; 

	/** between 0 and M-1 -- M is m_totalNumberOfBuckets */
	uint64_t h1=key->hash_function_2()%m_totalNumberOfBuckets;

	/* first probe is 0 */
	/** double hashing is computed on probe 1, not on 0 */
	uint64_t h2=0;
	uint64_t bucket=0;

	bucket=(h1+(*probe)*h2)%m_totalNumberOfBuckets;

	/** use double hashing */
	(*group)=bucket/m_numberOfBucketsInGroup;
	(*bucketInGroup)=bucket%m_numberOfBucketsInGroup;

	#ifdef ASSERT
	assert(bucket<m_totalNumberOfBuckets);
	assert(*group<m_numberOfGroups);
	assert(*bucketInGroup<m_numberOfBucketsInGroup);
	#endif

	/** probe bucket */
	while(m_groups[*group].bucketIsUtilisedBySomeoneElse(*bucketInGroup,key,&m_allocator)
		&& ((*probe)+1) < m_totalNumberOfBuckets){

		(*probe)++;
		
		/** the stride and the number of buckets are relatively prime because
			number of buckets = M = 2^m
		and the stride is 1 <= s <= M-1 and is odd thus does not share factor with 2^m */

		#ifdef ASSERT
		/** issue a warning for an unexpected large probe depth */
		/** each bucket should be probed in exactly NumberOfBuckets probing events */
		if((*probe)>=m_totalNumberOfBuckets){
			cout<<"Rank "<<m_rank<<" Warning, probe depth is "<<(*probe)<<" h1="<<h1<<" h2="<<h2<<" m_totalNumberOfBuckets="<<m_totalNumberOfBuckets<<" m_size="<<m_size<<" m_utilisedBuckets="<<m_utilisedBuckets<<" bucket="<<bucket<<" m_resizing="<<m_resizing<<endl;
		}

		assert((*probe)<m_totalNumberOfBuckets);
		#endif

		/** only compute the double hashing once */
		if((*probe)==1){
			/**  page 529 
 * 				between 1 and M-1 exclusive
				odd number
 * 			*/

			h2=key->hash_function_1()%m_totalNumberOfBuckets;
			
			/** h2 can not be 0 */
			if(h2 == 0)
				h2=1;

			/** h2 can not be even */
			if(h2%2 == 0)
				h2--; 

			/** check boundaries */
			#ifdef ASSERT
			assert(h2!=0);
			assert(h2%2!=0);
			assert(h2%2 == 1);
			assert(h2>=1);

/*
			if(h2>=m_totalNumberOfBuckets)
				cout<<"h2= "<<h2<<" Buckets= "<<m_totalNumberOfBuckets<<endl;
*/
			assert(h2<m_totalNumberOfBuckets);
			assert((*probe)<m_totalNumberOfBuckets);
			#endif
		}

		#ifdef ASSERT
		assert(h2!=0);
		#endif
		
		/** use double hashing */
		//bucket=(h1+(*probe)*h2)%m_totalNumberOfBuckets;
		
		if(h2>bucket)
			bucket+=m_totalNumberOfBuckets;

		bucket-=h2;

		(*group)=bucket/m_numberOfBucketsInGroup;
		(*bucketInGroup)=bucket%m_numberOfBucketsInGroup;

		/** sanity checks */
		#ifdef ASSERT
		assert(bucket<m_totalNumberOfBuckets);
		assert(*group<m_numberOfGroups);
		assert(*bucketInGroup<m_numberOfBucketsInGroup);
		#endif
	}

	/** sanity checks */
	#ifdef ASSERT
	assert(bucket<m_totalNumberOfBuckets);
	assert(*group<m_numberOfGroups);
	assert(*bucketInGroup<m_numberOfBucketsInGroup);
	#endif
}

/**
 * finds a key
 */
template<class KEY,class VALUE>
VALUE*MyHashTable<KEY,VALUE>::findKey(KEY*key,bool checkAuxiliary){ /* for verbosity */

	if(m_resizing && checkAuxiliary){
		/* check the new one first */
		VALUE*result=m_auxiliaryTableForIncrementalResize->find(key);
		if(result!=NULL)
			return result;
	}

	/** get the bucket */
	uint64_t probe;
	int group;
	int bucketInGroup;
	findBucketWithKey(key,&probe,&group,&bucketInGroup);

	#ifdef ASSERT
	assert(group<m_numberOfGroups);
	assert(bucketInGroup<m_numberOfBucketsInGroup);
	#endif
	
	#if 0
	if(m_verbose){
		cout<<"GridTable (service provided by MyHashTable) -> found key [";
		for(int i=0;i<key->getNumberOfU64();i++){
			if(i>0)
				cout<<" ";

			cout<<hex<<""<<key->getU64(i)<<dec;
		}
		cout<<"] on rank "<<m_rank;
		cout<<" in group "<<group<<" in bucket "<<bucketInGroup;
		cout<<" after "<<probe<<" probe operation";
		if(probe>1)
			cout<<"s";
		cout<<endl;
	}
	#endif

	/** ask the group to find the key */
	return m_groups[group].find(bucketInGroup,key,&m_allocator);
}

/**
 * finds a key
 */
template<class KEY,class VALUE>
VALUE*MyHashTable<KEY,VALUE>::find(KEY*key){
	#ifdef ASSERT
	assert(key!=NULL);
	#endif

	return findKey(key,true);
}

/**
 * inserts a key
 * \author Sébastien Boisvert
 * \date 2012-08-07 This method was reviewed by David Weese <weese@campus.fu-berlin.de>
 */
template<class KEY,class VALUE>
VALUE*MyHashTable<KEY,VALUE>::insert(KEY*key){
	/** do incremental resizing */
	if(m_resizing){
		resize();
	}

	if(m_resizing){ // the auxiliary table is only alive during resizing operations

		// Case 1. If a key is already in the new one, then the new one is 
		// responsible, regardless if it was in the old one or not 

		VALUE*fromAuxiliary=m_auxiliaryTableForIncrementalResize->findKey(key,false);

		if(fromAuxiliary!=NULL){
			return fromAuxiliary;
		}

		// Case 2. If a key if not in the old (and was never in the old one), 
		// then the new one is responsible for it 
		//
		/* check if key is not in the main table
 		 * 		insert it in the other one */
		if(findKey(key,false)==NULL){
			uint64_t sizeBefore=m_auxiliaryTableForIncrementalResize->m_utilisedBuckets;
			VALUE*e=m_auxiliaryTableForIncrementalResize->insert(key);
			if(m_auxiliaryTableForIncrementalResize->m_utilisedBuckets>sizeBefore){
				m_size++;
			}
			return e;
		}
	}

	// Case 3. Otherwise, the old one is responsible. 
	//
	
	#ifdef ASSERT
	uint64_t beforeSize=m_utilisedBuckets;
	#endif

	/** find the group */
	uint64_t probe;
	int group;
	int bucketInGroup;
	findBucketWithKey(key,&probe,&group,&bucketInGroup);

	#ifdef ASSERT
	assert(group<m_numberOfGroups);
	assert(bucketInGroup<m_numberOfBucketsInGroup);
	#endif

	/* actually insert something somewhere */
	bool inserted=false;
	VALUE*entry=m_groups[group].insert(m_numberOfBucketsInGroup,bucketInGroup,key,&m_allocator,&inserted);

	/* check that nothing failed elsewhere */
	#ifdef ASSERT
	assert(entry!=NULL);
	if(entry->getKey()!=*key)
		cout<<"Pointer: "<<entry<<endl;
	assert(entry->getKey()==*key);
	assert(m_groups[group].find(bucketInGroup,key,&m_allocator)!=NULL);
	assert(m_groups[group].find(bucketInGroup,key,&m_allocator)->getKey()==*key);
	#endif

	/* increment the elements if an insertion occured */
	if(inserted){
		m_utilisedBuckets++;
		m_size++;
		/* update the maximum number of probes */
		if(probe>(MAX_SAVED_PROBE-1))
			probe=(MAX_SAVED_PROBE-1);
		m_probes[probe]++;
	}

	#ifdef ASSERT
	assert(find(key)!=NULL);
	assert(find(key)->getKey()==*key);
	if(inserted)
		assert(m_utilisedBuckets==beforeSize+1);
	#endif


	/* check the load factor */
	growIfNecessary();
	
	return entry;
}

/** destroy the hash table */
template<class KEY,class VALUE>
void MyHashTable<KEY,VALUE>::destructor(){
	m_allocator.destructor();
	__Free(m_groups,m_mallocType,m_showMalloc);
}

/**
 * print handy statistics 
 */
template<class KEY,class VALUE>
void MyHashTable<KEY,VALUE>::printStatistics(){
	//m_allocator.print();
}

template<class KEY,class VALUE>
void MyHashTable<KEY,VALUE>::printProbeStatistics(){

	if(!m_verbose)
		return; /**/

	double loadFactor=(0.0+m_utilisedBuckets)/m_totalNumberOfBuckets*100;
	cout<<"Rank "<<m_rank<<": MyHashTable, BucketGroups: "<<m_numberOfGroups<<", BucketsPerGroup: "<<m_numberOfBucketsInGroup<<", LoadFactor: "<<loadFactor<<"%, OccupiedBuckets: "<<m_utilisedBuckets<<"/"<<m_totalNumberOfBuckets<<endl;
	cout<<"Rank "<<m_rank<<": incremental resizing in progress: ";
	if(m_resizing)
		cout<<"yes";
	else
		cout<<"no";
	cout<<endl;

	cout<<"Rank "<<m_rank<<" ProbeStatistics: ";
	for(int i=0;i<MAX_SAVED_PROBE;i++){
		if(m_probes[i]!=0)
			cout<<" "<<i<<" -> "<<m_probes[i]<<"";
	}

	cout<<endl;

	cout<<"Memory usage"<<endl;

	m_allocator.print();
}

template<class KEY,class VALUE>
void MyHashTable<KEY,VALUE>::completeResizing(){
	while(m_resizing)
		resize();

	#ifdef ASSERT
	assert(m_auxiliaryTableForIncrementalResize == NULL);
	#endif

	if(m_verbose)
		printProbeStatistics();
}

template<class KEY,class VALUE>
bool MyHashTable<KEY,VALUE>::needsToCompleteResizing(){
	return m_resizing;
}

#endif
