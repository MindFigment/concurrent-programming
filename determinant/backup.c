#define _REENTRANT

#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <signal.h>

#define TRUE 1

FILE *open_file(char *, char *);
int get_matrix_size(FILE *);
int **form__square_matrix(FILE *, int);
void print_matrix(int **, int);
void print_pointer_matrix(int ***, int);
int rule_of_sarrus(int **, int, int);
int **form_minor(int **, int, int, int);
int laplace_expansion(int **, int);

int main(int argc, char **argv) {

    FILE *stream;
    int n;
    int **matrix;
    int **minor;

    if (argc != 2) {
        fprintf(stderr, "Usage: %s <file>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    stream = open_file(argv[1], "r");
    n = get_matrix_size(stream);
    fclose(stream);

    stream = open_file(argv[1], "r");
    matrix = form__square_matrix(stream, n);
    fclose(stream);

    print_matrix(matrix, n);

    printf("LAPLACE EXPANSION\n");
    int det = laplace_expansion(matrix, n);
    printf("Det: %i\n", det);

    exit(EXIT_SUCCESS);

}


int get_matrix_size(FILE *stream) {
    char *line = NULL;
    size_t len = 0;
    ssize_t nread;
    int n = 0;
    char *token;

    if ((nread = getline(&line, &len, stream)) != -1) {
        printf("Retrived first row of length %zu (buffer len = %zu)\n", nread, len);
        fwrite(line, nread, 1, stdout);
        while ((token = strsep(&line, " "))) {
            n += 1;
        }
        printf("Matrix size: %i\n", n);
    }
    return n;
}

FILE *open_file(char *filename, char *mode) {
    FILE *fp;
    fp = fopen(filename, mode);
    if (fp == NULL) {
        perror("fopen");
        exit(EXIT_FAILURE);
    }
    return fp;
}

int **form__square_matrix(FILE *stream, int n) {
    int **matrix = (int **)malloc(n * sizeof(int*));
    for (int i = 0; i < n; i++) matrix[i] = (int *)malloc(n * sizeof(int));
    char *line = NULL;
    size_t len = 0;
    ssize_t nread;
    int row, col;
    char *token;

    row = 0;
    while ((nread = getline(&line, &len, stream)) != -1) {
        col = 0;
        while ((token = strsep(&line, " "))) {
            matrix[row][col] = atoi(token); 
            col += 1;

        }
        row += 1;
    }
    return matrix;
}

void print_matrix(int **matrix, int n) {
    printf("START PRINTING MATRIX\n");
    for (int i = 0; i < n; i++) {
        for (int j = 0; j < n; j++) {
            printf("%i ", matrix[i][j]);
        }
        printf("\n");
    }
    printf("END PRINTING MATRIX\n");
}

void print_pointer_matrix(int ***matrix, int n) {
    for (int i = 0; i < n; i++) {
        for (int j = 0; j < n; j++) {
            printf("%i ", *matrix[i][j]);
        }
        printf("\n");
    }
}


int rule_of_sarrus(int **matrix, int row, int col) {
    int determinant;
    determinant = (matrix[row][col] * matrix[row+1][col+1] * matrix[row+2][col+2] + 
                matrix[row+1][col] * matrix[row+2][col+1] * matrix[row][col+2] + 
                matrix[row+2][col] * matrix[row][col+1] * matrix[row+1][col+2]) -
                (matrix[row][col+2] * matrix[row+1][col+1] * matrix[row+2][col] + 
                matrix[row+1][col+2] * matrix[row+2][col+1] * matrix[row][col] + 
                matrix[row+2][col+2] * matrix[row][col+1] * matrix[row+1][col]);
    return determinant;
}

int **form_minor(int **matrix, int n, int del_row, int del_col) {
    int **minor = (int **)malloc(n * sizeof(int*));
    for (int i = 0; i < n; i++) minor[i] = (int *)malloc(n * sizeof(int));
    int row = 0;
    int col = 0;
    for (int i = 0; i < n; i++) {
        if (i != del_row) {
            col = 0;
            for (int j = 0; j < n; j++) {
                if (j != del_col) {
                    minor[row][col] = matrix[i][j];
                    col += 1;
                }
            }
        row += 1;
        }
    }
    return minor;
}

int laplace_expansion(int **matrix, int n) {
    int **minor;
    if (n == 3) {
        return rule_of_sarrus(matrix, 0, 0);
    }
    else {
        int det = 0;
        int multiplier;
        int del_row = n - 1;
        for (int j = 0; j < n; j++) {
            minor = form_minor(matrix, n, del_row, j);
            if ((del_row + j) % 2 == 0) {
                multiplier = 1;
            }
            else {
                multiplier = -1;
            };
            det += matrix[del_row][j] * multiplier * laplace_expansion(minor, n - 1);
        }
        return det;
    }
}
