#include <stdio.h>
#include <stdlib.h>
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

// solution only using one process
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

int main(int argc, char* argv) {
  generate_file(25, 5);
  one_process(1);
  exit(EXIT_SUCCESS);
}