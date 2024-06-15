#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <fcntl.h>
#include <sys/select.h>
#include <linux/i2c-dev.h>
#include <sys/ioctl.h>
#include <wiringPi.h>
#include <math.h>
#include "qr.h"

#define I2C_DEV "/dev/i2c-1"
#define I2C_ADDR 0x16

#define Tracking_Left1 27
#define Tracking_Left2 22
#define Tracking_Right1 17
#define Tracking_Right2 4

#define PORT 8080 // 서버 포트 번호
#define SERVER_IP "43.200.198.48" // 서버 IP 주소
// #define SERVER_IP "192.168.0.10" // 서버 IP 주소
// #define SERVER_IP "127.0.0.1" // 서버 IP 주소

#define MAX_CLIENTS 2
#define _MAP_ROW 4
#define _MAP_COL 4
#define MAP_ROW (_MAP_ROW + 1)
#define MAP_COL (_MAP_COL + 1)

#define turn_left 1
#define turn_right 2
#define go_straight 3

int marking = 0;
int dir = 0;
int first_check = 0;

typedef struct {
    int socket;
    struct sockaddr_in address;
    int row;
    int col;
    int score;
    int bomb;
} client_info;

enum Status {
    nothing, //0
    item, //1
    trap //2
};

typedef struct {
    enum Status status;
    int score;
} Item;

typedef struct {
    int row;
    int col;
    Item item;
} Node;

typedef struct {
    client_info players[MAX_CLIENTS];
    Node map[MAP_ROW][MAP_COL];
} DGIST;

enum Action {
    move, //0
    setBomb, //1
};

typedef struct {
    int row;
    int col;
    enum Action action;
} ClientAction;

enum Direction {
    UP,
    DOWN,
    LEFT,
    RIGHT
};

int fd;
char qr_data[128];
pthread_mutex_t qr_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t qr_cond = PTHREAD_COND_INITIALIZER;
DGIST dgist;
pthread_mutex_t dgist_mutex = PTHREAD_MUTEX_INITIALIZER; 
pthread_cond_t dgist_cond = PTHREAD_COND_INITIALIZER;

void printDGIST(DGIST* dgist) {
    printf("==== MAP ====\n");
    for (int j = 0; j < MAP_ROW; j++) {
        for (int i = 0; i < MAP_COL; i++) {
            switch (dgist->map[j][i].item.status) {
                case nothing:
                    printf("- ");
                    break;
                case item:
                    printf("%d ", dgist->map[j][i].item.score);
                    break;
                case trap:
                    printf("x ");
                    break;
            }
        }
        printf("\n");
    }
    printf("==== PLAYERS ====\n");
    for (int i = 0; i < MAX_CLIENTS; i++) {
        printf("Player %d: row=%d, col=%d, score=%d, bomb=%d\n",
                i+1, dgist->players[i].row, dgist->players[i].col,
                dgist->players[i].score, dgist->players[i].bomb);
    }
}

void i2c_init() {
    if ((fd = open(I2C_DEV, O_RDWR)) < 0) {
        perror("##### I2C OPEN FAIL #####");
        exit(1);
    }

    if (ioctl(fd, I2C_SLAVE, I2C_ADDR) < 0) {
        perror("##### I2C SLAVE FAIL #####");
        close(fd);
        exit(1);
    }
}

void i2c_write_block_data(unsigned char reg, unsigned char *data, int length) {
    unsigned char buffer[5];
    buffer[0] = reg;
    for (int i = 0; i < length; i++) {
        buffer[i + 1] = data[i];
    }
    if (write(fd, buffer, length + 1) != length + 1) {
        perror("##### BLOCK WRITE FAIL #####");
    }
}

void ctrl_car(int l_dir, int l_speed, int r_dir, int r_speed) {
    // l_dir, r_dir : [0 : back] [1 : front]
    unsigned char data[4] = {l_dir, l_speed, r_dir, r_speed};
    i2c_write_block_data(0x01, data, 4);
}

