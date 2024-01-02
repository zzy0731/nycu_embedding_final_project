#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/shm.h>
#include <time.h>
#define KEY 1234

key_t key = KEY;
int shmid;
typedef struct {
    int stay_time; //5-20sec
    int vip_level; // platinum(1) gold(2) normal(3)
    // char vehicle_type[10]; //scooter or car
    int vehivle_type;
    int destination; //1F,2F,3F,4F...
} client;

client random_client(int seed) {
    client rand_client;
    struct timespec ts;

    srand(rand());
    rand_client.stay_time = rand() % 16 + 5;  //5~20
    rand_client.vip_level = rand() % 3 + 1;  //1~3
    rand_client.destination = rand() % 8 + 1;  //1~8
    rand_client.vehivle_type = rand() % 2 +1; //1~2
    // const char *vehicleTypes[2] = {"scooter", "car"};
    // int randIndex = rand() % 2; 
    // strcpy(rand_client.vehicle_type, vehicleTypes[randIndex]); 

    return rand_client;
}
int main(int argc, char **argv) {
    // client client1;
    // client1 = random_client();
    // printf("stay time: %d\n", client1.stay_time);
    // printf("vip level: %d\n", client1.vip_level);
    // printf("vehicle type: %s\n", client1.vehicle_type);
    shmid = shmget(key, sizeof(int), 0666); // 连接到共享内存
    if (shmid == -1) {
        perror("shmget failed");
        exit(1);
    }
    int *batch_size = (int *) shmat(shmid, (void *)0, 0); // 將共享mem加到process的addr space
    if (batch_size == (int *) -1) {
        perror("shmat failed");
        exit(1);
    }
    printf("Client Dice:%d\n", *batch_size);
    
    int *socks = (int *)malloc(*batch_size * sizeof(int));
    if (socks == NULL) {
        fprintf(stderr, "Memory allocation failed!\n");
        return 1;
    }

    struct sockaddr_in serv_addr;
    char buffer[1024] = {0};

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = inet_addr(argv[1]);
    serv_addr.sin_port = htons(atoi(argv[2]));

    for (int i = 0; i < *batch_size; i++) {
        client client_info = random_client(i+1);

        if ((socks[i] = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
            printf("\n Socket creation error \n");
            return -1;
        }

        if (connect(socks[i], (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
            printf("\nConnection Failed \n");
            return -1;
        }
        send(socks[i], &client_info, sizeof(client_info), 0);

        printf("Connected to server with socket %d\n", socks[i]);
    }

    // 等待服务器的开始信号
    read(socks[0], buffer, 1024);  // 只读取一个socket的响应
    printf("Message from server: %s\n", buffer);
    free(socks);
    // 关闭所有sockets
    for (int i = 0; i < *batch_size; i++) {
        close(socks[i]);
    }

    return 0;
}
