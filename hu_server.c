#include <stdio.h>
#define __USE_GNU
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

#define MAX_CLIENTS 10
#define KEY 1234
pthread_barrier_t priority_barrier, batch_barrier; // 定義一個柵欄變量

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

typedef struct {
    int thread_priority; // 這個 thread 的優先級
    int **thread_result;
    int thread_stay_time; //這個client 要停留的時間
    int thread_vehicle_type; //1:機車 2:汽車
    int thread_batch_size; // 這批有多少
    int *client_sockets; // 這個thread 要連接的socket
    int thread_id; //這個thread 的id
} ThreadArgs;

int rolldice() {
    // return 5 ;
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
void * args_handler(void *args){
    ThreadArgs *arg = (ThreadArgs *)args;
    struct sched_param param;
    param.sched_priority = arg->thread_priority; //設定這一個thread 的priority
    pthread_setschedparam(pthread_self(), SCHED_FIFO, &param);
    // printf("current working thread priority is %d\n",arg->thread_priority);
    pthread_barrier_wait(&priority_barrier); // 設一個barrier 等到全部thread 進來才離開
    printf("Current working thread priority is %d\t",arg->thread_priority);

    printf("Current processes stay time is %d\t vehicle_type is: %d\n",arg->thread_stay_time,arg->thread_vehicle_type );
    int parking_list_index=0;
    int val;
    while( parking_list_index < arg->thread_batch_size){
        sem_getvalue(&parking_spots[arg->thread_result[parking_list_index][1]][arg->thread_result[parking_list_index][2]], &val);  //獲得這個停車位的semaphore值
        if (val >= arg->thread_vehicle_type){ // 確認 semaphore 數量足夠  thread_vehicle_type:1 表示機車 thread_vehicle_type:2 表示汽車
            printf("parking pos is x:%d, y:%d\n",arg->thread_result[parking_list_index][1],arg->thread_result[parking_list_index][2]);
            if (arg->thread_vehicle_type == 2){ //Car 需要兩個sempahore
                sem_wait(&parking_spots[arg->thread_result[parking_list_index][1]][arg->thread_result[parking_list_index][2]]);
                sem_wait(&parking_spots[arg->thread_result[parking_list_index][1]][arg->thread_result[parking_list_index][2]]);
            }
            else { //機車只需要一個sempahore 這邊好像只能這樣寫 
                sem_wait(&parking_spots[arg->thread_result[parking_list_index][1]][arg->thread_result[parking_list_index][2]]);
            }
            break;
        }
        else{ //這個index 的semphore已經被使用完 去下一個semaphore使用
            parking_list_index++;
        }
    }
    printf("current parking list index %d\n",parking_list_index);
    sem_getvalue(&parking_spots[arg->thread_result[parking_list_index][1]][arg->thread_result[parking_list_index][2]], &val); 
    printf("After get semaphore:%d\n",val); // 檢查是否有正確釋放 semaphore (目前看起來正常)
    printf("Current processes stay time is %d\t vehicle_type is: %d\n",arg->thread_stay_time,arg->thread_vehicle_type );
    pthread_barrier_wait(&batch_barrier);
    sleep(arg->thread_stay_time); // 停車時間 ?
    if (arg->thread_vehicle_type == 2){ //停完之後釋放資源
        sem_post(&parking_spots[arg->thread_result[parking_list_index][1]][arg->thread_result[parking_list_index][2]]); 
        sem_post(&parking_spots[arg->thread_result[parking_list_index][1]][arg->thread_result[parking_list_index][2]]);
    }
    else{
        sem_post(&parking_spots[arg->thread_result[parking_list_index][1]][arg->thread_result[parking_list_index][2]]);
    }
    sem_getvalue(&parking_spots[arg->thread_result[parking_list_index][1]][arg->thread_result[parking_list_index][2]], &val);
    printf("After release semaphore is: %d\n",val);
    printf("process with priority %d end\n",arg->thread_priority);
    // for (int i = 0 ; i < arg->thread_batch_size ; i++){
    //     printf("Current process parking_list is: %d, %d",arg->thread_result[i][1],arg->thread_result[i][2]);
    //     printf("\t and driving time is %d \n",arg->thread_result[i][0]);

    // }
    close(arg->client_sockets[arg->thread_id]); // 關閉這個thread 的socket
    pthread_exit(NULL);
}
/*整體流程:
server這邊會隨機生成一個數字寫入shared memory，然後client那邊讀shared memory的數字生成相對應數量的client*/
int main(int argc, char **argv) {
    
    // printf("global variable:%d",parking_time[FLOORS][SPOTS_PER_FLOOR]);
    srand((unsigned int)time(NULL)); // 初始化隨機生成器
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

    //*batch_size = rolldice();
    //printf("Dice: %d\n", *batch_size);
    
    //pthread_barrier_init(&barrier, NULL, *batch_size);
    
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
    while(1){
        *batch_size = rolldice();
        printf("Dice: %d\n", *batch_size);
        pthread_barrier_init(&batch_barrier, NULL, *batch_size+1);//多一個main thread
        pthread_barrier_init(&priority_barrier, NULL, *batch_size);
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
            // printf("destination: %d\n",all_client[connected_clients].destination);
            destination_s[connected_clients] = all_client[connected_clients].destination;
            vehicle_type_s[connected_clients] = all_client[connected_clients].vehicle_type;
            connected_clients++;
        }
        for (int i = 0; i< *batch_size ; i++){
            printf("destination: %d\t vehicle_type:%d\t vip:%d\tstay_time:%d\n"\
            ,all_client[i].destination, all_client[i].vehicle_type, all_client[i].vip_level, all_client[i].stay_time);
        }
        
        
        int ***result = get_parking_list(destination_s, vehicle_type_s, *batch_size ); //獲得 parking list
        int *priorities = priority_list(all_client, *batch_size);
        // we need proorities and result parking_list stay_time, vehicle_type and batch_size 
        //設定cpu 親合度
        cpu_set_t cpuset;
        CPU_ZERO(&cpuset);
        int cpu_id=3;
        CPU_SET(cpu_id, &cpuset);
        int rc = pthread_setaffinity_np(pthread_self(), sizeof(cpuset), &cpuset);
        //創建 thread 所需要之arguments(我用另一個結構去接寫在ThreadArgs裡
        pthread_t thread_ids[MAX_CLIENTS];
        ThreadArgs thread_args[MAX_CLIENTS];
        for (int thread_id =0; thread_id< *batch_size; thread_id++ ){
            thread_args[thread_id].thread_priority = priorities[thread_id];
            thread_args[thread_id].thread_stay_time = all_client[thread_id].stay_time;
            thread_args[thread_id].thread_vehicle_type = all_client[thread_id].vehicle_type;
            thread_args[thread_id].thread_batch_size = *batch_size;
            thread_args[thread_id].thread_result = result[thread_id];
            thread_args[thread_id].client_sockets = client_sockets;
            thread_args[thread_id].thread_id = thread_id;
            if (pthread_create(&thread_ids[thread_id], NULL, args_handler, &thread_args[thread_id]) != 0) { //將這些參數丟到thread 裡面去處理
                perror("pthread_create failed");
                exit(EXIT_FAILURE);
            }
            pthread_detach(thread_ids[thread_id]); // 使用 pthread_detach 代替 pthread_join
        }

        if (priorities != NULL) {
            for (int i = 0; i < *batch_size; i++) {
                printf("Client %d - Priority: %d\n", i, priorities[i]);
            }
            free(priorities);
        }
        pthread_barrier_wait(&batch_barrier); // 等待所有客戶進入sleep狀態
        // 向所有客户端發送信號目前好像是有點多餘
        for (int i = 0; i < *batch_size; i++) {
            send(client_sockets[i], "start", strlen("start"), 0);
        }
        /*for (int i = 0; i < *batch_size; i++) { // 如果有while迴圈的話這邊要注意一下不然可能會等到這邊thread 執行完 才去執行下一個while
            pthread_join(thread_ids[i], NULL);
        }*/

        printf("Signal sent to all clients. Exiting.\n");
        free(all_client);
        free(destination_s);
        free(vehicle_type_s);
        pthread_barrier_destroy(&batch_barrier);
        pthread_barrier_destroy(&priority_barrier);
        connected_clients = 0;// 重置連接客戶端數量
    }
    // 關閉所有socket(轉移給client自己去關閉)
    /*for (int i = 0; i < *batch_size; i++) {
        close(client_sockets[i]);
    }*/
    close(server_fd);
    
    return 0;
}