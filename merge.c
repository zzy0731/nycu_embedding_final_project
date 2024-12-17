#include <stdio.h>
#define __USE_GNU
#include <stdlib.h>
#include <string.h>
#include <netinet/in.h>
#include <unistd.h>
#include <sys/shm.h>
#include <signal.h>
#include <time.h>
#include <pthread.h>
#include "parking.h"

#define MAX_CLIENTS 1000
#define KEY 1234
pthread_barrier_t priority_barrier, batch_barrier; // 定義一個柵欄變量
pthread_mutex_t lock; // 定義一個全局的 mutex
void test()
{
    printf("test add in file a");
}

int shmid;
key_t key = KEY;
int demo_shmid;//
key_t key_demo = 2347;
int (*matrix)[16];//8層樓
int thread_status[MAX_CLIENTS] = {0}; // 用來追蹤每個線程的狀態
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
    int thread_id; //這個thread 的id
} ThreadArgs;

int rolldice() {
    // return 5 ;
    return rand() % 50 + 1;
}
void cleanup_shared_data(int signal) {
    if (shmid != -1) {
        // 解除映射
        shmdt(shmat(shmid, NULL, 0));
        // 删除共享内存
        shmctl(shmid, IPC_RMID, NULL);

        shmdt(shmat(demo_shmid, NULL, 0));
        // 删除共享内存
        shmctl(demo_shmid, IPC_RMID, NULL);
        printf("cleanup shared memory\n");
    }
    exit(0);
}
client random_client(int seed) {
    client rand_client;
    struct timespec ts;

    srand(rand());
    rand_client.stay_time = rand() % 16 + 5;  //5~20
    rand_client.vip_level = rand() % 3 + 1;  //1~3
    rand_client.destination = rand() % 16 ;  //0~16
    rand_client.vehicle_type = rand() % 2 +1; //1~2
    // const char *vehicleTypes[2] = {"scooter", "car"};
    // int randIndex = rand() % 2; 
    // strcpy(rand_client.vehicle_type, vehicleTypes[randIndex]); 

    return rand_client;
}
void update_demo_matrix() {
    for (int i=0; i<16; ++i) {
        for (int j=0; j<16; ++j) {
            int sem_val;
            sem_getvalue(&parking_spots[i][j], &sem_val);
            matrix[i][j] = sem_val;
            // printf("%d ", matrix[i][j]);
        }
        // printf("\n");
    }
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
    //printf("gogogo\n");
    update_demo_matrix();
    // printf("current parking list index %d\n",parking_list_index);
    sem_getvalue(&parking_spots[arg->thread_result[parking_list_index][1]][arg->thread_result[parking_list_index][2]], &val); 
    printf("pos %d %d after get semaphore:%d\n",\
    arg->thread_result[parking_list_index][1],arg->thread_result[parking_list_index][2],val); // 檢查是否有正確釋放 semaphore (目前看起來正常)
    printf("Current processes stay time is %d\t vehicle_type is: %d\n",arg->thread_stay_time,arg->thread_vehicle_type );
    pthread_barrier_wait(&batch_barrier);
    sleep(arg->thread_stay_time + arg->thread_result[parking_list_index][0]); // 停車時間 
    if (arg->thread_vehicle_type == 2){ //停完之後釋放資源
        sem_post(&parking_spots[arg->thread_result[parking_list_index][1]][arg->thread_result[parking_list_index][2]]); 
        sem_post(&parking_spots[arg->thread_result[parking_list_index][1]][arg->thread_result[parking_list_index][2]]);
    }
    else{
        sem_post(&parking_spots[arg->thread_result[parking_list_index][1]][arg->thread_result[parking_list_index][2]]);
    }
    update_demo_matrix();
    printf("\n");
    sem_getvalue(&parking_spots[arg->thread_result[parking_list_index][1]][arg->thread_result[parking_list_index][2]], &val);
    printf("thread_id:%d, Pos %d %d after release semaphore is: %d\n",\
    arg->thread_id,arg->thread_result[parking_list_index][1],arg->thread_result[parking_list_index][2],val);
    pthread_mutex_lock(&lock); // 獲取鎖
    thread_status[arg->thread_id] = 0;
    pthread_mutex_unlock(&lock); // 釋放鎖
    pthread_exit(NULL);
}
/*整體流程:
server這邊會隨機生成一個數字寫入shared memory，然後client那邊讀shared memory的數字生成相對應數量的client*/
int main(int argc, char **argv) {
    
    // printf("global variable:%d",parking_time[FLOORS][SPOTS_PER_FLOOR]);
    srand((unsigned int)time(NULL)); // 初始化隨機生成器

    int connected_clients = 0;
    init_parking_spots();
    shmid = shmget(key, sizeof(int), 0644|IPC_CREAT);
    demo_shmid = shmget(key_demo, sizeof(int) * 16 * 16, IPC_CREAT | 0666);
    matrix = (int (*)[16])shmat(demo_shmid, NULL, 0); //將此共享矩陣加入process addr space
    int *batch_size = (int *) shmat(shmid, (void *)0, 0); // 將共享mem加到process的addr space
    if (batch_size == (int *) -1) {
        perror("shmat failed");
        exit(1);
    }


    signal(SIGINT, cleanup_shared_data);
    int last_thread_id = 0;
    while(1){
        *batch_size = rolldice();
        printf("Dice: %d\n", *batch_size);
        pthread_barrier_init(&batch_barrier, NULL, *batch_size+1);//多一個main thread
        pthread_barrier_init(&priority_barrier, NULL, *batch_size);
        client *all_client = (client *)malloc(*batch_size * sizeof(client));
        // 初始化 mutex
        if (pthread_mutex_init(&lock, NULL) != 0) {
            perror("pthread_mutex_init failed");
            exit(EXIT_FAILURE);
        }
        int spot_counts[3] = {0, 0, 0}; // 分別記錄三種車輛類型的停車位數量

            // 計算每種車輛類型的停車位數量
        for (int i = 0; i < FLOORS; i++) {
            for (int j = 0; j < SPOTS_PER_FLOOR; j++) {
                int value;
                sem_getvalue(&parking_spots[i][j], &value);
                spot_counts[value]++;
            }
        }
        if (spot_counts[2] < *batch_size){
            sleep(10);
        }
        
        // 等待客户端連接
        sleep(1);
        int *destination_s = malloc(*batch_size * sizeof(int));
        int *vehicle_type_s = malloc(*batch_size * sizeof(int));
        while (connected_clients < *batch_size) {
            // printf("stay time: %d\n", all_client[connected_clients].stay_time);
            // printf("vip level: %d\n", all_client[connected_clients].vip_level);
            // printf("vehicle type: %s\n", all_client[connected_clients].vehicle_type);
            // printf("destination: %d\n",all_client[connected_clients].destination);
            all_client[connected_clients] = random_client(connected_clients);
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
        // 在創建新線程時，選擇 thread_status 陣列中第一個可用的元素
        for (int i = 0; i < *batch_size; i++ ){
            int thread_index = -1;

            // 尋找第一個可用的 thread_status 元素
            pthread_mutex_lock(&lock); // 獲取鎖
            for (int j = 0; j < MAX_CLIENTS; j++) {
                if (thread_status[j] == 0) {
                    thread_index = j;
                    thread_status[j] = 1; // 標記線程為正在運行
                    break;
                }
            }
            pthread_mutex_unlock(&lock); // 釋放鎖

            if (thread_index == -1) {
                perror("No available thread slot");
                sleep(10);
                // 尋找第一個可用的 thread_status 元素
                pthread_mutex_lock(&lock); // 獲取鎖
                for (int j = 0; j < MAX_CLIENTS; j++) {
                    if (thread_status[j] == 0) {
                        thread_index = j;
                        thread_status[j] = 1; // 標記線程為正在運行
                        break;
                    }
                }
                pthread_mutex_unlock(&lock); // 釋放鎖
                // exit(EXIT_FAILURE);
            }

            thread_args[thread_index].thread_priority = priorities[i];
            thread_args[thread_index].thread_stay_time = all_client[i].stay_time;
            thread_args[thread_index].thread_vehicle_type = all_client[i].vehicle_type;
            thread_args[thread_index].thread_batch_size = *batch_size;
            thread_args[thread_index].thread_result = result[i];
            thread_args[thread_index].thread_id = thread_index;

            if (pthread_create(&thread_ids[thread_index], NULL, args_handler, &thread_args[thread_index]) != 0) {
                perror("pthread_create failed");
                exit(EXIT_FAILURE);
            }

            pthread_detach(thread_ids[thread_index]);
        }
        
        // 更新最後一個線程ID的值
        last_thread_id += *batch_size;

        if (priorities != NULL) {
            for (int i = 0; i < *batch_size; i++) {
                printf("Client %d - Priority: %d\n", i, priorities[i]);
            }
            free(priorities);
        }
        pthread_barrier_wait(&batch_barrier); // 等待所有客戶進入sleep狀態
       
        // int spot_counts[3] = {0, 0, 0}; // 分別記錄三種車輛類型的停車位數量

        //     // 計算每種車輛類型的停車位數量
        // for (int i = 0; i < FLOORS; i++) {
        //     for (int j = 0; j < SPOTS_PER_FLOOR; j++) {
        //         int value;
        //         sem_getvalue(&parking_spots[i][j], &value);
        //         spot_counts[value]++;
        //     }
        // }
        //convert (spot_counts[2])to char use sprintf
        char str[4]; // 假設我們的整數不會超過 10 億
        sprintf(str, "%d", spot_counts[2]);
        
        
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
    
    return 0;
}