void car_run(int speed1, int speed2) {
    ctrl_car(1, speed1, 1, speed2);
}

void car_back(int speed1, int speed2) {
    ctrl_car(0, speed1, 0, speed2);
}

void car_spin_left(int speed1, int speed2) {
    ctrl_car(0, speed1, 1, speed2);
}

void car_spin_right(int speed1, int speed2) {
    ctrl_car(1, speed1, 0, speed2);
}

void car_stop() {
    unsigned char data[1] = {0x00};
    i2c_write_block_data(0x02, data, 1);
}

void setup() {
    wiringPiSetupGpio();
    pinMode(Tracking_Left1, INPUT);
    pinMode(Tracking_Left2, INPUT);
    pinMode(Tracking_Right1, INPUT);
    pinMode(Tracking_Right2, INPUT);
}

void tracing() {
    int Tracking_Left1Value = digitalRead(Tracking_Left1);
    int Tracking_Left2Value = digitalRead(Tracking_Left2);
    int Tracking_Right1Value = digitalRead(Tracking_Right1);
    int Tracking_Right2Value = digitalRead(Tracking_Right2);

    if (Tracking_Left1Value + Tracking_Left2Value + Tracking_Right1Value + Tracking_Right2Value <= 1){
        if (dir == go_straight){
            car_run(80, 80);
            usleep(800000);
            printf("GO STRAIGHT\n");
        }
        else if (dir == turn_left){
            car_spin_left(30, 70);
            usleep(1300000);  // 0.1초
            printf("TURN LEFT\n");
        }
        else if (dir == turn_right){
            car_spin_right(70, 30);
            usleep(1000000);  // 0.1초
            printf("TURN RIGHT\n");
        }
        dir = 0;
    }

    if ((Tracking_Left1Value == LOW || Tracking_Left2Value == LOW) && Tracking_Right2Value == LOW) {
        car_spin_right(70, 30);
        usleep(100000);  // 0.1초
    } else if (Tracking_Left1Value == LOW && (Tracking_Right1Value == LOW || Tracking_Right2Value == LOW)) {
        car_spin_left(30, 70);
        usleep(100000);  // 0.1초
    } else if (Tracking_Left1Value == LOW) {
        car_spin_left(70, 70);
        usleep(50000);  // 0.05초
    } else if (Tracking_Right2Value == LOW) {
        car_spin_right(70, 70);
        usleep(50000);  // 0.05초
    } else if (Tracking_Left2Value == LOW && Tracking_Right1Value == HIGH) {
        car_spin_left(60, 60);
        usleep(20000);  // 0.02초
    } else if (Tracking_Left2Value == HIGH && Tracking_Right1Value == LOW) {
        car_spin_right(60, 60);
        usleep(20000);  // 0.02초
    } else if (Tracking_Left2Value == LOW && Tracking_Right1Value == LOW) {
        car_run(65, 65);
    }
}

void *execution(void *arg) {
    i2c_init();
    setup();

    // time_t start_time = time(NULL);
    // time_t cur_time;

    while (1) {
        // cur_time = time(NULL);
        // if (difftime(cur_time, start_time) > 180) {
        //     break;
        // }
        tracing();
        delay(10);
    }
    car_stop();
    close(fd);
    return NULL;
}

void *qr_detection(void *arg) {
    while (1) {
        if (marking == 0){
            char *data = qr_detect();
            if (data != NULL) {
                pthread_mutex_lock(&qr_mutex);
                strncpy(qr_data, data, sizeof(qr_data) - 1);
                qr_data[sizeof(qr_data) - 1] = '\0';
                pthread_cond_signal(&qr_cond);
                pthread_mutex_unlock(&qr_mutex);
            }
            sleep(1);
        }
    }
    return NULL;
}

double get_weight(int distance) {
    switch(distance) {
        case 1: return 1.0;
        case 2: return 1.3;
        case 3: return 1.6;
        case 4: return 2.0;
        default: return 1.0;
    }
}

