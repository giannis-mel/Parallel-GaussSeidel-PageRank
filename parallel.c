/* 
   Parallel PageRank Implementation

   Compile with: gcc parallel.c -o parallel -O3 -lm -fopenmp

   Description:
   This program computes the PageRank of a list of websites based on their link structure,
   provided in a file named 'hollins.dat'. The program uses the PageRank algorithm with a
   damping factor to account for random jumps and OpenMP for parallelization. The output 
   is the ten websites with the highest PageRank scores.
*/

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include <sys/time.h>
#include <omp.h>

int n;
double d = 0.75; // Damping factor

void swap(double *xp, double *yp, char *str1, char *str2) {
    double temp = *xp;
    *xp = *yp;
    *yp = temp;
    
    char *tempStr = malloc((strlen(str1) + 1) * sizeof(char)); 
    strcpy(tempStr, str1); 
    strcpy(str1, str2); 
    strcpy(str2, tempStr); 
    free(tempStr); 
}

// BubbleSort algorithm that sorts PageRank scores and corresponding websites
void bubbleSort(double* arr, char** web, int n) {
    for (int i = 0; i < n - 1; i++) {      
        for (int j = 0; j < n - i - 1; j++) { 
            if (arr[j] < arr[j + 1]) {
                swap(&arr[j], &arr[j + 1], web[j], web[j + 1]);
            }
        }
    }
}

int main() {
    struct timeval startwtime, endwtime;
    double seq_time;
    int connections;
    int scanfReturn;

    // Read data from 'hollins.dat'
    FILE *myFile = fopen("hollins.dat", "r");
    if (myFile == NULL) {
        fprintf(stderr, "Failed to open hollins.dat.\n");
        return 1;
    }

    // Scan the number of websites and connections from file
    if (fscanf(myFile, "%d %d ", &n, &connections) != 2) {
        fprintf(stderr, "File format error.\n");
        fclose(myFile);
        return 1;
    }

    char **websites = malloc(n * sizeof(char *));
    if (!websites) {
        fprintf(stderr, "Memory allocation failed for websites array.\n");
        fclose(myFile);
        return 1;
    }

    int *numbers = malloc(n * sizeof(int)); // Declaration of numbers array
    if (!numbers) {
        fprintf(stderr, "Memory allocation failed for numbers array.\n");
        for (int j = 0; j < n; j++) {
            free(websites[j]);  // Clean up websites array
        }
        free(websites);
        fclose(myFile);
        return 1;
    }

    for (int i = 0; i < n; i++) {
        websites[i] = malloc(1024 * sizeof(char));
        if (!websites[i]) {
            fprintf(stderr, "Memory allocation failed for website %d.\n", i);
            // Cleanup previously allocated memory
            for (int j = 0; j < i; j++) {
                free(websites[j]);
            }
            free(websites);
            free(numbers);
            fclose(myFile);
            return 1;
        }
    }

    // Read website indices and names
    for (int i = 0; i < n; i++) {
        if (fscanf(myFile, "%d %[^\n]", &numbers[i], websites[i]) != 2) {
            fprintf(stderr, "Error reading websites data.\n");
            fclose(myFile);
            return 1;
        }
    }

    // Initialize adjacency matrix 'S'
    double **S = malloc(n * sizeof(double *)); 
    for (int i = 0; i < n; i++) {
        S[i] = calloc(n, sizeof(double)); // Use calloc to initialize to zero
    }

    int temp1, temp2;
    // Read connections and update the adjacency matrix
    for (int i = 0; i < connections; i++) {
        if (fscanf(myFile, "%d %d", &temp1, &temp2) != 2) {
            fprintf(stderr, "Error reading connections.\n");
            fclose(myFile);
            return 1;
        }
        S[temp1 - 1][temp2 - 1] = 1.0;
    }
    fclose(myFile);

    // Calculate the transpose of S, St
    double **St = malloc(n * sizeof(double *));
    for (int i = 0; i < n; i++) {
        St[i] = calloc(n, sizeof(double));
    }

    int *outlinks = calloc(n, sizeof(int));

    // Calculate outlinks and construct stochastic matrix S
    for (int i = 0; i < n; i++) {
        for (int j = 0; j < n; j++) {
            if (S[i][j] == 1.0) {
                outlinks[i]++;
            }
        }

        for (int j = 0; j < n; j++) {
            if (outlinks[i] == 0) {
                S[i][j] = 1.0 / n;
            } else if (S[i][j] != 0.0) {
                S[i][j] /= outlinks[i];
            }
            St[j][i] = S[i][j];
        }
    }

    printf("Using damping factor d = %f\n", d);

    // Matrix 'A' for the PageRank algorithm
    double **A = malloc(n * sizeof(double *));
    for (int i = 0; i < n; i++) {
        A[i] = malloc(n * sizeof(double));
        for (int j = 0; j < n; j++) {
            A[i][j] = (i == j ? 1.0 : 0.0) - d * St[i][j];
        }
    }

    double *b = malloc(n * sizeof(double));
    for (int i = 0; i < n; i++) {
        b[i] = (1.0 - d) * (1.0 / n);
    }

    double *pageRank = malloc(n * sizeof(double));
    for (int i = 0; i < n; i++) {
        pageRank[i] = 1.0 / n;
    }

    double error;
    int iterations = 0;

    int threads = omp_get_max_threads(); // Get the maximum number of threads available
    printf("Using %d threads\n", threads);

    omp_set_dynamic(0);     // Explicitly disable dynamic teams
    omp_set_num_threads(threads);

    gettimeofday(&startwtime, NULL);

    // Iterative algorithm to compute PageRank
    do {
        error = 0.0;
        #pragma omp parallel for reduction(+:error)
        for (int i = 0; i < n; i++) {
            double temp3 = 0.0;
            for (int j = 0; j < n; j++) {
                temp3 += A[i][j] * pageRank[j];
            }

            double shift = (1 / A[i][i]) * (b[i] - temp3);
            pageRank[i] += shift;
            error += fabs(shift);
        }
        iterations++;
    } while (error >= 0.000001);

    gettimeofday(&endwtime, NULL);
    seq_time = (double)((endwtime.tv_usec - startwtime.tv_usec) / 1.0e6 + endwtime.tv_sec - startwtime.tv_sec);
    printf("Parallel time with %d threads is = %f seconds\n", threads, seq_time);

    // Sorting the PageRank scores
    bubbleSort(pageRank, websites, n);

    // Display the top 10 websites
    printf("The 10 biggest sites are:\n");
    for (int i = 0; i < 10; i++) {
        printf("%d(%lf): %s\n", i + 1, pageRank[i], websites[i]);
    }

    printf("The number of iterations is: %d\n", iterations);

    // Clean up
    for (int i = 0; i < n; i++) {
        free(S[i]);
        free(St[i]);
        free(A[i]);
        free(websites[i]);
    }
    free(S);
    free(St);
    free(A);
    free(websites);
    free(numbers);
    free(b);
    free(pageRank);
    free(outlinks);

    return 0;
}
