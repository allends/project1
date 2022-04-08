// C program to illustrate
// pipe system call in C
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#define MSGSIZE 16
char* msg1 = "hello, world #1";
char* msg2 = "hello, world #2";
char* msg3 = "hello, world #3";

int main() {
  char inbuf[MSGSIZE];
  int p[2], i;

  if (pipe(p) < 0) exit(1);

  /* continued */
  /* write pipe */

  for (i = 0; i < 3; i++) {
    /* read pipe */
    if (fork() == 0) {
      close(p[0]);
      write(p[1], msg1, MSGSIZE);
    }
  }

  for (i = 0; i < 3; i++) {
    close(p[1]);
    read(p[0], inbuf, MSGSIZE);
    printf("%s\n", inbuf);
    fflush(stdout);
  }
  return 0;
}