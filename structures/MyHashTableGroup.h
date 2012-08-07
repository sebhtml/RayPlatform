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

#ifndef _MyHashTableGroup_hh
#define _MyHashTableGroup_hh

#include <stdint.h>
#include <time.h>
#include <memory/ChunkAllocatorWithDefragmentation.h>
#include <iostream>
#include <assert.h>
using namespace std;

/* debug the hardware stuff (gcc built-ins */
//#define __hardware_debugging_gnuc


/**
 * This is an hash group.
 * MyHashTable contains many MyHashTableGroup.
 * Each MyHashTableGroup is responsible for a slice of buckets.
 * \see http://google-sparsehash.googlecode.com/svn/trunk/doc/implementation.html
 *
 * All operations are constant-time or so
 * \author Sébastien Boisvert
 * \date 2012-08-07 This class was reviewed by David Weese <weese@campus.fu-berlin.de>
 */
template<class KEY,class VALUE>
class MyHashTableGroup{

	#ifdef __hardware_debugging_gnuc

	void __hash_print64(uint64_t a);

	#endif /* __hardware_debugging_gnuc */



	/**
 * 	The VALUEs
 */
	SmartPointer m_vector;

	/**
 * 	the bitmap containing the presence of absence  of a bucket 
 */
	uint64_t m_bitmap;

	/**
 * 	set a bit
 */
	void setBit(int i,uint64_t j);
public:
	void print(ChunkAllocatorWithDefragmentation*allocator,int numberOfBucketsInGroup);

	/** requests the bucket directly
 * 	fails if it is empty
 * 	*/
	VALUE*getBucket(int bucket,ChunkAllocatorWithDefragmentation*a);

/**
 * get the number of occupied buckets before bucket 
 */
	int getBucketsBefore(int bucket);

	int getUsedBuckets();

	int __countBits(uint64_t value);

	/**
 * 	is the bucket utilised by someone else than key ?
 */
	bool bucketIsUtilisedBySomeoneElse(int bucket,KEY*key,ChunkAllocatorWithDefragmentation*allocator);

	/**
 * 	get the bit
 */
	int getBit(int i);

	/** 
 * 	build a group of buckets
 */
	void constructor(int numberOfBucketsInGroup,ChunkAllocatorWithDefragmentation*allocator);

	/**
 * 	insert into the group
 * 	should not be called if the bucket is used by someone else already
 * 	otherwise it *will* fail
 */
	VALUE*insert(int numberOfBucketsInGroup,int bucket,KEY*key,ChunkAllocatorWithDefragmentation*allocator,bool*inserted);

	/**
 * 	find a key
 * 	should not be called if the bucket is used by someone else already
 * 	otherwise it *will* fail
 */
	VALUE*find(int bucket,KEY*key,ChunkAllocatorWithDefragmentation*allocator);

};

/** sets the bit in the correct byte */
template<class KEY,class VALUE>
void MyHashTableGroup<KEY,VALUE>::setBit(int bit,uint64_t value){
/**
 * 	just set the bit to value in m_bitmap
 */
	#ifdef ASSERT
	assert(value==1||value==0);
	#endif
	
	/** use binary and or binary or */

	/* set bit to 1 */
	if(value==1){
		m_bitmap|=(value<<bit);

	/* set bit to 0 */
	}else if(value==0){
		uint64_t filter=1;
		filter<<=bit;
		filter=~filter;
		m_bitmap&=filter;
	}

	/** make sure the bit is OK */
	#ifdef ASSERT
	if(getBit(bit)!=(int)value)
		cout<<"Bit="<<bit<<" Expected="<<value<<" Actual="<<getBit(bit)<<endl;
	assert(getBit(bit)==(int)value);
	#endif
}

/* replace  hard-coded 64 by m_numberOfBucketsInGroup  or numberOfBucketsInGroup 
 * 2012-08-07 - done */
template<class KEY,class VALUE>
void MyHashTableGroup<KEY,VALUE>::print(ChunkAllocatorWithDefragmentation*allocator,int
	numberOfBucketsInGroup){

	cout<<"Group bitmap"<<endl;
	for(int i=0;i<numberOfBucketsInGroup;i++)
		cout<<" "<<i<<":"<<getBit(i);
	cout<<endl;
}

/** gets the bit  */
template<class KEY,class VALUE>
int MyHashTableGroup<KEY,VALUE>::getBit(int bit){
	/* use a uint64_t word because otherwise bits are not correct */
	int bitValue=(m_bitmap<<(63-bit))>>63;

	/**  just a sanity check here */
	#ifdef ASSERT
	assert(bitValue==0||bitValue==1);
	#endif

	return bitValue;
}

/** builds the group of buckets */
template<class KEY,class VALUE>
void MyHashTableGroup<KEY,VALUE>::constructor(int numberOfBucketsInGroup,ChunkAllocatorWithDefragmentation*allocator){
	/** we start with an empty m_vector */
	m_vector=SmartPointer_NULL;

	/* the number of buckets in a group must be a multiple of 8 */
	/* why ? => to save memory -- not to waste memory */
	#ifdef ASSERT
	//assert(numberOfBucketsInGroup%8==0);
	// this is not necessary.
	#endif

	/** set all bits to 0 */
	m_bitmap=0;
}

