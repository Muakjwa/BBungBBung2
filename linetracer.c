#include <wiringPi.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>  // usleep 함수 사용
#include <fcntl.h>
#include <linux/i2c-dev.h>
#include <sys/ioctl.h>
#include <math.h>
#include <signal.h>
#include <time.h>
#include <pthread.h>
#include "qr.h"

#define I2C_DEV "/dev/i2c-1"
#define I2C_ADDR 0x16

#define Tracking_Left1 27
#define Tracking_Left2 22
#define Tracking_Right1 17
#define Tracking_Right2 4 

int fd;

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
    if (length == 1){
        unsigned char buffer[2];
        buffer[0] = reg;
        buffer[1] = data[0];
        if (write(fd, buffer, length + 1) != length + 1) {
            perror("##### BLOCK WRITE FAIL #####");
        }
    }

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

    if ((Tracking_Left1Value == LOW || Tracking_Left2Value == LOW) && Tracking_Right2Value == LOW) {
        car_spin_right(70, 30);
        usleep(100000);  // 0.2초

    } else if (Tracking_Left1Value == LOW && (Tracking_Right1Value == LOW || Tracking_Right2Value == LOW)) {
        car_spin_left(30, 70);
        usleep(100000);  // 0.2초

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
        car_run(80,80);
    }
}

void * execution() {
    i2c_init();
    setup();

    time_t start_time = time(NULL);
    time_t cur_time;

    while (1) {
        cur_time = time(NULL);
        if(difftime(cur_time, start_time)>30){
            break;
        }

        tracing();
        delay(50);
    }

    car_stop();
    close(fd);
}

void * qr_detection(){
    qr_detect();
}

int main(void) {
    pthread_t thread1, thread2;

    int th1_id = pthread_create(&thread1, NULL, execution, NULL);
    if (th1_id < 0)
    {
        perror("##### THREAD CREATE ERROR #####");
        exit(0);
    }
    int th2_id = pthread_create(&thread2, NULL, qr_detection, NULL);
    if (th2_id < 0)
    {
        perror("##### THREAD CREATE ERROR #####");
        exit(0);
    }

    pthread_join(thread1, NULL);
    pthread_join(thread2, NULL);

    return 0;
}
