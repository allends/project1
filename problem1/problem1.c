#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>
#include <math.h>
#include <time.h>

#define MAXIMUM 100

// selection sort for our program
void swap(int* one, int* two) {
  int temp = *one;
  *one = *two;
  *two = temp;
}
void selectionSort(int* array, int H) {
  int i, j, min_idx;

  for (i=0;i<H-1;i++) {
    min_idx = i;
    for (j=i+1; j<H;j++) {
      if (array[j] < array[min_idx]){
        min_idx = j;
      }
    }
    swap(&array[min_idx], &array[i]);
  }
}

// this will create a file with L lines
// it will also hide the H keys
void generate_file(int L, int H) {
  int* secret_indicies = malloc(H*sizeof(int));
  time_t t;
  srand((unsigned) time(&t));


  // generate unique indecies
  for (int i=0; i<H; i++) {
    secret_indicies[i] = rand() % L;
    int checking = 1;
    while (checking) {
      int conflicting = 0;
      for (int j=0; j < i; j++){
        if (secret_indicies[j] == secret_indicies[i]) {
          conflicting = 1;
        }
      }
      if (conflicting) {
        secret_indicies[i] = rand() % L;
      } else {
        checking = 0;
      }
    }
  }

  // sort this array 
  selectionSort(secret_indicies, H);

  FILE* file;
  file = fopen("secrets.txt",  "w+");
  int secret_index =0;
  for (int i=0; i<L; i++) {
    if (secret_indicies[secret_index] == i) {
      fprintf(file, "-1\n");
      secret_index++;
    } else {
      fprintf(file, "%d\n", rand() % MAXIMUM);
    }
  }
  fclose(file);
  return;
}

// Use one process to find the average and the secrets
void one_process(int PN) {
  int* secret_indicies = malloc(5 * sizeof(int));
  int secret_index = 0;
  int total = 0;
  int line_count =0;

  FILE *file = fopen("secrets.txt", "r");
  char line[1024];
  while (fscanf(file, "%[^\n ] ", line) > 0) {
    int current = atoi(line);
    if (current == -1) {
      secret_indicies[secret_index] = line_count;
      secret_index++;
    } else {
      total += current;
    }
    line_count++;
  }
  printf("line count %d total %d\n", line_count, total);
  for (int i=0; i<5; i++) {
    printf("secret found at index %d\n", secret_indicies[i]);
  }
}

// we have to keep track of each processes number.
// 

// Spawn N more processes and then search a section of the file
void spawn_processes(int NP, int max_children, pid_t main_pid, int writingpipe) {
  int processes_left = getpid() - main_pid;
  int child_num = 0;
  int** pipes = malloc(sizeof(int*)*max_children);
  for (int i=0; i<max_children; i++) {
    pipes[i] = malloc(sizeof(int[2]));
  }
  char buffer[1024];
  pid_t* pids = malloc(sizeof(pid_t)*max_children);
  
  // create max_children amount of pipes

  do {
    // Child code
    pipe(pipes[child_num]);
    printf("succesfully made the pipe \n");
    if ((pids[child_num] = fork()) == 0 ) {
      pid_t pnum = getpid() - main_pid;
      if (pnum >= NP) {
        break;
      } 
      close(pipes[child_num][0]);
      printf("I am child process %d and my parent is %d \n", pnum, getppid());
      spawn_processes(NP, max_children, main_pid, pipes[child_num][1]);
      exit(EXIT_SUCCESS);
    }
    child_num++;
    if (child_num >= max_children) {
      break;
    }
  } while (pids[child_num-1] <= NP);

  // look in the file

  // collect the children
  for (int i = 0; i<child_num; i++) {
    // read from the children pipes
    if(pids[child_num] - main_pid < NP) {
      close(pipes[child_num][1]);
      read(pipes[child_num][0], buffer, 6);
      printf("read from the pipe: %s\n", buffer);
    }
    wait(NULL);
  }
  printf("I am the parent process pid: %d \n", getpid());
  // write to the parent pipe
  write(writingpipe, "test", 6);
  free(pids);
  return;
}

int main(int argc, char* argv) {
  pid_t main_pid = getpid();\
  int first[2];
  pipe(first);
  printf("Main pid is %d \n", main_pid);
  int max_children = 3;
  generate_file(25, 5);
  one_process(1);
  spawn_processes(5, max_children, main_pid, first[0]);
  exit(EXIT_SUCCESS);
}