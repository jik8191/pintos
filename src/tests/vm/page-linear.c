/* Encrypts, then decrypts, 2 MB of memory and verifies that the
   values are as they should be. */

#include <string.h>
#include "tests/arc4.h"
#include "tests/lib.h"
#include "tests/main.h"

#define SIZE (2 * 1024 * 1024)
/* #define SIZE (2 * 1024 * 756) */
/* #define SIZE (2 * 1024 * 768) */

static char buf[SIZE];

void
test_main (void)
{
  struct arc4 arc4;
  size_t i;

  /* Initialize to 0x5a. */
  msg ("initialize");
  memset (buf, 0x5a, sizeof buf);
  /* for (i = 0; i < SIZE; i += 4096) */
  /* { */
  /*     printf("memset for i = %d out of %d\n", i, SIZE); */
  /*     memset (buf + i, 0x5a, 1024); */
  /* } */

  /* Check that it's all 0x5a. */
  msg ("read pass");
  for (i = 0; i < SIZE; i++)
  {
      /* if (i % 1024 == 0) */
        /* printf("reading %d out of %d\n", i, SIZE); */

    if (buf[i] != 0x5a)
      fail ("byte %zu != 0x5a", i);
  }

  /* Encrypt zeros. */
  msg ("read/modify/write pass one");
  arc4_init (&arc4, "foobar", 6);
  arc4_crypt (&arc4, buf, SIZE);

  /* Decrypt back to zeros. */
  msg ("read/modify/write pass two");
  arc4_init (&arc4, "foobar", 6);
  arc4_crypt (&arc4, buf, SIZE);

  /* Check that it's all 0x5a. */
  msg ("read pass");
  for (i = 0; i < SIZE; i++)
    if (buf[i] != 0x5a)
      fail ("byte %zu is %x != 0x5a", i, (unsigned) buf[i]);
}