double calculate_weighted_score(DGIST *dgist, int row, int col, int row_offset, int col_offset, int distance) {
    int new_row = row + row_offset;
    int new_col = col + col_offset;
    if (new_row >= 0 && new_row < MAP_ROW && new_col >= 0 && new_col < MAP_COL) {
        Item item = dgist->map[new_row][new_col].item;
        if (item.status == nothing) {
            return 0.0;
        } else if (item.status == 1) {
            return item.score / get_weight(distance);
        } else if (item.status == trap) {
            return -8.0 / get_weight(distance);
        }
    }
    return 0.0; // Boundary check, use 0 for invalid moves
}


enum Direction find_best_direction(DGIST *dgist, int my_row, int my_col) {
    double up_score = 0;
    double down_score = 0;
    double left_score = 0;
    double right_score = 0;

    // 상하좌우 점수 계산
    for (int i = 1; i < MAP_ROW; i++) {
        up_score += calculate_weighted_score(dgist, my_row, my_col, -i, 0, i);
        down_score += calculate_weighted_score(dgist, my_row, my_col, i, 0, i);
        left_score += calculate_weighted_score(dgist, my_row, my_col, 0, -i, i);
        right_score += calculate_weighted_score(dgist, my_row, my_col, 0, i, i);
    }

    // 가장 높은 점수의 방향 찾기
    enum Direction best_direction = UP;
    double highest_score = up_score;

    if (down_score > highest_score) {
        highest_score = down_score;
        best_direction = DOWN;
    }
    if (left_score > highest_score) {
        highest_score = left_score;
        best_direction = LEFT;
    }
    if (right_score > highest_score) {
        highest_score = right_score;
        best_direction = RIGHT;
    }

    return best_direction;
}

void* receiveData(void* arg) {
    int sock = *(int*)arg;
    ssize_t total_bytes_read = 0;

    while (1) {
        total_bytes_read = 0;
        while (total_bytes_read < sizeof(DGIST)) {
            ssize_t bytes_read = recv(sock, ((char*)&dgist) + total_bytes_read, sizeof(DGIST) - total_bytes_read, 0);
            if (bytes_read <= 0) {
                if (bytes_read == 0) {
                    printf("Server closed the connection\n");
                    close(sock);
                    pthread_exit(NULL);
                } else {
                    perror("Failed to receive data from server");
                    close(sock);
                    pthread_exit(NULL);
                }
            }
            total_bytes_read += bytes_read;
        }

        // mutex를 사용하여 공유 데이터 보호
        pthread_mutex_lock(&dgist_mutex);
        printf("DGIST structure received from server:\n");
        printDGIST(&dgist);
        pthread_cond_signal(&dgist_cond);
        pthread_mutex_unlock(&dgist_mutex);
    }
    return NULL;
}

