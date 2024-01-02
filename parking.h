#include <stdio.h>
#include <semaphore.h>
#include <stdlib.h>

#define FLOORS 8
#define SPOTS_PER_FLOOR 8
#define MAX_TIME 999

sem_t parking_spots[FLOORS][SPOTS_PER_FLOOR];
int parking_time[FLOORS][SPOTS_PER_FLOOR];

typedef struct {
    int time;
    int index_x;
    int index_y;
} ParkingInfo;

ParkingInfo infos[FLOORS * SPOTS_PER_FLOOR];

void init_parking_spots() {
    for (int i = 0; i < FLOORS; i++) {
        for (int j = 0; j < SPOTS_PER_FLOOR; j++) {
            sem_init(&parking_spots[i][j], 0, 2);
            parking_time[i][j] = MAX_TIME;
        }
    }
}

void get_parking_time(int vehicle_type) {
    for (int i = 0; i < FLOORS; i++) {
        for (int j = 0; j < SPOTS_PER_FLOOR; j++) {
            int val;
            sem_getvalue(&parking_spots[i][j], &val);
            if (val >= vehicle_type) {
                parking_time[i][j] = i + 0.3 * (SPOTS_PER_FLOOR-j) + j;
            } else {
                parking_time[i][j] = -1;
            }
        }
    }
}

void bubble_sort(ParkingInfo arr[], int n) {
    for (int i = 0; i < n-1; i++) {
        for (int j = 0; j < n-i-1; j++) {
            if (arr[j].time > arr[j+1].time) {
                ParkingInfo temp = arr[j];
                arr[j] = arr[j+1];
                arr[j+1] = temp;
            }
        }
    }
}

/*void get_parking_list(int target_floors[], int vehicle_types[], int n, int** result, int*** indices) {
    
    for (int i = 0; i < n; i++) {
        get_parking_time(vehicle_types[i]);//1 or 2
        int target_floor = target_floors[i];
        
        int k = 0;
        for (int j = 0; j < FLOORS; j++) {
            for (int l = 0; l < SPOTS_PER_FLOOR; l++) {
                if (parking_time[j][l] != -1) {
                    infos[k].time = parking_time[j][l] + 3 * abs(target_floor - j);
                    infos[k].index_x = j;
                    infos[k].index_y = l;
                    k++;
                }
            }
        }
        bubble_sort(infos, k);
        for (int j = 0; j < n; j++) {
            result[i][j] = infos[j].time;
            indices[i][j][0] = infos[j].index_x;
            indices[i][j][1] = infos[j].index_y;
        }
    }
}*/
int*** get_parking_list(int target_floors[], int vehicle_types[], int n) {
    int *type1_indices = malloc(n * sizeof(int));
    int *type2_indices = malloc(n * sizeof(int));
    int type1_count = 0, type2_count = 0;

    // 分類車輛類型
    for (int i = 0; i < n; i++) {
        if (vehicle_types[i] == 1) {
            if (type1_count < n) {
                type1_indices[type1_count++] = i;
            }
        } else if (vehicle_types[i] == 2) {
            if (type2_count < n) {
                type2_indices[type2_count++] = i;
            }
        }
    }

    // 建立3維陣列
    int ***result = malloc(n * sizeof(int**));
    for (int i = 0; i < n; i++) {
        result[i] = malloc(n * sizeof(int*));
        for (int j = 0; j < n; j++) {
            result[i][j] = malloc(3 * sizeof(int));
        }
    }

    // 處理車輛類型1
    get_parking_time(1);
    for (int i = 0; i < type1_count; i++) {
        int target_floor = target_floors[type1_indices[i]];
        int k = 0;
        for (int j = 0; j < FLOORS; j++) {
            for (int l = 0; l < SPOTS_PER_FLOOR; l++) {
                if (parking_time[j][l] != -1) {
                    infos[k].time = parking_time[j][l] + 3 * abs(target_floor - j);
                    infos[k].index_x = j;
                    infos[k].index_y = l;
                    k++;
                }
            }
        }
        bubble_sort(infos, k);
        for (int j = 0; j < k; j++) {
            if (j < n) {
                result[type1_indices[i]][j][0] = infos[j].time;
                result[type1_indices[i]][j][1] = infos[j].index_x;
                result[type1_indices[i]][j][2] = infos[j].index_y;
            }
        }
    }

    // 處理車輛類型2
    get_parking_time(2);
    for (int i = 0; i < type2_count; i++) {
        int target_floor = target_floors[type2_indices[i]];
        int k = 0;
        for (int j = 0; j < FLOORS; j++) {
            for (int l = 0; l < SPOTS_PER_FLOOR; l++) {
                if (parking_time[j][l] != -1) {
                    infos[k].time = parking_time[j][l] + 3 * abs(target_floor - j);
                    infos[k].index_x = j;
                    infos[k].index_y = l;
                    k++;
                }
            }
        }
        bubble_sort(infos, k);
        for (int j = 0; j < k; j++) {
            if (j < n) {
                result[type2_indices[i]][j][0] = infos[j].time;
                result[type2_indices[i]][j][1] = infos[j].index_x;
                result[type2_indices[i]][j][2] = infos[j].index_y;
            }
        }
    }
    free(type1_indices);
    free(type2_indices);

    return result;
}