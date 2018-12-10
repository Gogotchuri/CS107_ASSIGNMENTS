#include "vector.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <search.h>



//returns true if position is in range of vector positions
bool IsValidPosition(const vector * v, int pos) {
	return !(pos < 0 || pos > VectorLength(v) - 1);
}

/* Tries reallocation of vector, increasing size by initial allocation size.
 * Returns false if space couldn't be allocated*/
bool Expand(vector * v) {
	v->alloc_len += v->init_alloc;
	void * realloced = realloc(v->arr, v->alloc_len * v->elem_size);
	if (realloced == NULL) return false;
	v->arr = realloced;
	return true;
}


/*Shifts vector's (num) elems started from passed address;

	@param v (as self to proceed operation)
	@param from address of first element which should be shifted
	@param num number of element (0 - no elements is going to be moved)
	@param by number of vector elements to move by, (if passed -1 elements will be
	moved to previous indexes, if by = 1 next indexes and so on)
*/
void shift(vector * v, void * from, int num, int by) {
	if (by == 0 || num == 0) return; //nothing to move
	void * dest_addr;
	void * src;
	int dir;

	//There will be overlapping otherwise. we need to make sure,
	//in case we need to move elems forward we should start
	//from the last one and move to the start
	//otherwise proceed normally
	if (by < 0) {
		src = from;
		dir = 1;
	}
	else {
		//shifting backwards
		src = (char*)from + v->elem_size * (num - 1);
		dir = -1;
	}

	for (int i = 0; i < num; i++) {
		dest_addr = (char*)src + by * v->elem_size;//calculating address to copy to
		memcpy(dest_addr, src, v->elem_size);
		src = (char*)src + dir*v->elem_size;
	}
}

/*Basic constructor, more details in header*/
void VectorNew(vector *v, int elemSize, VectorFreeFunction freeFn, int initialAllocation) {
	v->init_alloc = (initialAllocation != 0) ? initialAllocation : 10;
	v->alloc_len = v->init_alloc;
	v->elem_size = elemSize;
	v->log_len = 0;
	v->arr = malloc(v->alloc_len * elemSize);
	v->VectorFreeFunction = freeFn;

}

void VectorDispose(vector *v) {
	if (v->VectorFreeFunction != NULL)
		for (int i = 0; i < v->log_len; i++)
			v->VectorFreeFunction(VectorNth(v, i));

	free(v->arr);
}

int VectorLength(const vector *v) {
	return v->log_len;
}

void *VectorNth(const vector *v, int position) {
	assert(IsValidPosition(v, position));
	return ((char*)v->arr + (v->elem_size * position));
}

void VectorReplace(vector *v, const void *elemAddr, int position) {
	void * old_elem = VectorNth(v, position);//if wrong pos assert is in foo; 
	if (v->VectorFreeFunction != NULL)
		v->VectorFreeFunction(old_elem);
	memcpy(old_elem, elemAddr, v->elem_size);
}

void VectorInsert(vector *v, const void *elemAddr, int position) {
	//checking size and checking whether reallocation went as planned
	if (VectorLength(v) == v->alloc_len) assert(Expand(v));
	if (VectorLength(v) == position) {
		VectorAppend(v, elemAddr);
		return;
	}
	void * dest = VectorNth(v, position);
	int num_elems_after = VectorLength(v) - position;
	shift(v, dest, num_elems_after, 1);//making space for insertion
	memcpy(dest, elemAddr, v->elem_size);
	v->log_len++;

}

void VectorAppend(vector *v, const void *elemAddr) {
	//checking size and checking whether reallocation went as planned
	if (VectorLength(v) == v->alloc_len) assert(Expand(v));
	void * dest = v->arr;//appending to empty vector
	if (VectorLength(v) > 0)
		dest = (char*)VectorNth(v, VectorLength(v) - 1) + v->elem_size;

	memcpy(dest, elemAddr, v->elem_size);
	v->log_len++;
}

void VectorDelete(vector *v, int position) {
	void * to_delete = VectorNth(v, position); //asserts if pos isn't in range
	
	if(v->VectorFreeFunction != NULL)
		v->VectorFreeFunction(to_delete);

	int num = VectorLength(v) - position - 1; // elements after deleted elem
											  //to move every elem in range[from, from + (num - 1)*elem_size] by 'by' elem;
	shift(v, (char*)to_delete + v->elem_size, num, -1);//cleaning up empty space
	v->log_len--;

}

void VectorSort(vector *v, VectorCompareFunction compare) {
	assert(compare != NULL);
	if (VectorLength(v) != 0) qsort(v->arr, v->log_len, v->elem_size, compare);
}

void VectorMap(vector *v, VectorMapFunction mapFn, void *auxData){
	assert(mapFn != NULL);
	for (int i = 0; i < VectorLength(v); i++)
		mapFn((char*)v->arr + v->elem_size*i, auxData);

}


int VectorSearch(const vector *v, const void *key, VectorCompareFunction searchFn, int startIndex, bool isSorted){
	if(startIndex == 0 && VectorLength(v) == 0) return -1;
	assert(IsValidPosition(v, startIndex) && key != NULL && searchFn != NULL);
	void * found_addr = NULL;
	long unsigned int num_elems = VectorLength(v) - startIndex; 
	
	if (isSorted)
		found_addr = bsearch(key, (char*)v->arr + v->elem_size*startIndex, num_elems, v->elem_size, searchFn);
	else 
		found_addr = lfind(key, (char*)v->arr + v->elem_size*startIndex, &num_elems, v->elem_size, searchFn);
	
	int res = (found_addr == NULL)? -1 : ((char*)found_addr - (char*)v->arr) / v->elem_size;

	return res;
}