//
// Runtime library that assists the analysis passes for the Checked C project.
//

#include <map>
#include <vector>
#include <cstdint>
#include <cassert>
#include <iostream>
#include <mutex>
#include <stdio.h>
#include <unistd.h>
#include <execinfo.h>

#define FOUND_PTR(addr) cout << "Found:\naddr = " << addr << ", size = "\
  << array_sizes.back() << "\n\n"

#if defined __cplusplus
extern "C" {
#endif

using namespace std;

// Use mutex to provide atomic access to global variables.
mutex objs_mutex;
mutex arrays_mutex;
mutex obj_mutex;
mutex summary_mtx;
mutex heap_summary_mtx;

// Address ranges of all live heap objects.
static map<uint64_t, size_t> heap_objs;

// The size of each shared array of pointers.
static vector<size_t> array_sizes;

// Size of largest heap object
static size_t largest_obj = 0;

// Summary for all heap objects.
static struct Heap_Summary {
  size_t alloc_num = 0;
  size_t obj_over_4K = 0;
  size_t mem_access_num = 0;
  size_t mem_access_over_4K = 0;
} heap_summary;

void _remove_obj_range(void *p);

#ifdef DEBUG
//
// print_trace(): Dump the current call stack. Copy & pasted from:
// https://www.gnu.org/software/libc/manual/html_node/Backtraces.html
//
void print_trace (void)
{
  void *array[10];
  char **strings;
  int size, i;

  size = backtrace (array, 10);
  strings = backtrace_symbols (array, size);
  if (strings != NULL)
  {

    const char *filename = "/tmp/callstack.txt";
    FILE *output = fopen(filename, "a");
    fprintf(output, "Obtained %d stack frames.\n", size);
    for (i = 0; i < size; i++)
      fprintf(output, "%s\n", strings[i]);
    fclose(output);
  }

  free (strings);
}
#endif

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

  // Update heap_objs
  unique_lock<mutex> lock(objs_mutex, defer_lock);
  lock.lock();
  heap_objs[(uint64_t)start] = (uint64_t)start + size;
  lock.unlock();

  // Update largest_obj
  unique_lock<mutex> lock1(obj_mutex, defer_lock);
  lock1.lock();
  if (size > largest_obj) largest_obj = size;
  lock1.unlock();

  // Update heap_summary
  unique_lock<mutex> lock2(heap_summary_mtx, defer_lock);
  lock2.lock();
  heap_summary.alloc_num++;
  if (size >= 4096) heap_summary.obj_over_4K++;
  lock2.unlock();

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
  unique_lock<mutex> lock(objs_mutex, defer_lock);
  lock.lock();
  heap_objs.erase((uint64_t)p);
  lock.unlock();
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

  unique_lock<mutex> objs_lock(objs_mutex, defer_lock);
  objs_lock.lock();
  auto range = heap_objs.lower_bound(addr);
  if (range != heap_objs.end()) {
    if (range->first == addr) {
      // Find the starting address of an object.

      unique_lock<mutex> array_lock(arrays_mutex, defer_lock);
      array_lock.lock();
      array_sizes.push_back(range->second - addr);
      array_lock.unlock();
#ifdef DEBUG
      FOUND_PTR(addr);
#endif
    } else {
      if (--range != heap_objs.end() &&
          (addr >= range->first && addr < range->second)) {
        unique_lock<mutex> lock(arrays_mutex, defer_lock);
        lock.lock();
        array_sizes.push_back(range->second - addr);
        lock.unlock();
#ifdef DEBUG
        FOUND_PTR(addr);
#endif
      }
    }
  } else {
    // Addr is greater than the starting address of the obj that has the largest
    // starting address. There are two possibilities. First, the target pointer
    // points to the middle of the last object in the heap_objs. Second, the
    // heap_objs is empty.
    if (heap_objs.empty()) return;

    auto last_obj = heap_objs.rbegin();
    if (last_obj->second > addr) {
      unique_lock<mutex> lock(arrays_mutex, defer_lock);
      lock.lock();
      array_sizes.push_back(last_obj->second - addr);
      lock.unlock();
    }

#ifdef DEBUG
    FOUND_PTR(addr);
#endif
  }
  objs_lock.unlock();
}

//
// Function: _dump_summary()
// This function prints the summarized statistics.
//
void _dump_summary() {
  size_t largest_arr = 0;
  size_t arr_total = 0;
  for (size_t size : array_sizes) {
    arr_total += size;
    if (size > largest_arr) largest_arr = size;
  }

  size_t alloc_num = heap_summary.alloc_num, obj_over_4K = heap_summary.obj_over_4K;

  // Write the result to a temporary file. This file will be processed by
  // a script that runs the experiment.
  const char *filename = "/tmp/analysis_result.stat";
  unique_lock<mutex> lock(summary_mtx, defer_lock);
  lock.lock();
  FILE *output = fopen(filename, "a");
  fprintf(output, "Largest heap object: %zu\n", largest_obj);
  fprintf(output, "Largest shared array of pointers: %zu\n", largest_arr);
  fprintf(output, "Total shared array of pointers: %zu\n", arr_total);

  if (alloc_num) {
    fprintf(output, "Percentage of heap objects greater than 4K: %zu/%zu = %.2f\n",
        obj_over_4K, alloc_num, obj_over_4K * 1.0 / alloc_num);
  } else {
    fprintf(output, "No heap objects allocated\n");
  }

  fclose(output);
  lock.unlock();
}

#if defined __cplusplus
}
#endif
