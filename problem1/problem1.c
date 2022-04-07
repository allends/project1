#include <math.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#define MAXIMUM 4000
#define BUFFERSIZE 1024

// selection sort for our program
void swap(int *one, int *two) {
  int temp = *one;
  *one = *two;
  *two = temp;
}
void selectionSort(int *array, int H) {
  int i, j, min_idx;

  for (i = 0; i < H - 1; i++) {
    min_idx = i;
    for (j = i + 1; j < H; j++) {
      if (array[j] < array[min_idx]) {
        min_idx = j;
      }
    }
    swap(&array[min_idx], &array[i]);
  }
}

// this will create a file with L lines
// it will also hide the H keys
void generate_file(int L, int H) {
  int *secret_indicies = malloc(H * sizeof(int));
  time_t t;
  srand((unsigned)time(&t));

  // generate unique indecies
  for (int i = 0; i < H; i++) {
    secret_indicies[i] = rand() % L;
    int checking = 1;
    while (checking) {
      int conflicting = 0;
      for (int j = 0; j < i; j++) {
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

  FILE *file;
  file = fopen("secrets.txt", "w+");
  int secret_index = 0;
  for (int i = 0; i < L; i++) {
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
void one_process() {
  int *secret_indicies = malloc(5 * sizeof(int));
  int secret_index = 0;
  int total = 0;
  int line_count = 0;
  int maximum = 0;
  FILE *output = fopen("output.txt", "w+");

  clock_t start, end;
  double cpu_time_used;

  start = clock();
  FILE *file = fopen("secrets.txt", "r");
  char line[BUFFERSIZE];
  while (fscanf(file, "%[^\n ] ", line) > 0) {
    int current = atoi(line);
    if (current == -1) {
      secret_indicies[secret_index] = line_count;
      secret_index++;
    } else {
      total += current;
      if (maximum < current) {
        maximum = current;
      }
    }
    line_count++;
  }
  end = clock();
  cpu_time_used = ((double)(end - start) / CLOCKS_PER_SEC);
  int average = total / line_count;
  fprintf(output, "Max: %d Avg: %d \n", maximum, average);
  for (int i = 0; i < 5; i++) {
    fprintf(output, "Single process found a key at line: %d.\n",
            secret_indicies[i] + 1);
  }
  printf("Time used for one process: %2f\n", cpu_time_used);
  fprintf(output, "+---------------------------------+\n");
  fclose(output);
  fclose(file);
}

// we have to keep track of each processes number.
//

// Spawn N more processes and then search a section of the file
void spawn_processes(int NP, int num_lines, pid_t p_num, int max_children,
                     pid_t main_pid, int writingpipe, char **lines,
                     FILE *output) {
  int processes_left = getpid() - main_pid;
  int child_num = 0;
  int **pipes = malloc(sizeof(int *) * max_children);
  for (int i = 0; i < max_children; i++) {
    pipes[i] = malloc(sizeof(int[2]));
  }
  char buffer[BUFFERSIZE];
  pid_t *pids = malloc(sizeof(pid_t) * max_children);

  // create max_children amount of pipes
  do {
    // Child code
    pipe(pipes[child_num]);
    if ((pids[child_num] = fork()) == 0) {
      pid_t pnum = getpid() - main_pid;
      pid_t ppnum = getppid() - main_pid;
      fprintf(output, "Hi, I'm process %d and my parent is %d.\n", pnum, ppnum);
      fflush(output);
      if (pnum >= NP) {
        break;
      }
      close(pipes[child_num][0]);
      spawn_processes(NP, num_lines, pnum, max_children, main_pid,
                      pipes[child_num][1], lines, output);
      exit(EXIT_SUCCESS);
    }
    child_num++;
    if (child_num >= max_children) {
      break;
    }
  } while (pids[child_num - 1] <= NP);
  
  // look in the file
  int linesperchild = num_lines / NP;
  int total = 0;
  int local_max = 0;
  int i;
  for (i = p_num * linesperchild; i < p_num * linesperchild + linesperchild;
       i++) {
    int temp = atoi(lines[i]);
    if (temp != -1) {
      if (local_max < atoi(lines[i])) {
        local_max = atoi(lines[i]);
      }
      total += atoi(lines[i]);
    } else {
      fprintf(output, "Process %d found a key at line: %d.\n", p_num, i + 1);
      fflush(output);
    }
  }
  while (p_num == NP - 1 && i < num_lines) {
    int temp = atoi(lines[i]);
    if (temp != -1) {
      if (local_max < atoi(lines[i])) {
        local_max = atoi(lines[i]);
      }
      total += atoi(lines[i]);
    } else {
      fprintf(output, "Process %d found a key at line: %d.\n", p_num, i + 1);
    }
    i++;
  }

  // collect the children
  for (int i = 0; i < child_num; i++) {
    // read from the children pipes
    if (pids[i] - main_pid < NP) {
      close(pipes[i][1]);
      read(pipes[i][0], buffer, sizeof(buffer));
      int child_value = atoi(buffer);
      total += child_value;
      read(pipes[i][0], buffer, sizeof(buffer));
      int child_max = atoi(buffer);
      if (local_max < child_max) {
        local_max = child_max;
      }
    }
    wait(NULL);
  }
  // write to the parent pipe
  char sendbuff[BUFFERSIZE];
  sprintf(sendbuff, "%d", total);
  write(writingpipe, sendbuff, sizeof(sendbuff));

  sprintf(sendbuff, "%d", local_max);
  write(writingpipe, sendbuff, sizeof(sendbuff));
  free(pids);
  free(pipes);
  if (getpid() != main_pid)
    exit(EXIT_SUCCESS);
}

void multiple_processes(int NP, int num_lines, int max_children) {
  pid_t main_pid = getpid();
  FILE *output = fopen("output.txt", "a");

  int first[2];
  pipe(first);
  fprintf(output, "Hi, I'm process %d and my parent had pid %d.\n",
          getpid() - main_pid, getppid());
  fflush(output);

  // read the file and make an array with each index as a line of the file
  char **lines = malloc(sizeof(char *) * num_lines);
  FILE *file = fopen("secrets.txt", "r");
  for (int i = 0; i < num_lines; i++) {
    lines[i] = malloc(sizeof(char[BUFFERSIZE]));
    fscanf(file, "%[^\n ] ", lines[i]);
  }
  fclose(file);

  clock_t start, end;
  double cpu_time_used;

  start = clock();
  spawn_processes(NP, num_lines, 0, max_children, main_pid, first[1], lines,
                  output);
  end = clock();
  cpu_time_used = ((double)(end - start) / CLOCKS_PER_SEC);
  printf("Time used for %d processes: %2f\n", NP, cpu_time_used);
  fflush(stdout);
  char buffer[BUFFERSIZE];
  close(first[1]);
  read(first[0], buffer, sizeof(buffer));
  int total = atoi(buffer);
  read(first[0], buffer, sizeof(buffer));
  int maximum = atoi(buffer);
  int average = total / num_lines;
  fprintf(output, "Max: %d Avg: %d \n", maximum, average);
  fflush(output);
  fclose(output);
}

int main(int argc, char *argv) {
  int NP = 50;
  int num_lines = 50000;
  int max_children = 3;
  generate_file(num_lines, 5);

  one_process();
  multiple_processes(NP, num_lines, max_children);
  exit(EXIT_SUCCESS);
}