#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <sys/shm.h>
#include <signal.h>
#include <time.h>
#include <pthread.h>
#include "parking.h"

#define MATRIX_SIZE 8

void copyMatrix(int src[MATRIX_SIZE][MATRIX_SIZE], int dest[MATRIX_SIZE][MATRIX_SIZE]) {
    for (int i = 0; i < MATRIX_SIZE; i++) {
        for (int j = 0; j < MATRIX_SIZE; j++) {
            dest[i][j] = src[i][j];
        }
    }
}

int areMatricesEqual(int matrix1[MATRIX_SIZE][MATRIX_SIZE], int matrix2[MATRIX_SIZE][MATRIX_SIZE]) {
    for (int i = 0; i < MATRIX_SIZE; i++) {
        for (int j = 0; j < MATRIX_SIZE; j++) {
            if (matrix1[i][j] != matrix2[i][j]) {
                return 0; 
            }
        }
    }
    return 1; 
}

int main() {
    key_t key_demo = 2345;
    int save_previous[MATRIX_SIZE][MATRIX_SIZE] = {0};
    int demo_shmid = shmget(key_demo, sizeof(int) * MATRIX_SIZE * MATRIX_SIZE, 0666);
    int (*matrix)[MATRIX_SIZE] = (int (*)[MATRIX_SIZE])shmat(demo_shmid, NULL, 0); 
    for (int i=0; i<8; ++i) {
        for (int j=0; j<8; ++j) {
            matrix[i][j] = 2;
        }
    }
    while(1) {
        if (areMatricesEqual((int (*)[MATRIX_SIZE])save_previous, matrix) == 0) {
            printf("Paking lot status\n");
            for (int i = 0; i < MATRIX_SIZE; ++i) {
                for (int j = 0; j < MATRIX_SIZE; ++j) {
                    printf("%d ", matrix[MATRIX_SIZE - 1 - i][j]);
                }
                printf("\n");
            }
            printf("\n");
        }
        // printf("matrix:\n");
        // for (int i = 0; i < MATRIX_SIZE; ++i) {
        //         for (int j = 0; j < MATRIX_SIZE; ++j) {
        //             printf("%d ", matrix[MATRIX_SIZE - 1 - i][j]);
        //         }
        //         printf("\n");
        //     }
        //     printf("\n");
        copyMatrix(matrix, (int (*)[MATRIX_SIZE])save_previous);
        // printf("after copy:\n");
        // for (int i = 0; i < MATRIX_SIZE; ++i) {
        //         for (int j = 0; j < MATRIX_SIZE; ++j) {
        //             printf("%d ", save_previous[MATRIX_SIZE - 1 - i][j]);
        //         }
        //         printf("\n");
        //     }
        //     printf("\n");
    }
    return 0;
}
