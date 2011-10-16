#include <assert.h>

#include "pkproto.h"
#include "utils.h"

int main(void) {
  assert(utils_test() &&
         pkproto_test());
  return 0;
}