/**
 * inserts a key in a group
 * if the bucket is already utilised, then its key is key
 * if it is not used, key is added in bucket.
 */
template<class KEY,class VALUE>
VALUE*MyHashTableGroup<KEY,VALUE>::insert(int numberOfBucketsInGroup,int bucket,KEY*key,
	ChunkAllocatorWithDefragmentation*allocator,bool*inserted){

	/* the bucket can not be used by another key than key */
	/* if it would be the case, then MyHashTable would not have sent key here */
	#ifdef ASSERT
	if(bucketIsUtilisedBySomeoneElse(bucket,key,allocator)){
		cout<<"Error, bucket "<<bucket<<" is utilised by another key numberOfBucketsInGroup "<<numberOfBucketsInGroup<<" utilised buckets in group: "<<getUsedBuckets()<<endl;
	}
	assert(!bucketIsUtilisedBySomeoneElse(bucket,key,allocator));
	#endif

	/*  count the number of occupied buckets before bucket */
	int bucketsBefore=getBucketsBefore(bucket);

	/* if the bucket is occupied, then it is returned immediately */
	if(getBit(bucket)==1){
		VALUE*vectorPointer=(VALUE*)allocator->getPointer(m_vector);
		#ifdef ASSERT
		VALUE*value=vectorPointer+bucketsBefore;
		assert(value->getKey()==*key);
		#endif
		return vectorPointer+bucketsBefore;
	}

	/* make sure that it is not occupied by some troll already */
	#ifdef ASSERT
	assert(getBit(bucket)==0);
	#endif

	#ifdef ASSERT
	int usedBucketsBefore=getUsedBuckets();
	#endif

	/* the bucket is not occupied */
	setBit(bucket,1);

	#ifdef ASSERT
	assert(getUsedBuckets()==usedBucketsBefore+1);
	#endif

	/* compute the number of buckets to actually move. */
	int bucketsAfter=0;
	for(int i=bucket+1;i<numberOfBucketsInGroup;i++)
		bucketsAfter+=getBit(i);

	/* will allocate a new vector with one more element */
	int requiredElements=(bucketsBefore+1+bucketsAfter);
	SmartPointer newVector=allocator->allocate(requiredElements);

	VALUE*newVectorPointer=(VALUE*)allocator->getPointer(newVector);
	VALUE*vectorPointer=(VALUE*)allocator->getPointer(m_vector);

	/* copy the buckets before */
	for(int i=0;i<bucketsBefore;i++)
		newVectorPointer[i]=vectorPointer[i];

	/* copy the buckets after */
	for(int i=0;i<bucketsAfter;i++)
		newVectorPointer[bucketsBefore+1+i]=vectorPointer[bucketsBefore+i];

	/* assign the new bucket */
	newVectorPointer[bucketsBefore].setKey(*key);
	
	/* garbage the old vector, ChunkAllocatorWithDefragmentation will reuse it */
	if(m_vector!=SmartPointer_NULL)
		allocator->deallocate(m_vector);

	/* assign the vector */
	m_vector=newVector;

	/* check that everything is OK now ! */
	#ifdef ASSERT
	assert(getBit(bucket)==1);
	if(getBucket(bucket,allocator)->getKey()!=*key){
		cout<<"Expected"<<endl;
		key->print();
		cout<<"Actual"<<endl;
		getBucket(bucket,allocator)->getKey().print();
		
		cout<<"Bucket= "<<bucket<<" BucketsBefore= "<<bucketsBefore<<" BucketsAfter= "<<bucketsAfter<<endl;
	}
	assert(getBucket(bucket,allocator)->getKey()==*key);
	assert(!bucketIsUtilisedBySomeoneElse(bucket,key,allocator));
	#endif

	/** check that we inserted something somewhere actually */
	#ifdef ASSERT
	assert(find(bucket,key,allocator)!=NULL);
	assert(find(bucket,key,allocator)->getKey()==*key);
	#endif

	/** tell the caller that we inserted something  somewhere */
	*inserted=true;

	newVectorPointer=(VALUE*)allocator->getPointer(m_vector);
	return newVectorPointer+bucketsBefore;
}

/** checks that the bucket is not utilised by someone else than key */
template<class KEY,class VALUE>
bool MyHashTableGroup<KEY,VALUE>::bucketIsUtilisedBySomeoneElse(int bucket,KEY*key,
	ChunkAllocatorWithDefragmentation*allocator){
	/** bucket is not used and therefore nobody uses it yet */
	if(getBit(bucket)==0)
		return false;

	/** get the number of buckets before */
	int bucketsBefore=getBucketsBefore(bucket);

	/** check if the key is the same */
	VALUE*vectorPointer=(VALUE*)allocator->getPointer(m_vector);
	return vectorPointer[bucketsBefore].getKey()!=*key;
}

