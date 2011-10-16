#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int zero_first_crlf(int length, char* data)
{
  for (int i = 0; i < length-1; i++)
  {
    if ((data[i] == '\r') && (data[i+1] == '\n'))
    {
      data[i] = data[i+1] = '\0';
      return i+2;
    }
  }
  return 0;
}


/**[ TESTS ]******************************************************************/


int utils_test(void)
{
  char buffer1[60];

  strcpy(buffer1, "\r\n\r\n");
  assert(2 == zero_first_crlf(4, buffer1));

  strcpy(buffer1, "abcd\r\n\r\ndefghijklmnop");
  int length = zero_first_crlf(strlen(buffer1), buffer1);

  assert(length == 6);
  assert((buffer1[4] == '\0') && (buffer1[5] == '\0') && (buffer1[6] == '\r'));
  assert(strcmp(buffer1, "abcd") == 0);

  return 1;
}

