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

#define MATRIX_SIZE 16

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

void clearTerminal() {
    printf("\033[H\033[J"); // ANSI escape code to clear the screen
}

void printColored(int value) {
    switch (value) {
        case 0:
            printf("\033[1;91m0\033[0m "); // Lighter Red
            break;
        case 1:
            printf("\033[1;93m1\033[0m "); // Lighter Yellow
            break;
        case 2:
            printf("\033[1;92m2\033[0m "); // Bright Green
            break;
        default:
            printf("%d ", value);
    }
}

int main() {
    key_t key_demo = 2347;
    int save_previous[MATRIX_SIZE][MATRIX_SIZE] = {0};
    int demo_shmid = shmget(key_demo, sizeof(int) * MATRIX_SIZE * MATRIX_SIZE, 0666);
    int (*matrix)[MATRIX_SIZE] = (int (*)[MATRIX_SIZE])shmat(demo_shmid, NULL, 0);
    for (int i = 0; i < MATRIX_SIZE; ++i) {
        for (int j = 0; j < MATRIX_SIZE; ++j) {
            matrix[i][j] = rand() % 3; // Initialize with random values (0, 1, 2)
        }
    }
    while (1) {
        if (areMatricesEqual((int (*)[MATRIX_SIZE])save_previous, matrix) == 0) {
            clearTerminal();
            printf("Parking lot status\n");
            for (int i = 0; i < MATRIX_SIZE; ++i) {
                for (int j = 0; j < MATRIX_SIZE; ++j) {
                    printColored(matrix[MATRIX_SIZE - 1 - i][j]);
                }
                printf("\n");
            }
            printf("\n");
        }
        copyMatrix(matrix, (int (*)[MATRIX_SIZE])save_previous);
        // Add a sleep or delay here if you want to control the update speed
    }
    return 0;
}
