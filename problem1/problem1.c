#include <math.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#define MAXIMUM 4000
#define BUFFERSIZE 1024
int key_count = 0;
int child_no;
// -------------------------------------------------
// Signal Section
void increment_sig(int signo) { key_count++; }

void child_exit(int signo) { exit(EXIT_SUCCESS); }

// -------------------------------------------------
// Helper Section and File Generation

// selection sort for our program
void swap(int *one, int *two) {
  int temp = *one;
  *one = *two;
  *two = temp;
}

// Selection sort to sort the number of keys to make it easier to replace later
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

  // array used to store the key locations
  int *secret_indicies = malloc(H * sizeof(int));
  time_t t;
  srand((unsigned)time(&t));

  // generate unique indecies
  // these are used to place the keys later
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
  // then we can replace them in order
  selectionSort(secret_indicies, H);

  // Create the  file for secrets
  // Write to the secrets file 
  FILE *file;
  file = fopen("secrets.txt", "w+");
  int secret_index = 0;
  for (int i = 0; i < L; i++) {
    // if the index is a secret index
    // make it -1 and increment the secret_index counter
    // else make it a random number (maximum value is MAXIMUM)
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

// -----------------------------------------------------------------
// One process section

// Use one process to find the average and the secrets
void one_process() {
  // array to store where the secrets are 
  int *secret_indicies = malloc(5 * sizeof(int));
  int secret_index = 0;
  int total = 0;
  int line_count = 0;
  int maximum = 0;

  // The output file that we want to write to
  // Open for writing/reading
  FILE *output = fopen("output.txt", "w+");

  clock_t start, end;
  double cpu_time_used;

  // Start the timer so we know how long it takes
  start = clock();
  FILE *file = fopen("secrets.txt", "r");
  char line[BUFFERSIZE];

  // Scan each line until the end of the file
  while (fscanf(file, "%[^\n ] ", line) > 0) {
    int current = atoi(line);

    // Routine if we encounter a secret
    if (current == -1) {
      secret_indicies[secret_index] = line_count;
      secret_index++;
    } else {

      // Routine the collect the total and keep track of max
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

  // Display what we found
  fprintf(output, "Max: %d Avg: %d \n", maximum, average);
  for (int i = 0; i < 5; i++) {
    fprintf(output, "Single process found a key at line: %d.\n",
            secret_indicies[i] + 1);
  }
  fprintf(output, "Time used for one process: %2f\n", cpu_time_used);
  fprintf(output, "+---------------------------------+\n");
  fclose(output);
  fclose(file);
}

// -----------------------------------------------------------------------------
// Multiple Processes Section

// Spawn N more processes and then search a section of the file
void spawn_processes(int NP, int num_lines, pid_t p_num, int max_children,
                     pid_t main_pid, int writingpipe, char **lines,
                     FILE *output) {
  int processes_left = getpid() - main_pid;
  int child_num = 0;

  // Data structure that holds all of the pipes
  int **pipes = malloc(sizeof(int *) * max_children);
  for (int i = 0; i < max_children; i++) {
    pipes[i] = malloc(sizeof(int[2]));
  }
  char buffer[BUFFERSIZE];
  pid_t *pids = malloc(sizeof(pid_t) * max_children);

  // Create max_children amount of pipes
  do {
    // Child code
    pipe(pipes[child_num]);
    if ((pids[child_num] = fork()) == 0) {
      // Get PID through difference of process numbers
      pid_t pnum = getpid() - main_pid;
      pid_t ppnum = getppid() - main_pid;
      fprintf(output, "Hi, I'm process %d and my parent is %d.\n", pnum, ppnum);
      fflush(output);
      if (pnum >= NP) {
        break;
      }
      // Close the childs end of the reading pipe
      close(pipes[child_num][0]);

      // Spawm N more processes
      spawn_processes(NP, num_lines, pnum, max_children, main_pid,
                      pipes[child_num][1], lines, output);

      // Exit
      exit(EXIT_SUCCESS);
    }
    child_num++;

    // Check if we want to actually make more children
    if (child_num >= max_children) {
      break;
    }
  } while (pids[child_num - 1] <= NP);

  // Read in the childs section of the file
  int linesperchild = num_lines / NP;
  int total = 0;
  int local_max = 0;
  int i;

  // Loop over the childs section (defined by linesperchild and its process number)
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

  // This can happen if num_lines % NP != 0
  // The last child has to pick up the slack
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

  // collect the children that this process spawned
  for (int i = 0; i < child_num; i++) {
    // read from the children pipes
    if (pids[i] - main_pid < NP) {

      // close the parents writing end of the pipe
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
  // this is the data that it found
  char sendbuff[BUFFERSIZE];
  sprintf(sendbuff, "%d", total);
  write(writingpipe, sendbuff, sizeof(sendbuff));

  sprintf(sendbuff, "%d", local_max);
  write(writingpipe, sendbuff, sizeof(sendbuff));
  free(pids);
  free(pipes);
  if (getpid() != main_pid) exit(EXIT_SUCCESS);
}

// Helper function to start the spawn_processes() funciton
void multiple_processes(int NP, int num_lines, int max_children) {
  pid_t main_pid = getpid();
  FILE *output = fopen("output.txt", "a");

  // the main pipe that sends info to the one process that will-
  // print to stdout
  int first[2];
  pipe(first);
  fprintf(output, "Hi, I'm process %d and my parent had pid %d.\n",
          getpid() - main_pid, getppid());
  fflush(output);

  // read the file and make an array with each index as a line of the file
  char **lines = malloc(sizeof(char *) * num_lines);
  FILE *file = fopen("secrets.txt", "r");
  for (int i = 0; i < num_lines; i++) {
    // allocate each index with some memory
    lines[i] = malloc(sizeof(char[BUFFERSIZE]));
    fscanf(file, "%[^\n ] ", lines[i]);
  }
  fclose(file);

  clock_t start, end;
  double cpu_time_used;

  // Start timing the processes
  start = clock();
  spawn_processes(NP, num_lines, 0, max_children, main_pid, first[1], lines,
                  output);
  end = clock();
  cpu_time_used = ((double)(end - start) / CLOCKS_PER_SEC);

  // Report anything we need to report
  fprintf(output, "Time used for %d processes: %2f\n", NP, cpu_time_used);
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

// --------------------------------------------------------------------------
// Section for first N keys search
void n_keys_spawn(int N, int NP, int num_lines, pid_t p_num, pid_t main_pid,
                  char **lines, FILE *output) {
  int process_count = getpid() - main_pid;
  // check if we want to store more processes
  // spawn 1 more child if we are allowed to spawn more
  if (process_count < NP - 1) {
    if (fork() == 0) {
      // child
      n_keys_spawn(N, NP, num_lines, getpid() - main_pid, main_pid, lines,
                   output);
      signal(SIGUSR2, child_exit);
    }
  }

  // look in the file
  int linesperchild = num_lines / NP;
  int total = 0;
  int local_max = 0;
  int i;
  for (i = p_num * linesperchild; i < p_num * linesperchild + linesperchild;
       i++) {
    int temp = atoi(lines[i]);
    // Send a signal to the main parent if we find a key 
    if (temp == -1) {
      fprintf(output, "Process %d found a key at line: %d.\n", p_num, i + 1);
      fflush(output);
      kill(main_pid, SIGUSR1);
    }
  }
  while (p_num == NP - 1 && i < num_lines) {
    int temp = atoi(lines[i]);
    if (temp == -1) {
      // Send a signal to the main parent if we find a key
      fprintf(output, "Process %d found a key at line: %d.\n", p_num, i + 1);
      fflush(output);
      kill(main_pid, SIGUSR1);
    }
    i++;
  }

  // if it is not the main process, exit
  if (getpid() != main_pid) exit(EXIT_SUCCESS);

  // wait for the key_count to get over N (in the test case it is 3)
  while (key_count < N) {
  }
  for (int i = main_pid + 1; i < main_pid + NP; i++) {
    kill(i, SIGUSR2);
    wait(NULL);
  }
}

// Helper function to start the n_keys_spawn() function
void n_keys(int N, int NP, int num_lines) {
  signal(SIGUSR1, increment_sig);
  child_no = NP;

  // Open the output in append mode
  FILE *output = fopen("output.txt", "a");
  fprintf(output, "+---------------------------------+\n");
  fflush(output);
  pid_t pid = fork();
  if (pid == 0) {
    pid_t main_pid = getpid();
    // read the file and make an array with each index as a line of the file
    char **lines = malloc(sizeof(char *) * num_lines);
    FILE *file = fopen("secrets.txt", "r");
    for (int i = 0; i < num_lines; i++) {
      lines[i] = malloc(sizeof(char[BUFFERSIZE]));
      // allocate memory for each index of the file
      fscanf(file, "%[^\n ] ", lines[i]);
    }
    fclose(file);
    clock_t start, end;
    double cpu_time_used;

    // Start the timer so we can report how long it takes
    start = clock();
    n_keys_spawn(N, NP, num_lines, 0, main_pid, lines, output);
    end = clock();
    cpu_time_used = ((double)(end - start) / CLOCKS_PER_SEC);

    // Report how long it took to find those keys
    printf("Time used for %d keys: %2f\n", N, cpu_time_used);
    exit(EXIT_SUCCESS);
  }
  wait(NULL);
}

int main(int argc, char **argv) {
  
  // Define the parameters for the tests
  int NP = 5;
  int keys = 3;
  int num_lines = 50000;
  int max_children = 5;

  // Generate the file for our test
  generate_file(num_lines, 5);


  // Run all the routines to test them
  // We can see their outputs in output.txt
  one_process();
  multiple_processes(NP, num_lines, max_children);
  n_keys(keys, NP, num_lines);
  exit(EXIT_SUCCESS);
}