void *server_communication(void *arg) {
    int sock = 0;
    struct sockaddr_in serv_addr;
    ClientAction cAction;
    char last_qr_data[128] = {0};

    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        printf("Socket creation error\n");
        return NULL;
    }

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(PORT);

    if (inet_pton(AF_INET, SERVER_IP, &serv_addr.sin_addr) <= 0) {
        printf("Invalid address/ Address not supported\n");
        return NULL;
    }

    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        printf("Connection Failed\n");
        close(sock);
        return NULL;
    }
    printf("Connected to server\n");

    pthread_t receive_thread;
    pthread_create(&receive_thread, NULL, receiveData, (void *)&sock);

    // int past_row = 0;
    // int past_col = -1;
    int past_row;
    int past_col;

    while (1) {
        // QR 코드 감지 및 전송
        pthread_mutex_lock(&qr_mutex);
        pthread_cond_wait(&qr_cond, &qr_mutex);

        if (strcmp(qr_data, last_qr_data) != 0) {
            strncpy(last_qr_data, qr_data, sizeof(last_qr_data));
            int a, b;
            sscanf(qr_data, "%1d%1d", &a, &b);
            marking = 1;
            cAction.row = a;
            cAction.col = b;
            cAction.action = setBomb;

            if (send(sock, &cAction, sizeof(ClientAction), 0) < 0) {
                printf("Failed to send action to server\n");
                close(sock);
                return NULL;
            }
            printf("Action sent to server\n");

            pthread_mutex_lock(&dgist_mutex); 
            pthread_cond_wait(&dgist_cond, &dgist_mutex);

            if(first_check==0 && (a == 0 && b==0)){
                past_row = 0;
                past_col = -1;
                first_check += 1; 
            }else if(first_check == 0 && (a == 4 && b == 4)){
                past_row=4;
                past_col = 5;
                first_check +=1;
            }

            // 알고리즘 수행 및 출력
            int row = a; //dgist.players[0].row;
            int col = b; //dgist.players[0].col;

            enum Direction best_direction = find_best_direction(&dgist, a, b);

            int my_row = row;
            int my_col = col;

            // 새로운 위치로 이동
            switch (best_direction) {
                case UP:
                    printf("up\n");
                    my_row -= 1;
                    break;
                case DOWN:
                    printf("down\n");
                    my_row += 1;
                    break;
                case LEFT:
                    printf("left\n");
                    my_col -= 1;
                    break;
                case RIGHT:
                    printf("right\n");
                    my_col += 1;
                    break;
            }

            int past_x = col - past_col;
            int past_y = past_row - row;
            int present_x = my_col - col;
            int present_y = row - my_row;
            // printf("%d %d %d %d", past_x, past_y, present_x, present_y);

            if (past_x >= 1 && present_x >= 1) {
                printf("go straight\n");
                dir = go_straight;
            } else if (past_x >= 1 && present_y >= 1) {
                printf("turn left\n");
                dir = turn_left;
            } else if (past_x >= 1 && present_y <= -1) {
                printf("turn right\n");
                dir = turn_right;
            } else if (past_x <= -1 && present_x <= -1) {
                printf("go straight\n");
                dir = go_straight;
            } else if (past_x <= -1 && present_y >= 1) {
                printf("turn right\n");
                dir = turn_right;
            } else if (past_x <= -1 && present_y <= -1) {
                printf("turn left\n");
                dir = turn_left;
            } else if (past_y >= 1 && present_y >= 1) {
                printf("go straight\n");
                dir = go_straight;
            } else if (past_y >= 1 && present_x >= 1) {
                printf("turn right\n");
                dir = turn_right;
            } else if (past_y >= 1 && present_x <= -1) {
                printf("turn left\n");
                dir = turn_left;
            } else if (past_y <= -1 && present_y <= -1) {
                printf("go straight\n");
                dir = go_straight;
            } else if (past_y <= -1 && present_x <= -1) {
                printf("turn right\n");
                dir = turn_right;
            } else if (past_y <= -1 && present_x >= 1) {
                printf("turn left\n");
                dir = turn_left;
            } else {
                
            }
            
            marking = 0;

            past_col = col;
            past_row = row;
            row = my_row;
            col = my_col;
            pthread_mutex_unlock(&dgist_mutex);
        }
        pthread_mutex_unlock(&qr_mutex);
    }
    close(sock);
    return NULL;
}

int main(void) {
    pthread_t thread1, thread2, thread3;

    int th1_id = pthread_create(&thread1, NULL, execution, NULL);
    if (th1_id < 0) {
        perror("##### THREAD CREATE ERROR #####");
        exit(0);
    }

    int th2_id = pthread_create(&thread2, NULL, qr_detection, NULL);
    if (th2_id < 0) {
        perror("##### THREAD CREATE ERROR #####");
        exit(0);
    }

    int th3_id = pthread_create(&thread3, NULL, server_communication, NULL);
    if (th3_id < 0) {
        perror("##### THREAD CREATE ERROR #####");
        exit(0);
    }

    pthread_join(thread1, NULL);
    pthread_join(thread2, NULL);
    pthread_join(thread3, NULL);

    return 0;
}
