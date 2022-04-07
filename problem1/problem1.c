#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>
#include <math.h>
#include <time.h>

#define MAXIMUM 4000

// selection sort for our program
void swap(int *one, int *two)
{
  int temp = *one;
  *one = *two;
  *two = temp;
}
void selectionSort(int *array, int H)
{
  int i, j, min_idx;

  for (i = 0; i < H - 1; i++)
  {
    min_idx = i;
    for (j = i + 1; j < H; j++)
    {
      if (array[j] < array[min_idx])
      {
        min_idx = j;
      }
    }
    swap(&array[min_idx], &array[i]);
  }
}

// this will create a file with L lines
// it will also hide the H keys
void generate_file(int L, int H)
{
  int *secret_indicies = malloc(H * sizeof(int));
  time_t t;
  srand((unsigned)time(&t));

  // generate unique indecies
  for (int i = 0; i < H; i++)
  {
    secret_indicies[i] = rand() % L;
    int checking = 1;
    while (checking)
    {
      int conflicting = 0;
      for (int j = 0; j < i; j++)
      {
        if (secret_indicies[j] == secret_indicies[i])
        {
          conflicting = 1;
        }
      }
      if (conflicting)
      {
        secret_indicies[i] = rand() % L;
      }
      else
      {
        checking = 0;
      }
    }
  }

  // sort this array
  selectionSort(secret_indicies, H);

  FILE *file;
  file = fopen("secrets.txt", "w+");
  int secret_index = 0;
  for (int i = 0; i < L; i++)
  {
    if (secret_indicies[secret_index] == i)
    {
      fprintf(file, "-1\n");
      secret_index++;
    }
    else
    {
      fprintf(file, "%d\n", rand() % MAXIMUM);
    }
  }
  fclose(file);
  return;
}

// Use one process to find the average and the secrets
void one_process(int PN)
{
  int *secret_indicies = malloc(5 * sizeof(int));
  int secret_index = 0;
  int total = 0;
  int line_count = 0;
  int maximum = 0;

  FILE *file = fopen("secrets.txt", "r");
  char line[1024];
  while (fscanf(file, "%[^\n ] ", line) > 0)
  {
    int current = atoi(line);
    if (current == -1)
    {
      secret_indicies[secret_index] = line_count;
      secret_index++;
    }
    else
    {
      total += current;
      if (maximum < current) {
        maximum = current;
      }
    }
    line_count++;
  }
  printf("line count %d total %d\n", line_count, total);
  printf("maximum of %d \n", maximum);
  for (int i = 0; i < 5; i++)
  {
    printf("found a key at line: %d\n", secret_indicies[i] + 1);
  }
  fclose(file);
}

// we have to keep track of each processes number.
//

// Spawn N more processes and then search a section of the file
void spawn_processes(int NP, int num_lines, pid_t p_num, int max_children, pid_t main_pid, int writingpipe, char **lines)
{
  int processes_left = getpid() - main_pid;
  int child_num = 0;
  int **pipes = malloc(sizeof(int *) * max_children);
  for (int i = 0; i < max_children; i++)
  {
    pipes[i] = malloc(sizeof(int[2]));
  }
  char buffer[1024];
  pid_t *pids = malloc(sizeof(pid_t) * max_children);

  // create max_children amount of pipes
  do
  {
    // Child code
    pipe(pipes[child_num]);
    if ((pids[child_num] = fork()) == 0)
    {
      pid_t pnum = getpid() - main_pid;
      printf("Hi, I'm process %d and my parent is %d.\n", pnum, getppid());
      if (pnum >= NP)
      {
        break;
      }
      close(pipes[child_num][0]);
      spawn_processes(NP, num_lines, pnum, max_children, main_pid, pipes[child_num][1], lines);
      exit(EXIT_SUCCESS);
    }
    child_num++;
    if (child_num >= max_children)
    {
      break;
    }
  } while (pids[child_num - 1] <= NP);

  // look in the file
  int linesperchild = num_lines / NP;
  int total = 0;
  int local_max = 0;
  int i;
  for (i = p_num * linesperchild; i < p_num * linesperchild + linesperchild; i++)
  {
    int temp = atoi(lines[i]);
    if (temp != -1)
    {
      if (local_max < atoi(lines[i])) { local_max = atoi(lines[i]); }
      total += atoi(lines[i]);
    }
    else
    {
      printf("found a key at line: %d\n", i+1);
    }
  }
  while(p_num == NP-1 && i<num_lines) {
    int temp = atoi(lines[i]);
    if (temp != -1)
    {
      if (local_max < atoi(lines[i])) { local_max = atoi(lines[i]); }
      total += atoi(lines[i]);
    }
    else
    {
      printf("found a key at line: %d\n", i+1);
    }
    i++;
  }

  // collect the children
  for (int i = 0; i < child_num; i++)
  {
    // read from the children pipes
    if (pids[i] - main_pid < NP)
    {
      close(pipes[i][1]);
      read(pipes[i][0], buffer, sizeof(buffer));
      int child_value = atoi(buffer);
      total += child_value;
      read(pipes[i][0], buffer, sizeof(buffer));
      int child_max = atoi(buffer);
      if (local_max < child_max) { local_max = child_max; }
    }
    wait(NULL);
  }
  // write to the parent pipe
  char sendbuff[1024];
  sprintf(sendbuff, "%d", total);
  write(writingpipe, sendbuff, sizeof(sendbuff));

  sprintf(sendbuff, "%d", local_max);
  printf("sending max of %s \n", sendbuff);
  write(writingpipe, sendbuff, sizeof(sendbuff));
  free(pids);
  free(pipes);
  return;
}

int main(int argc, char *argv)
{
  pid_t main_pid = getpid();
  int numlines = 2394;
  int NP = 13;
  int max_children = 3;

  int first[2];
  pipe(first);
  printf("Main pid is %d \n", main_pid);
  generate_file(numlines, 5);
  one_process(1);

  // read the file and make an array with each index as a line of the file
  char **lines = malloc(sizeof(char *) * numlines);
  FILE *file = fopen("secrets.txt", "r");
  for (int i = 0; i < numlines; i++)
  {
    lines[i] = malloc(sizeof(char[1024]));
    fscanf(file, "%[^\n ] ", lines[i]);
  }
  fclose(file);

  spawn_processes(NP, numlines, 0, max_children, main_pid, first[1], lines);
  char buffer[14];
  close(first[1]);
  read(first[0], buffer, sizeof(buffer));
  printf("Total: %s \n", buffer);
  read(first[0], buffer, sizeof(buffer));
  printf("Max: %s \n", buffer);
  
  exit(EXIT_SUCCESS);
}