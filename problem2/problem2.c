#include <stdio.h>
#include <sys/types.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <unistd.h>

/* Structures to define the process tree */
struct processInfo {
  char processName; /* A, B, C, D */
  int numChild;
  char children[5];
  int retValue; 
};
/* define the process tree */
struct processInfo process[] = {{'A', 2, {'B', 'C'}, 2},
                                {'B', 1, {'D'}, 4},
                                {'C', 0, {}, 6},
                                {'D', 0, {}, 10}};
char myid = 'A'; /* process id */
int retVal;
void processfunction() {
  int i, j;
  int n = sizeof(process) / sizeof(struct processInfo);
  printf("Started process %c, pid = %d\n", myid, getpid());
  /* Get details of children */
  for (i = 0; i < n; i++) {
    if (myid == process[i].processName) /* found process details */
      break;
  }
  if (i < n) { /* entry found */
    /* update return value */
    retVal = process[i].retValue;
    /* create forked children */
    pid_t pids[5];
    for (j = 0; j < process[i].numChild; j++) {
      pids[j] = fork();
      if (pids[j] < 0) {
        printf("Process %c, pid = %d: fork failed\n", myid, getpid());
      }
      if (pids[j] == 0) { /* child process */
        /* update id */
        myid = process[i].children[j];
        /* call the processfunction and return */
        processfunction();
        exit(retVal);
      } else {
        printf("Process %c, pid = %d: forked %c, pid = %d\n", myid, getpid(),
               process[i].children[j], pids[j]);
      }
    }
    /* children forked, wait for children to end */
    printf("Process %c, pid = %d: waiting for children to end\n", myid,
           getpid());
    for (j = 0; j < process[i].numChild; j++) {
      int status;
      if (pids[j] > 0) { /* child successfully forked */
        waitpid(pids[j], &status, 0);
        printf("Process %c, pid = %d: child exited with status %d\n", myid,
               getpid(), WEXITSTATUS(status));
      }
    }
  }
  /* sleep for sometime */
  sleep(10);
  printf("Process %c, pid = %d: ending process\n", myid, getpid());
}
int main() {
  processfunction();
  exit(EXIT_SUCCESS);
}
