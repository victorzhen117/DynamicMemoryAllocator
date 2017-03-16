/**
 * All functions you make for the assignment must be implemented in this file.
 * Do not submit your assignment with a main function in this file.
 * If you submit with a main function in this file, you will get a zero.
 */
#include "sfmm.h"
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>

/**
 * You should store the head of your free list in this variable.
 * Doing so will make it accessible via the extern statement in sfmm.h
 * which will allow you to pass the address to sf_snapshot in a different file.
 */
sf_free_header* freelist_head = NULL;
int numberOfSBRKs = 0;
void* beginningOfHeap;
void* endOfHeap;
static size_t allocatedBlocks;
static size_t splinterBlocks;
static size_t padding;
static size_t splintering;
static size_t coalesces;
static int maxUtil = 0;

void *sf_malloc(size_t size) {
	if(size >= 4096*4){
		//return NULL and set ERRNO to ERsomething
		errno = ENOMEM;
		return NULL;
	}
	if(freelist_head==NULL){
		//this means that this is the first time calling malloc, start the free list here, but first call sf_sbrk
		//freelist_head = (sf_free_header*) sf_sbrk(1);
		void* freelistheadpointer = sf_sbrk(1);
		numberOfSBRKs++;
		beginningOfHeap = freelistheadpointer;
		void* sffooterpointer = freelistheadpointer;
		freelist_head = (sf_free_header*) freelistheadpointer;
		//sf_footer* tempsffooter  = (sf_footer*) sffotterpointer;
		//add a footer, but first update the header
		freelist_head->next = NULL;
		freelist_head->prev = NULL;
		freelist_head->header.alloc = 0;
		freelist_head->header.splinter = 0;
		freelist_head->header.block_size = 4096 >> 4;
		freelist_head->header.requested_size = 0;
		freelist_head->header.splinter_size = 0;
		freelist_head->header.padding_size = 0;
		//now the footer, access it using math.
		/*
		printf("%d", (int)(4096/sizeof(sf_footer) - 1));
		if(sf_sbrk(0)==tempsffooter + (4096/sizeof(sf_footer))){
			printf("fdsfdsfads");
		}
		*/
		sf_footer* tempsffooter = (sf_footer*) ((char*)sffooterpointer + (freelist_head->header.block_size << 4) - 8);
		//tempsffooter += 4096/sizeof(sf_footer) - 1; //typically the size of the block / side of a footer - 1
		endOfHeap = sf_sbrk(0);
		//now at the footer, update the values
		tempsffooter->alloc = 0;
		tempsffooter->splinter = 0;
		tempsffooter->block_size = 4096 >> 4;
	}

	//traverse through the free block list, and look for the min block size that is > size.
	int minBlockSize = -1;
	sf_free_header* temp = freelist_head;
	sf_free_header* locationOfMallocBlock = NULL;
	while(temp != NULL){
		if((temp->header.block_size<<4 < minBlockSize || minBlockSize == -1 ) && temp->header.block_size<<4 >= (size+16)){
			minBlockSize = (temp -> header.block_size)<<4; //this if statement gets the new block size that is
			//greater than size, but the smallest of the possible set. also give me the location of this block.
			locationOfMallocBlock = temp;
		}
		temp = temp -> next;
	}
	if(locationOfMallocBlock==NULL){
		//this means that there was no room. sbrk. TODO for the things like sbrk going past 4 or shit like
		//asking for more than 9192 bytes in the very first call. probably a while loop.
		//sbrk, then get the last free block, append the information.
		if(numberOfSBRKs==4){
			errno = ENOMEM;
			return NULL;
		}
		sf_sbrk(1);
		numberOfSBRKs++;
		//get the last free block in the linked list.
		sf_free_header* tempvar = freelist_head;
		while(tempvar->next != NULL){
			tempvar = tempvar->next;
		}
		locationOfMallocBlock = tempvar;
		//update the header information and footer.
		tempvar->header.block_size += (4096 >> 4);
		sf_footer* sfffooter = (sf_footer*) ((char*)tempvar + (tempvar->header.block_size << 4) - 8);
		//then, update the footer info.
		sfffooter->block_size = tempvar->header.block_size;
		endOfHeap = (void*) sfffooter;
		void* ptr = sf_malloc(size);
		return ptr;
	}
	//now that i have the location of the malloc block, update the locationofmallocblock, and then update the remaining free block.
	//remember about splinters.
	locationOfMallocBlock->header.alloc = 1;
	locationOfMallocBlock->header.requested_size = size;
	int tempSize = size;
	locationOfMallocBlock->header.padding_size = 0;
	while(tempSize % 16 != 0){
		tempSize ++;
		locationOfMallocBlock->header.padding_size++;
	}
	if(locationOfMallocBlock->header.padding_size!=0){
		padding += locationOfMallocBlock->header.padding_size;
	}
	if(size + locationOfMallocBlock->header.padding_size > maxUtil){
		maxUtil = size + locationOfMallocBlock->header.padding_size;
	}
	//locationOfMallocBlock->header.block_size
	int blocksize = size + locationOfMallocBlock->header.padding_size + 16;
	//printf("%d, %d, %d, %d\n", (int)size, locationOfMallocBlock->header.padding_size, (int)(sizeof(sf_header)), (int)sizeof(sf_footer));
	if((locationOfMallocBlock->header.block_size<<4) - blocksize < 32){
		//the things that need to change are: splinter, block_size, splinter_size.
		locationOfMallocBlock->header.splinter = 1;
		int splintersize = (locationOfMallocBlock->header.block_size<<4) - blocksize;
		locationOfMallocBlock->header.splinter_size = splintersize;
		locationOfMallocBlock->header.block_size = (splintersize + blocksize)>>4;
		//now update the footer.
		sf_footer* tempmallocfooter = (sf_footer*) ((char*)locationOfMallocBlock + (locationOfMallocBlock->header.block_size << 4) - 8);
		//i think im at at the footer. update information
		tempmallocfooter->alloc = 1;
		tempmallocfooter->block_size = (splintersize + blocksize) >> 4;
		tempmallocfooter->splinter = 1;
		splinterBlocks++;
		splintering += splintersize;
	}
	else{
		locationOfMallocBlock->header.block_size = blocksize>>4;
		sf_footer* tempmallocfooter = (sf_footer*) ((char*)locationOfMallocBlock + (locationOfMallocBlock->header.block_size << 4) - 8);
		//i think im at at the footer. update information
		tempmallocfooter->alloc = 1;
		tempmallocfooter->block_size = blocksize>>4;
	}
	allocatedBlocks++;
	//update the header of the next block. TODO make this right with a
	//while loop as long as alloc = 1, continue moving on with the block size
	void* toReturn = (void*) ((char*)locationOfMallocBlock + 8);
	//change the block size of the new free block.
	//HERE
	sf_free_header* newFreeBlock = (sf_free_header*) ((char*)locationOfMallocBlock + (locationOfMallocBlock->header.block_size << 4));
	if(newFreeBlock->header.alloc!=1){
		newFreeBlock->header.block_size = (minBlockSize - (locationOfMallocBlock->header.block_size << 4)) >> 4;
		sf_footer* endOfSecondBlock = (sf_footer*) ((char*)newFreeBlock + (newFreeBlock->header.block_size << 4)-8);
		endOfSecondBlock->block_size = newFreeBlock->header.block_size;
		coalesces++;
		//its  abrand new free block
		newFreeBlock->header.alloc = 0;
		newFreeBlock->header.splinter = 0;
		newFreeBlock->header.requested_size = 0;
		newFreeBlock->header.splinter_size = 0;
		newFreeBlock->header.padding_size = 0;
		if(freelist_head->next == NULL && freelist_head->prev == NULL){
			freelist_head = newFreeBlock;
		}
		else if(freelist_head->next == NULL){
			//near the end.
			sf_free_header* tempp = freelist_head;
			while(tempp->next != locationOfMallocBlock){
				tempp = tempp -> next;
			}
			tempp->next = NULL;
		}
		else if(freelist_head->prev == NULL){
			freelist_head = newFreeBlock;
			freelist_head->prev = NULL;
		}
		else{
			sf_free_header* tempp = freelist_head;
			while(tempp->next != locationOfMallocBlock){
				tempp = tempp -> next;
			}
			tempp->next = newFreeBlock;
			newFreeBlock->prev = tempp;
		}
	}
	else{
		//next block is malloc'd.
		sf_free_header* tempp = freelist_head;
		if(tempp==locationOfMallocBlock){
			freelist_head = freelist_head->next;
		}
		else{
			while(tempp->next != locationOfMallocBlock){
				tempp = tempp -> next;
			}
			if(tempp->next!=NULL){
				tempp->next = tempp->next->next;
				tempp->next->prev = temp;
			}
			else{
				// at the end
				tempp->next = NULL;
			}
		}
	}

	//return the void pointer to the malloc'd block.
	//update the next free blocks information (block size)

	return toReturn;
}