/** get direct access to the bucket 
 * \pre the bucket must be used or the code fails
 * */
template<class KEY,class VALUE>
VALUE*MyHashTableGroup<KEY,VALUE>::getBucket(int bucket,ChunkAllocatorWithDefragmentation*allocator){
	/*  the bucket is not occupied therefore the key does not exist */
	#ifdef ASSERT
	assert(getBit(bucket)!=0);
	if(m_vector==SmartPointer_NULL)
		cout<<"bucket: "<<bucket<<endl;
	assert(m_vector!=SmartPointer_NULL);
	#endif

	/* compute the number of elements before bucket */
	int bucketsBefore=getBucketsBefore(bucket);

	/* return the bucket */
	VALUE*vectorPointer=(VALUE*)allocator->getPointer(m_vector);

	#ifdef ASSERT
	assert(vectorPointer!=NULL);
	#endif

	return vectorPointer+bucketsBefore;
}

/** get the number of occupied buckets before bucket 
 * this is done by summing the bits of all buckets before bucket
 */
template<class KEY,class VALUE>
int MyHashTableGroup<KEY,VALUE>::getBucketsBefore(int bucket){
	
	uint64_t mask=1;
	mask<<=bucket;
	mask-=1;

	return __countBits( m_bitmap & mask );
}

/** finds a key in a bucket  
 * this will *fail* if another key is found.
 * */
template<class KEY,class VALUE>
VALUE*MyHashTableGroup<KEY,VALUE>::find(int bucket,KEY*key,ChunkAllocatorWithDefragmentation*allocator){

	/*  the bucket is not occupied therefore the key does not exist */
	if(getBit(bucket)==0){
		return NULL;
	}

	/** the bucket should contains key at this point */
	#ifdef ASSERT
	assert(!bucketIsUtilisedBySomeoneElse(bucket,key,allocator));
	#endif

	/* compute the number of elements before bucket */
	int bucketsBefore=getBucketsBefore(bucket);

	/** return the pointer m_vector with the offset valued by bucketsBefore */
	VALUE*vectorPointer=(VALUE*)allocator->getPointer(m_vector);
	return vectorPointer+bucketsBefore;
}

#ifdef __hardware_debugging_gnuc
template<class KEY,class VALUE>
void MyHashTableGroup<KEY,VALUE>::__hash_print64(uint64_t a){
	for(int k=63;k>=0;k-=2){
		int bit=a<<(k-1)>>63;
		printf("%i",bit);
		bit=a<<(k)>>63;
		printf("%i ",bit);
	}
	printf("\n");
}
#endif /* __hardware_debugging_gnuc */


/* 
 * get the number of used buckets */
template<class KEY,class VALUE>
int MyHashTableGroup<KEY,VALUE>::getUsedBuckets(){

	return __countBits(m_bitmap);
}

/**
 * could also use _mm_popcnt_u64
 *
 * \see http://www.dalkescientific.com/writings/diary/archive/2011/11/02/faster_popcount_update.html
 */
template<class KEY,class VALUE>
int MyHashTableGroup<KEY,VALUE>::__countBits(uint64_t map){

	#define KNUTH
	//#define CONFIG_SSE_4_2
	//#define CONFIG_HAS_POPCNT

	/** Support is indicated via the CPUID.01H:ECX.POPCNT[Bit 23] flag. (Wikipedia **/
	/* \see http://stackoverflow.com/questions/6427473/how-to-generate-a-sse4-2-popcnt-machine-instruction */
	#if defined (CONFIG_HAS_POPCNT)

	return _mm_popcnt_u64 (map);


	/* this is the default on 64-bit gnu systems */
	/* \see http://gcc.gnu.org/onlinedocs/gcc-3.4.5/gcc/Other-Builtins.html  */
	#elif  defined (__GNUC__) && defined (__WORDSIZE) && __WORDSIZE == 64

	return  __builtin_popcountll( map );

	/* for Microsoft */
	/* \see http://msdn.microsoft.com/en-us/library/bb385231.aspx  */
	#elif defined (_MSC_VER) // should work on _WIN64 or _WIN32

	
	return __popcnt64 ( map ) ;

	
	#elif defined(KNUTH)

	// from http://gcc.gnu.org/bugzilla/show_bug.cgi?id=36041
	
	map = (map & 0x5555555555555555ULL) + ((map >> 1) & 0x5555555555555555ULL);
	map = (map & 0x3333333333333333ULL) + ((map >> 2) & 0x3333333333333333ULL);
	map = (map & 0x0F0F0F0F0F0F0F0FULL) + ((map >> 4) & 0x0F0F0F0F0F0F0F0FULL);
	return (map * 0x0101010101010101ULL) >> 56;


	#else

	// silly implementation, slow.

	int bits=0;
	int i=0;
	while(i<64){
		bits+=((<map<(63-i))>>63);
		i++;
	}

	return bits;

	#endif

	#undef KNUTH
}

#endif /* _MyHashTableGroup_hh */


