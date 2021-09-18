//
// Runtime library that assists the analysis passes for the Checked C project.
//

#include <map>
#include <vector>
#include <cstdint>
#include <cassert>
#include <iostream>

#define DEBUG
#define FOUND_PTR(addr) cout << "Found:\naddr = " << addr << ", size = "\
  << array_sizes.back() << "\n\n"

#if defined __cplusplus
extern "C" {
#endif

using namespace std;

// Address ranges of all live heap objects.
static map<uint64_t, uint64_t> heap_objs;

// The size of each shared array of pointers.
vector<size_t> array_sizes;

//
// Function: _record_obj_range().
// This function records the address range of a heap object.
//
// @param - start: starting address of the object
// @pram - size: size of the object
//
void _record_obj_range(void *start, size_t size) {
#ifdef DEBUG
  cout << "Adding range: " << (uint64_t)start << " - " << (uint64_t)start + size << "\n";
#endif
  heap_objs[(uint64_t)start] = (uint64_t)start + size;
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
// This function looks for the size of a shared object based on a pointer
// to the object. The pointer does not need to point to the beginning of
// the object.
//
void _find_array_size(void *p) {
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

#if defined __cplusplus
}
#endif
