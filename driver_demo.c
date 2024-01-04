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
#include <fcntl.h>
#define DEVICE_PATH "/dev/etx_device"
#define MATRIX_SIZE 16
int fd;
int num_arr[10][7] = {
    {1,1,1,1,1,1,0}, // display number '0'
    {0,1,1,0,0,0,0}, // display number '1'
    {1,1,0,1,1,0,1}, // display number '2'
    {1,1,1,1,0,0,1}, // display number '3'
    {0,1,1,0,0,1,1}, // display number '4'
    {1,0,1,1,0,1,1}, // display number '5'
    {1,0,1,1,1,1,1}, // display number '6'
    {1,1,1,0,0,0,0}, // display number '7'
    {1,1,1,1,1,1,1}, // display number '8'
    {1,1,1,1,0,1,1}  // display number '9'
    };

int *stringToArray(char *str) {

    int str_long = strlen(str);
    int *my_number = malloc(str_long * sizeof(int));
    if (str_long == 3) {
        for (int i = 0; i < str_long; i++) {
            my_number[i] = str[i] - '0';
            // printf("%d", my_number[0]);
    }
    }
    else if (str_long == 2) {
        my_number[0] = 0;
        for (int i = 0; i < str_long; ++i) {
            my_number[i+1] = str[i] - '0';
        }
    }
    else if (str_long == 1) {
        my_number[0] = 0;
        my_number[1] = 0;
        my_number[2] = str[0] - '0';
    }

    return my_number;
}
void handle_signal() {
    close(fd);
    exit(0);
}
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
            printf("\033[1;31m0\033[0m "); // Red
            break;
        case 1:
            printf("\033[1;33m1\033[0m "); // Yellow
            break;
        case 2:
            printf("\033[1;32m2\033[0m "); // Green
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
    fd = open(DEVICE_PATH, O_RDWR);
    signal(SIGINT, handle_signal);
    for (int i = 0; i < MATRIX_SIZE; ++i) {
        for (int j = 0; j < MATRIX_SIZE; ++j) {
            matrix[i][j] = 2; // Initialize with random values (0, 1, 2)
        }
    }
    while (1) {
        if (areMatricesEqual((int (*)[MATRIX_SIZE])save_previous, matrix) == 0) {
            int rest_sum = 0;
            char num_str[10];
            clearTerminal();
            printf("Parking lot status\n");
            for (int i = 0; i < MATRIX_SIZE; ++i) {
                for (int j = 0; j < MATRIX_SIZE; ++j) {
                    rest_sum += matrix[MATRIX_SIZE - 1 - i][j];
                    printColored(matrix[MATRIX_SIZE - 1 - i][j]);
                }
                printf("\n");
            }
            printf("rest sum: %d\n", rest_sum);
            sprintf(num_str, "%d", rest_sum);
            int *numtodriver = stringToArray(num_str);
            int buffer[3 * 7]; // This will hold all segments for all digits

            // Convert each digit to its 7-segment array representation
            for (int i = 0; i < 3; ++i) {
                memcpy(&buffer[i * 7], num_arr[numtodriver[i]], 7 * sizeof(int));
            }
            // Do something with buffer here, such as writing it to driver
            // for (int i=0; i<21; ++i) {
            //     printf("%d ", buffer[i]);
            // }
            write(fd, buffer, 21 * sizeof(int));
            free(numtodriver);  // Free the memory allocated by stringToArray
        }
        copyMatrix(matrix, (int (*)[MATRIX_SIZE])save_previous);
        // Add a sleep or delay here if you want to control the update speed
    }
    return 0;
}