void *sf_realloc(void *ptr, size_t size) {
	if(ptr == NULL){
		return NULL;
	}
	ptr = (void*)((char*)ptr - 8); //now im pointing to the header.
	sf_free_header* headerOfBlock = (sf_free_header*) ptr;
	void* toReturn = (void*)((char*)headerOfBlock + 8);
	if ((headerOfBlock->header.requested_size<<4) == size)
	{
		return (void*)((char*)ptr + 8);
	}
	if((headerOfBlock->header.block_size << 4) > size){
		//shrink
		//if i shrink, that means moving the freelist shit.
		//do nothing if theres a splinter i believe.
		int sizeOfBlock;
		if(size%16!=0){
			sizeOfBlock = size + (16 - size%16) + 16;
		}
		else{
			sizeOfBlock = size + 16;
		}
		int larger, smaller;
		if(sizeOfBlock < (headerOfBlock->header.block_size << 4)){
			larger = (headerOfBlock->header.block_size << 4);
			smaller = sizeOfBlock;
		}
		else{
			larger = sizeOfBlock;
			smaller = (headerOfBlock->header.block_size << 4);
		}
		if(larger < smaller+32){
			//splinter, check to see if it can be attached to the next free block
			//get header of the next block
			int splintsize = size - (headerOfBlock->header.block_size);
			sf_free_header* headerOfNextBlock = (sf_free_header*) ((char*)headerOfBlock + (headerOfBlock->header.block_size << 4));
			int tempBlockSize = headerOfBlock->header.block_size; //tis is for splinter later.
			if(headerOfNextBlock->header.alloc == 0){
				//its free, so actually do something
				int paddingsize = 16 - (size%16);
				if(paddingsize!=0){
					padding += paddingsize;
				}
				headerOfBlock->header.block_size = (size + paddingsize + 16)>>4;
				headerOfBlock->header.padding_size = paddingsize;
				headerOfBlock->header.requested_size = size;
				sf_footer* footerOfBlock = (sf_footer*) ((char*)headerOfBlock + (headerOfBlock->header.block_size << 4) - 8);
				footerOfBlock->block_size = headerOfBlock->header.block_size; //i shrunk it.
				if(tempBlockSize==headerOfBlock->header.block_size){
					//the padding made the block the same. do nothing.
					return (void*)((char*)ptr + 8);
				}
				else{
					//add the remaining splinter onto free block TODO
					sf_footer* megaFooter = (sf_footer*) ((char*)headerOfNextBlock + (headerOfNextBlock->header.block_size << 4) - 8);
					megaFooter->block_size += ((tempBlockSize-headerOfBlock->header.block_size)>>4);
					sf_free_header* megaHeader = (sf_free_header*) ((char*)headerOfBlock + (headerOfBlock->header.block_size << 4));
					megaHeader->header.block_size = megaFooter->block_size;

					sf_free_header* tempp = NULL;
					sf_free_header* temppp = freelist_head;
					while(temppp < megaHeader){
						tempp = temppp;
						temppp = temppp->next;
					}
					if(tempp==NULL){
						megaHeader->next = temppp;
						megaHeader->prev = NULL;
						freelist_head = megaHeader;
					}
					else{
						megaHeader->next = temppp;
						tempp->next = megaHeader;
						temppp->prev = megaHeader;
						megaHeader->prev = tempp;
					}
					splinterBlocks++;
					splintering += splintsize;
					coalesces++;
				}
			}
			else{
				//this means that it was a splinter next to a malloced block. do nothing.
				return (void*)((char*)ptr + 8);
			}
		}
		else{
			//no splinter, just shrink.
			sf_free_header* headerOfNextBlock = (sf_free_header*) ((char*)headerOfBlock + (headerOfBlock->header.block_size << 4));
			int tempBlockSize = headerOfBlock->header.block_size; //tis is for splinter later.
			if(headerOfNextBlock->header.alloc == 0){
				//its free, so actually do something
				sf_free_header* savedValue = (sf_free_header*) ((char*)headerOfBlock + (headerOfBlock->header.block_size<<4));
				int paddingsize = 16 - (size%16);
				if(paddingsize!=0){
					padding += paddingsize;
				}
				headerOfBlock->header.block_size = (size + paddingsize + 16)>>4;
				headerOfBlock->header.padding_size = paddingsize;
				headerOfBlock->header.requested_size = size;
				sf_footer* footerOfBlock = (sf_footer*) ((char*)headerOfBlock + (headerOfBlock->header.block_size << 4) - 8);
				footerOfBlock->block_size = headerOfBlock->header.block_size; //i shrunk it.
				if(tempBlockSize==headerOfBlock->header.block_size){
					//the padding made the block the same. do nothing.
					return (void*)((char*)ptr + 8);
				}
				else{
					//add the remaining info onto the malloc block.
					sf_free_header* appendInfo = (sf_free_header*) ((char*)headerOfBlock + (headerOfBlock->header.block_size<<4));
					appendInfo->header.alloc = 0;
					appendInfo->header.splinter = 0;
					appendInfo->header.splinter_size = 0;
					appendInfo->header.padding_size = 0;
					appendInfo->header.block_size = ((savedValue->header.block_size<<4) + paddingsize + size)>>4;
					//if i wanna debug, test this block size versus tempblocksize - headerofblock->header.blocksize
					//then update footer of appendinfo
					sf_footer* footerOfAppended = (sf_footer*) ((char*)appendInfo + (appendInfo->header.block_size << 4) - 8);
					footerOfAppended->block_size = appendInfo->header.block_size;
					footerOfAppended->alloc = 0;
					footerOfAppended->splinter = 0;
					coalesces++;

					//make the free list point to this.
					sf_free_header* tempp = freelist_head;
					if(tempp==savedValue){
						//special case
						appendInfo->next = freelist_head->next;
						appendInfo->prev = NULL;
						freelist_head = appendInfo;
					}
					else{
						while(tempp->next != savedValue){
							tempp = tempp->next;
						}
						appendInfo->next = tempp->next->next;
						if(tempp->next->next!=NULL){
							tempp->next->next->prev = appendInfo;
						}
						tempp->next = appendInfo;
						appendInfo->prev = tempp;
					}
				}
			}
			else{
				//malloced block next to it. just make it a free thing.
				int paddingsize = 16 - (size%16);
				if(size%16 == 0){
					paddingsize = 0;
				}
				else{
					paddingsize = 16-(size%16);
				}
				headerOfBlock->header.block_size = (size + paddingsize + 16)>>4;
				headerOfBlock->header.padding_size = paddingsize;
				headerOfBlock->header.requested_size = size;
				sf_footer* footerOfBlock = (sf_footer*) ((char*)headerOfBlock + (headerOfBlock->header.block_size << 4) - 8);
				footerOfBlock->block_size = headerOfBlock->header.block_size; //i shrunk it.
				sf_free_header* savedValue = (sf_free_header*) ((char*)headerOfBlock + (headerOfBlock->header.block_size<<4));

				//add the new fragment to the free list.
				sf_free_header* tempp = NULL;
				sf_free_header* temppp = freelist_head;
				while(temppp < savedValue){
					tempp = temppp;
					temppp = temppp->next;
				}
				if(tempp==NULL){
					savedValue->next = temppp;
					savedValue->prev = NULL;
					freelist_head = savedValue;
				}
				else{
					savedValue->next = temppp;
					tempp->next = savedValue;
					temppp->prev = savedValue;
					savedValue->prev = tempp;
				}
			}
		}
	}
	else{
		//enlarge
		int currentSpace = headerOfBlock->header.block_size << 4;
		sf_free_header* asdf = (sf_free_header*)((char*)headerOfBlock + (headerOfBlock->header.block_size<<4));
		if(asdf->header.alloc == 0)
			currentSpace += (asdf->header.block_size << 4);
		if(currentSpace > (size+16)){ //stay in the current block
			int tempBlockSize = headerOfBlock->header.block_size; //tis is for splinter later.
			//this means there is room in the current block. expand, then look for a splinter
			int fakesize = size;
			int paddingsize = 0;
			while(fakesize%16!=0){
				paddingsize++;
				fakesize++;
			}
			if(paddingsize!=0){
				padding += paddingsize;
			}
			//now the size and paddingsize are mod 16.
			headerOfBlock->header.block_size = ((paddingsize + fakesize + 16) >> 4);
			headerOfBlock->header.padding_size = paddingsize;
			headerOfBlock->header.splinter = 0;
			headerOfBlock->header.splinter_size = 0;
			headerOfBlock->header.requested_size = size;

			//sf_footer* footerOfBlock = (sf_footer*)((char*)headerOfBlock + headerOfBlock->header.block_size << 4) - 8);

			sf_footer* footerOfBlock = (sf_footer*) ((char*)headerOfBlock + (headerOfBlock->header.block_size << 4) - 8);
			footerOfBlock->block_size = headerOfBlock->header.block_size;
			footerOfBlock->alloc = 1;
			footerOfBlock->splinter = 0;

			//footerOfBlock->block_size = headerOfBlock->header.block_size;
			if(((headerOfBlock->header.block_size - tempBlockSize)<<4) < 32 && (((headerOfBlock->header.block_size - tempBlockSize)<<4) != 0)){
				int value = tempBlockSize - headerOfBlock->header.block_size;
				int splintsizer = ((headerOfBlock->header.block_size - tempBlockSize)<<4);
				headerOfBlock->header.block_size = ((headerOfBlock->header.block_size << 4) + value ) >> 4;
				headerOfBlock->header.splinter = 1;
				headerOfBlock->header.splinter_size = value;
				//update footer too.
				footerOfBlock = (sf_footer*) ((char*)headerOfBlock + (headerOfBlock->header.block_size << 4) - 8);
				footerOfBlock->block_size = headerOfBlock->header.block_size;
				footerOfBlock->splinter = 1;
				splinterBlocks++;
				splintering += splintsizer;
				coalesces++;
			}
		}
		else{
			toReturn = sf_malloc(size);
			sf_free((void*)((char*)ptr + 8));
		}
	}

	return toReturn;
}

