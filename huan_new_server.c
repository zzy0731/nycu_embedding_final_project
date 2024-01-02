#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <sys/shm.h>
#include <signal.h>
#include <time.h>
#include "parking.h"

#define MAX_CLIENTS 10
#define KEY 1234

int shmid;
key_t key = KEY;
typedef struct {
    int stay_time; //5-20sec
    int vip_level; // platinum(1) gold(2) normal(3)
    // char vehicle_type[10]; //scooter or car
    int vehicle_type;
    int destination; //1F,2F,3F,4F...
} client;
typedef struct {
    client *client_ptr;
    int original_index;
} client_with_index;

int rolldice() {
    return rand() % 6 + 1;
}
void cleanup_shared_data(int signal) {
    if (shmid != -1) {
        // 解除映射
        shmdt(shmat(shmid, NULL, 0));
        // 删除共享内存
        shmctl(shmid, IPC_RMID, NULL);
        printf("cleanup shared memory\n");
    }
    exit(0);
}
// 比較函數，用於排序
int compare_clients(const void *a, const void *b) {
    client_with_index *clientA = (client_with_index *)a;
    client_with_index *clientB = (client_with_index *)b;

    // 首先按VIP等級排序，等級高的優先（數字越大等級越高）
    if (clientA->client_ptr->vip_level != clientB->client_ptr->vip_level) {
        return clientB->client_ptr->vip_level - clientA->client_ptr->vip_level;
    }

    // 如果VIP等級相同，則停車時間短的優先（時間越短優先級越高）
    return clientA->client_ptr->stay_time - clientB->client_ptr->stay_time;
}
// 生成優先級列表
int *priority_list(client *all_client, int size) {
    client_with_index *indexed_clients = malloc(size * sizeof(client_with_index));
    if (indexed_clients == NULL) {
        fprintf(stderr, "Memory allocation failed!\n");
        return NULL;
    }

    for (int i = 0; i < size; i++) {
        indexed_clients[i].client_ptr = &all_client[i];
        indexed_clients[i].original_index = i;
    }

    qsort(indexed_clients, size, sizeof(client_with_index), compare_clients);

    int *priorities = malloc(size * sizeof(int));
    if (priorities == NULL) {
        free(indexed_clients);
        fprintf(stderr, "Memory allocation failed!\n");
        return NULL;
    }

    // 優先級越高，分配的數字越大
    for (int i = 0; i < size; i++) {
        priorities[indexed_clients[i].original_index] = size - i;
    }

    free(indexed_clients);
    return priorities;
}

/*整體流程:
 server這邊會隨機生成一個數字寫入shared memory，然後client那邊讀shared memory的數字生成相對應數量的client*/
int main(int argc, char **argv) {
    srand(time(NULL)); // 初始化隨機生成器
    int server_fd, new_socket, client_sockets[MAX_CLIENTS];
    struct sockaddr_in address;
    int addrlen = sizeof(address);
    int connected_clients = 0;
    init_parking_spots();
    shmid = shmget(key, sizeof(int), 0644|IPC_CREAT);
    int *batch_size = (int *) shmat(shmid, (void *)0, 0); // 將共享mem加到process的addr space
    if (batch_size == (int *) -1) {
        perror("shmat failed");
        exit(1);
    }

    *batch_size = rolldice();
    printf("Dice: %d\n", *batch_size);
    
    // 創建socket文件描述符
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }
    
    //reusable
    // 設置 SO_REUSEADDR 選項
    int reuse = 1;
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) == -1) {
        perror("Error setting SO_REUSEADDR option");
        exit(EXIT_FAILURE);
    }

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(atoi(argv[1]));
      
    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address))<0) {
        perror("bind failed");
        exit(EXIT_FAILURE);
    }
    if (listen(server_fd, 1000) < 0) {
        perror("listen");
        exit(EXIT_FAILURE);
    }
    signal(SIGINT, cleanup_shared_data);

    client *all_client = (client *)malloc(*batch_size * sizeof(client));
    // 等待客户端連接
    int *destination_s = malloc(*batch_size * sizeof(int));
    int *vehicle_type_s = malloc(*batch_size * sizeof(int));
    while (connected_clients < *batch_size) {
        printf("Waiting for clients...\n");
        if ((new_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t*)&addrlen))<0) {
            perror("accept");
            exit(EXIT_FAILURE);
        }
        printf("Client connected\n");
        client_sockets[connected_clients] = new_socket;
        recv(new_socket, &all_client[connected_clients], sizeof(client), 0);
        // printf("stay time: %d\n", all_client[connected_clients].stay_time);
        // printf("vip level: %d\n", all_client[connected_clients].vip_level);
        // printf("vehicle type: %s\n", all_client[connected_clients].vehicle_type);
        printf("destination: %d\n",all_client[connected_clients].destination);
        destination_s[connected_clients] = all_client[connected_clients].destination;
        vehicle_type_s[connected_clients] = all_client[connected_clients].vehicle_type;
        connected_clients++;
    }
    for (int i = 0; i<*batch_size ;i++){
        printf("destination: %d\tvehicle_type:%d\tvip:%d\tstay_time:%d\n"\
        ,all_client[i].destination, all_client[i].vehicle_type, all_client[i].vip_level, all_client[i].stay_time);
    }
    
    
    int ***result = get_parking_list(destination_s, vehicle_type_s, *batch_size );
    for (int i = 0; i < *batch_size; i++) {
        printf("target floor: %d, vehicle type: %d\n", destination_s[i], vehicle_type_s[i]);
        for (int j = 0; j < *batch_size; j++) {
            printf("time: %d, index_x: %d, index_y: %d\n", result[i][j][0], result[i][j][1], result[i][j][2]);
        }
    }

    free(destination_s);
    free(vehicle_type_s);
    int *priorities = priority_list(all_client, *batch_size);
    if (priorities != NULL) {
        for (int i = 0; i < *batch_size; i++) {
            printf("Client %d - Priority: %d\n", i, priorities[i]);
        }
        free(priorities);
    }

    // 向所有客户端發送信號
    for (int i = 0; i < *batch_size; i++) {
        send(client_sockets[i], "start", strlen("start"), 0);
    }

    printf("Signal sent to all clients. Exiting.\n");
    free(all_client);
    // 關閉所有socket
    for (int i = 0; i < *batch_size; i++) {
        close(client_sockets[i]);
    }
    close(server_fd);
    while (1) {

    }
    return 0;
}
