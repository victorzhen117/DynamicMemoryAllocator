#include <criterion/criterion.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "sfmm.h"

/**
 *  HERE ARE OUR TEST CASES NOT ALL SHOULD BE GIVEN STUDENTS
 *  REMINDER MAX ALLOCATIONS MAY NOT EXCEED 4 * 4096 or 16384 or 128KB
 */

Test(sf_memsuite, Malloc_an_Integer, .init = sf_mem_init, .fini = sf_mem_fini) {
  int *x = sf_malloc(sizeof(int));
  *x = 4;
  cr_assert(*x == 4, "Failed to properly sf_malloc space for an integer!");
}

Test(sf_memsuite, Free_block_check_header_footer_values, .init = sf_mem_init, .fini = sf_mem_fini) {
  void *pointer = sf_malloc(sizeof(short));
  sf_free(pointer);
  pointer = (char*)pointer - 8;
  sf_header *sfHeader = (sf_header *) pointer;
  cr_assert(sfHeader->alloc == 0, "Alloc bit in header is not 0!\n");
  sf_footer *sfFooter = (sf_footer *) ((char*)pointer + (sfHeader->block_size << 4) - 8);
  cr_assert(sfFooter->alloc == 0, "Alloc bit in the footer is not 0!\n");
}

Test(sf_memsuite, SplinterSize_Check_char, .init = sf_mem_init, .fini = sf_mem_fini){
  void* x = sf_malloc(32);
  void* y = sf_malloc(32);
  (void)y;

  sf_free(x);

  x = sf_malloc(16);

  sf_header *sfHeader = (sf_header *)((char*)x - 8);
  cr_assert(sfHeader->splinter == 1, "Splinter bit in header is not 1!");
  cr_assert(sfHeader->splinter_size == 16, "Splinter size is not 16");

  sf_footer *sfFooter = (sf_footer *)((char*)sfHeader + (sfHeader->block_size << 4) - 8);
  cr_assert(sfFooter->splinter == 1, "Splinter bit in header is not 1!");
}

Test(sf_memsuite, Check_next_prev_pointers_of_free_block_at_head_of_list, .init = sf_mem_init, .fini = sf_mem_fini) {
  int *x = sf_malloc(4);
  memset(x, 0, 0);
  cr_assert(freelist_head->next == NULL);
  cr_assert(freelist_head->prev == NULL);
}

Test(sf_memsuite, Coalesce_no_coalescing, .init = sf_mem_init, .fini = sf_mem_fini) {
  int *x = sf_malloc(4);
  int *y = sf_malloc(4);
  memset(y, 0, 0);
  sf_free(x);

  //just simply checking there are more than two things in list
  //and that they point to each other
  cr_assert(freelist_head->next != NULL);
  cr_assert(freelist_head->next->prev != NULL);
}

//#
//STUDENT UNIT TESTS SHOULD BE WRITTEN BELOW
//DO NOT DELETE THESE COMMENTS
//#

Test(sf_memsuite, Testing_malloc_at_same_location, .init = sf_mem_init, .fini = sf_mem_fini) {
  int *x = sf_malloc(4);
  void*xx = x;
  sf_malloc(4);
  sf_free(x);
  int *yy = sf_malloc(4);

  cr_assert(xx==yy, "Malloc isn't at the location it's supposed to be in");
}

Test(sf_memsuite, Bunch_of_mallocs_and_frees_for_freehead, .init = sf_mem_init, .fini = sf_mem_fini) {
  int *x = sf_malloc(8192);
  sf_free_header *xx = freelist_head;
  int *y = sf_malloc(4);
  sf_free(y);
  int *z = sf_malloc(4);
  int *a = sf_malloc(2000);
  sf_free(z);
  sf_free(a);
  sf_free(x);
  sf_malloc(8192);
  sf_free_header *zz = freelist_head;


  cr_assert(zz == xx, "Supposed to be the same address");
}

Test(sf_memsuite, Testing_reallocs, .init = sf_mem_init, .fini = sf_mem_fini) {
  int *x = sf_malloc(8192);
  sf_realloc(x, 50);
  sf_realloc(x, 10000);
  int *xx = sf_realloc(x, 8192);

  //just simply checking there are more than two things in list
  //and that they point to each other
  cr_assert(x == xx, "Supposed to be the same address");
}
