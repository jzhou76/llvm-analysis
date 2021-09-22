//
// Runtime library that assists the analysis passes for the Checked C project.
//

#include <map>
#include <vector>
#include <cstdint>
#include <cassert>
#include <iostream>
#include <stdio.h>

#define FOUND_PTR(addr) cout << "Found:\naddr = " << addr << ", size = "\
  << array_sizes.back() << "\n\n"

#if defined __cplusplus
extern "C" {
#endif

using namespace std;

// Address ranges of all live heap objects.
static map<uint64_t, uint64_t> heap_objs;

// The size of each shared array of pointers.
static vector<size_t> array_sizes;

// Size of largest heap object
static size_t largest_obj = 0;

void _remove_obj_range(void *p);

//
// Function: _record_alloc()
// This function records the address range of a heap object allocated by
// malloc or calloc. It is also used by _record_realloc() for certain cases.
//
// @param - start: starting address of the object
// @param - size: size of the object
//
void _record_alloc(void *start, size_t size) {
  if (start == nullptr) return;  // Allocation failed.

  heap_objs[(uint64_t)start] = (uint64_t)start + size;
  if (size > largest_obj) largest_obj = size;

#ifdef DEBUG
  cout << "Adding range: " << (uint64_t)start << " - " << (uint64_t)start + size << "\n";
#endif
}

//
// Function: _record_realloc()
// This functions records the address range of a heap object allocated by
// realloc() and reallocarray().
//
// @param - oldp: the ptr argument passed to realloc() or reallocarray().
// @param - newp: the new pointer returned by realloc() or reallocarray().
// @param - size: the allocated size.
//
void _record_realloc(void *oldp, void *newp, size_t size) {
  if (oldp == nullptr) {
    _record_alloc(newp, size);
#ifdef DEBUG
    printf("old ptr to realloc() is null\n");
#endif
  } else {
    if (size == 0) {
      _remove_obj_range(oldp);
    } else {
      if (oldp != newp && newp != nullptr) {
        _remove_obj_range(oldp);
#ifdef DEBUG
        printf("realloc fress old ptr (%p) and allocates a new one (%p).\n", oldp, newp);
#endif
      }
      _record_alloc(newp, size);
    }
  }
}

//
// Function: _remove_obj_range()
// This function removes the range of a heap object from the database.
//
// @param - p: starting address of the object
//
void _remove_obj_range(void *p) {
  heap_objs.erase((uint64_t)p);
}

//
// Function: _find_array_size()
// This function gets the size of a shared object based on a pointer
// to the object. The pointer does not need to point to the beginning of
// the object.
//
void _cal_array_size(void *p) {
  uint64_t addr = (uint64_t)p;

#ifdef DEBUG
  cout << "Looking for " << addr << "\n";
#endif

  auto range = heap_objs.lower_bound(addr);
  if (range != heap_objs.end()) {
    if (range->first == addr) {
      // Find the starting address of an object.
      array_sizes.push_back(range->second - addr);
#ifdef DEBUG
      FOUND_PTR(addr);
#endif
    } else {
      if (--range != heap_objs.end() &&
          (addr >= range->first && addr < range->second)) {
        array_sizes.push_back(range->second - addr);
#ifdef DEBUG
        FOUND_PTR(addr);
#endif
      }
    }
  } else {
    // Addr is greater than the starting address of the obj that has the largest
    // starting address.
    range--;
    array_sizes.push_back(range->second - addr);

#ifdef DEBUG
    FOUND_PTR(addr);
#endif
  }
}

//
// Function: _dump_summary()
// This function prints the summarized statistics.
//
void _dump_summary() {
  size_t largest_arr = 0;
  for (size_t size : array_sizes) {
    if (size > largest_arr) largest_arr = size;
  }

  // Write the result to a temporary file. This file will be processed by
  // a script that runs the experiment.
  const char *filename = "/tmp/analysis_result.txt";
  FILE *output = fopen(filename, "a");
  fprintf(output, "Largest heap object: %lu\n", largest_obj);
  fprintf(output, "Largest shared array of pointers: %lu\n", largest_arr);
  fclose(output);
}

#if defined __cplusplus
}
#endif
