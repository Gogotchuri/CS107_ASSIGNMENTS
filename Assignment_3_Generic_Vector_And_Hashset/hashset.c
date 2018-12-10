#include "hashset.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>

/**
 * Function:  HashSetNew
 * ---------------------
 * Initializes the identified hashset to be empty
 * (Full description of functions in header)
 */
void HashSetNew(hashset *h, int elemSize, int numBuckets,
		HashSetHashFunction hashfn, HashSetCompareFunction comparefn, HashSetFreeFunction freefn){
	assert(h != NULL && elemSize > 0 && numBuckets > 0 && hashfn != NULL && comparefn != NULL);
	h->elem_size = elemSize;
	h->num_buckets = numBuckets;
	h->HashSetHashFunction = hashfn;
	h->HashSetCompareFunction = comparefn;
	h->HashSetFreeFunction = freefn;
	h->elem_count = 0;
	h->buckets = malloc(sizeof(vector) * numBuckets);
	for(int i = 0; i < numBuckets; i++)
		VectorNew(h->buckets + i, elemSize, freefn, 2);//pointer arithmetics rocks!
}


/* Disposing hashset. But before that we apply free function (if client provided one)
 * on every leftover element by disposing each bucket (without abstraction level Vector)*/
void HashSetDispose(hashset *h){
	assert(h != NULL);
	for(int i = 0; i < h->num_buckets; i++)
		VectorDispose(h->buckets + i); 

	free(h->buckets);
}

/*Returns number of elements in hashset*/
int HashSetCount(const hashset *h){
	assert(h != NULL);
	return h->elem_count;
}

/*Typical mapping function. Applies supplied function on each element.*/
void HashSetMap(hashset *h, HashSetMapFunction mapfn, void *auxData){
	assert(h != NULL && mapfn != NULL);
	for(int i = 0; i < h->num_buckets; i++){
		vector * tmp_ptr = h->buckets + i;
		int length = VectorLength(tmp_ptr);
		for(int j = 0; j < length; j++){
			mapfn(VectorNth(tmp_ptr, j), auxData);
		}
	}

}

/**
 * Function: HashSetEnter
 * ----------------------
 * Inserts the specified element into the specified
 * hashset.  If the specified element matches an
 * element previously inserted (as far as the hash
 * and compare functions are concerned), the the
 * old element is replaced by this new element.
 */
void HashSetEnter(hashset *h, const void *elemAddr){
	assert(elemAddr != NULL);
	int buck_num = h->HashSetHashFunction(elemAddr, h->num_buckets);
	assert(buck_num >= 0 && buck_num < h->num_buckets);
	vector * curr_vec = h->buckets + buck_num;
	int old_ind = VectorSearch(curr_vec, elemAddr, h->HashSetCompareFunction, 0, false);
	if(old_ind == -1)
		VectorAppend(curr_vec, elemAddr);
	else{
		VectorDelete(curr_vec, old_ind);
		VectorInsert(curr_vec, elemAddr, old_ind);
	}
	h->elem_count++;
}


void *HashSetLookup(const hashset *h, const void *elemAddr){
	assert(elemAddr != NULL);
	int buck_num = h->HashSetHashFunction(elemAddr, h->num_buckets);
	assert(buck_num >= 0 && buck_num < h->num_buckets);
	vector * curr_vec = h->buckets + buck_num;
	int old_ind = VectorSearch(curr_vec, elemAddr, h->HashSetCompareFunction, 0, false);
	if(old_ind == -1)
		return NULL;

	return VectorNth(curr_vec, old_ind); 
}