void sf_free(void* ptr) {
	if(ptr == NULL){
		return;
	}
	ptr = (void*)((char*)ptr - 8); //now im pointing to the header.
	sf_free_header* toFree = (sf_free_header*) ptr;
	toFree->header.alloc = 0;
	//footer as well.
	sf_footer* footerToFree = (sf_footer*) ((char*)toFree + (toFree->header.block_size << 4) - 8);
	footerToFree->alloc = 0;
	sf_free_header* coalescer = NULL; //maybe hold the header instead for better search
	sf_free_header* didICoalesce = NULL;

	//coaslescing
	if(ptr == beginningOfHeap){
		//act accordingly: this is the beginning of the heap so just check the footer.
		sf_free_header* toAddToFreeList = (sf_free_header*) ((char*)footerToFree + 8);
		if(toAddToFreeList->header.alloc==0){
			coalesces++;
			sf_footer* tempfooter = (sf_footer*) ((char*)toAddToFreeList + (toAddToFreeList->header.block_size << 4) - 8);
			tempfooter->block_size += toFree->header.block_size;
			toFree->header.block_size = tempfooter->block_size;
			//free list
			if(freelist_head==toAddToFreeList){
				toFree->next = freelist_head->next;
				toFree->prev = NULL;
				freelist_head = toFree;
			}
			else{
				//theres a header before toaddtofreelist
				sf_free_header* toTraverse = freelist_head;
				while(toTraverse->next!=toAddToFreeList){
					toTraverse = toTraverse->next;
				}
				//now set totraverse next to to add to freelist. but first:
				toAddToFreeList->next = toTraverse->next->next;
				toTraverse->next->next->prev = toAddToFreeList;
				toTraverse->next = toAddToFreeList;
				toAddToFreeList->prev = toTraverse;
			}
		}
		else{
			//do nothing. continue
			//free list stuff maybe??
			sf_free_header* xx = freelist_head;
			freelist_head = toFree;
			freelist_head->next = xx;
			freelist_head->next->prev = toFree;
		}
	}
	else if(ptr == endOfHeap){
		//look backwards only!
		sf_footer* toFreeTop = (sf_footer*) ((char*)ptr - 8);
		if(toFreeTop->alloc == 0){
			coalesces++;
			//this is a free block :0
			sf_free_header* tempHeader = (sf_free_header*) ((char*)toFreeTop - (toFreeTop->block_size << 4) + 8);
			tempHeader->header.block_size += toFree->header.block_size;
			footerToFree->block_size = tempHeader->header.block_size;
			//stuff with the free list. jk theres nothing.
		}
		else{
			//do nothing.
			//maybe play around with free blocks.
			sf_free_header* xx = freelist_head;
			while(xx->next != NULL){
				xx = xx->next;
			}
			xx->next = toFree;
			toFree->next = NULL;
			toFree->prev = xx;
		}
	}
	else{
		//look at front and back.
		int coaled = 0;
		sf_footer* toFreeTop = (sf_footer*) ((char*)ptr - 8);
		if(toFreeTop->alloc == 0){
			//this is a free block :0
			sf_free_header* tempHeader = (sf_free_header*) ((char*)toFreeTop - (toFreeTop->block_size << 4) + 8);
			coalescer = tempHeader;
			tempHeader->header.block_size += toFree->header.block_size;
			footerToFree->block_size = tempHeader->header.block_size;
			toFree = tempHeader;
			coalesces++;
			coaled = 1;
		}
		sf_free_header* toAddToFreeList = (sf_free_header*) ((char*)footerToFree + 8);
		if(toAddToFreeList->header.alloc==0){
			//coaslesce, change the header, then the footer.
			if(coalescer==NULL){
				coalescer = toFree;
			}
			didICoalesce = toAddToFreeList;
			sf_footer* tempmallocfooter = (sf_footer*) ((char*)toAddToFreeList + (toAddToFreeList->header.block_size << 4) - 8);
			tempmallocfooter->block_size += toFree->header.block_size;
			toFree->header.block_size = tempmallocfooter->block_size;
			//ff->block_size = toFree->header.block_size;
			if(coaled==0){
				coalesces++;
			}
		}
		if(coalescer!=NULL){
			//means i coalesced
			sf_free_header* tempp = NULL;
			sf_free_header* temppp = freelist_head;
			while(temppp < coalescer){
				tempp = temppp;
				temppp = temppp->next;
			}
			if(tempp==NULL){
				coalescer->next = temppp;
				coalescer->prev = NULL;
				freelist_head = coalescer;
			}
			else{
				if(didICoalesce==NULL){
					coalescer->next = temppp;
					if(temppp != NULL){
						temppp->prev = coalescer;
					}
					tempp->next = coalescer;
					coalescer->prev = tempp;
				}
				else{
					coalescer->next = tempp->next->next;
					if(tempp->next->next != NULL){
						tempp->next->next->prev = coalescer;
					}
					tempp->next = coalescer;
					coalescer->prev = tempp;
				}
			}
			/*
			sf_free_header* temporary = freelist_head;
			if(temporary!=coalescer){
				while(temporary->next != coalescer){
					temporary = temporary->next;
				}
			}
			if(temporary==coalescer){
				if(didICoalesce!=NULL){
					//skip the free block
					temporary->next = temporary->next->next;
					if(temporary->next->next!=NULL){
						temporary->next->prev = temporary;
					}
				}
			}
			else{
				if(didICoalesce==NULL){
					coalescer->next = temporary->next->next;
					if(temporary->next->next != NULL){
						temporary->next->next->prev = coalescer;
					}
				}
				else{
					coalescer->next = temporary->next->next->next;
					if(temporary->next->next->next != NULL){
						temporary->next->next->next->prev = coalescer;
					}
				}
			}

			if(didICoalesce == NULL){
				//coalesced upwards only
				if(temporary->next->next != NULL){
					coalescer->next = temporary->next->next;
					temporary->next->next->prev = coalescer;
					temporary->next = coalescer;
					coalescer->prev = temporary;
				}
				else{
					//be careful of temporary->next->next==null
					coalescer->next = NULL;
					temporary->next = coalescer;
					coalescer->prev = temporary;
				}
			}
			else{
				//coalesced downwards, ignore the free block.
				if(temporary->next->next->next != NULL){
					coalescer->next = temporary->next->next->next;
					temporary->next->next->next->prev = coalescer;
					temporary->next = coalescer;
					coalescer->prev = temporary;
				}
				else{
					//be careful of temporary->next->next==null
					coalescer->next = NULL;
					temporary->next = coalescer;
					coalescer->prev = temporary;
				}
			}*/
		}
		else{
			//i didnt even coalesce. find the block that is right before/after the current tobefreed.
			sf_free_header* temporary = freelist_head;
			while((temporary->next) < toFree && (temporary->next) != NULL){
				//if its null, do something
				//else something normal
				temporary = temporary -> next;
			}
			if(temporary->next==NULL && (temporary < toFree)){
				//this means the free block is at the very end
				temporary->next = toFree;
				toFree->next = NULL;
				toFree->prev = temporary;
			}
			else if(temporary->next==NULL && (temporary > toFree)){
				toFree->next = temporary;
				if(temporary->prev == NULL){
					freelist_head = toFree;
				}
				else{
					temporary->prev->next = toFree;
				}
				toFree->prev = temporary->prev;
				temporary->prev = toFree;
			}
			else{
				/*
				toFree->next = temporary->next->next;
				if(temporary->next->next->prev != NULL){
					temporary->next->next->prev = toFree;
				}
				temporary->next = toFree;
				toFree->prev = temporary;
				*/
				toFree->next = temporary->next;
				temporary->next = toFree;
				toFree->next->prev = toFree;
				toFree->prev = temporary;
			}
		}
	}
	return;
}

int sf_info(info* ptr) {
	//cast it
	if(ptr==NULL){
		return 0;
	}
	info* infoer = (info*)ptr;
	infoer->allocatedBlocks = allocatedBlocks;
	infoer->splinterBlocks = splinterBlocks;
	infoer->padding = padding;
	infoer->splintering = splintering;
	infoer->coalesces = coalesces;
	//calculate peak mem util
	infoer->peakMemoryUtilization = maxUtil/(numberOfSBRKs*4096);
	return 1;
}
