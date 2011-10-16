#include <assert.h>

#include "pkproto.h"
#include "utils.h"

int main(void) {
  assert(pkproto_test() && utils_test());
  return 0;
}

