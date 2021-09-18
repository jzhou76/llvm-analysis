#include <stdio.h>
#include <stdlib.h>

void _record_obj_range(void *start, size_t size);
void _remove_obj_range(void *p);
void _find_array_size(void *p);

int main(int argc, char *argv[]) {
  int **pi = (int **)malloc(sizeof(int *) * 300);
  _record_obj_range(pi, sizeof(int *) * 300);
  int **pc = (int **)malloc(sizeof(char *) * 500);
  _record_obj_range(pc, sizeof(char *) * 500);

  _find_array_size(pi);
  _find_array_size(pi + 100);
  _find_array_size(pi + 200);
  _find_array_size(pc + 300);

  free(pi);
  _remove_obj_range(pi);
  _find_array_size(pi + 100);

  return 0;
}
