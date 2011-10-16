#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int zero_first_crlf(int length, char* data)
{
  int found = 0;
  while ((length > 0) && !found)
  {
    while (((*data == '\r') || (*data == '\n')) && (length > 0))
    {
      *data++ = '\0';
      length--;
      found++;
    }
    length--;
    data++;
  }
  return found;
}


/**[ TESTS ]******************************************************************/


int utils_test(void)
{
  char buffer1[60], buffer2[60];
  strcpy(buffer1, "abcd\r\n\r\ndefghijklmnop");
  strcpy(buffer2, "abcd\r\n\r\ndefghijklmnop");

  int zeros1 = zero_first_crlf(strlen(buffer1), buffer1);
  int zeros2 = zero_first_crlf(6, buffer2);

  assert(zeros1 == 4);
  assert(zeros2 == 2);
  assert((buffer1[4] == '\0') && (buffer1[7] == '\0'));
  assert((buffer2[4] == '\0') && (buffer2[6] == '\r'));
  assert(strcmp(buffer1, "abcd") == 0);
  assert(strcmp(buffer2, "abcd") == 0);

  return 1;
}

