#include <stdio.h>
#include <stdlib.h>
#include "pkproto.h"
#include "utils.h"

int main(void) {
  return !(pkproto_test() &&
           utils_test());
}

