#include <Wire.h>  
#include <MPU6050_tockn.h>
#include "Arduino.h"
#include "LedControl.h"
#include "Delay.h"

MPU6050 mpu6050(Wire);

#define MATRIX_A 1
#define MATRIX_B 0

// 点阵屏
#define PIN_DATAIN 3  // DIN 引脚
#define PIN_CLK 2     // CLK 引脚
#define PIN_LOAD 1    // CS 引脚

// 点阵屏安装方向
#define ROTATION_OFFSET 90

int last_direction = 0;  // 记录上一次的方向

int gravity;
LedControl lc = LedControl(PIN_DATAIN, PIN_CLK, PIN_LOAD, 2);
NonBlockDelay d;
int resetCounter = 0;

// 定义结构体，用于保存坐标
struct coord {
  int x;
  int y;
};

// 返回坐标点的下方坐标
coord getDown(int x, int y) {
  coord xy;
  xy.x = x - 1;
  xy.y = y + 1;
  return xy;
}

// 返回坐标点的左方坐标
coord getLeft(int x, int y) {
  coord xy;
  xy.x = x - 1;
  xy.y = y;
  return xy;
}

// 返回坐标点的右方坐标
coord getRight(int x, int y) {
  coord xy;
  xy.x = x;
  xy.y = y + 1;
  return xy;
}

// 判断某个坐标点是否可以向左移动
bool canGoLeft(int addr, int x, int y) {
  if (x == 0) return false;               // 边界检查，如果在左边界，返回false
  return !lc.getXY(addr, getLeft(x, y));  // 如果左侧坐标没有点亮，则返回true
}

// 判断某个坐标点是否可以向右移动
bool canGoRight(int addr, int x, int y) {
  if (y == 7) return false;                // 边界检查，如果在右边界，返回false
  return !lc.getXY(addr, getRight(x, y));  // 如果右侧坐标没有点亮，则返回true
}

// 判断某个坐标点是否可以向下移动
bool canGoDown(int addr, int x, int y) {
  if (y == 7) return false;  // 边界检查，如果在底部，返回false
  if (x == 0) return false;  // 边界检查，如果在左边界，返回false
  // 检查左下和右下两个坐标，如果它们都没有点亮，则返回true
  if (!canGoLeft(addr, x, y)) return false;
  if (!canGoRight(addr, x, y)) return false;
  return !lc.getXY(addr, getDown(x, y));  // 如果下方坐标没有点亮，则返回true
}

// 将指定坐标点下移一个位置
void goDown(int addr, int x, int y) {
  lc.setXY(addr, x, y, false);
  lc.setXY(addr, getDown(x, y), true);
}

// 将指定坐标点左移一个位置
void goLeft(int addr, int x, int y) {
  lc.setXY(addr, x, y, false);
  lc.setXY(addr, getLeft(x, y), true);
}

// 将指定坐标点右移一个位置
void goRight(int addr, int x, int y) {
  lc.setXY(addr, x, y, false);
  lc.setXY(addr, getRight(x, y), true);
}

// 统计指定地址的点阵屏上点亮的粒子数量
int countParticles(int addr) {
  int c = 0;
  for (byte y = 0; y < 8; y++) {
    for (byte x = 0; x < 8; x++) {
      if (lc.getXY(addr, x, y)) {
        c++;
      }
    }
  }
  return c;
}

// 移动指定坐标点上的粒子
bool moveParticle(int addr, int x, int y) {
  if (!lc.getXY(addr, x, y)) {
    return false; // 如果指定坐标点上没有粒子，则返回false
  }

  bool can_GoLeft = canGoLeft(addr, x, y);
  bool can_GoRight = canGoRight(addr, x, y);

  if (!can_GoLeft && !can_GoRight) {
    return false;  // 如果左右两侧都不能移动，则返回false，表示粒子没地儿去了
  }

  bool can_GoDown = canGoDown(addr, x, y);

  if (can_GoDown) {
    goDown(addr, x, y);
  } else if (can_GoLeft && !can_GoRight) {
    goLeft(addr, x, y);
  } else if (can_GoRight && !can_GoLeft) {
    goRight(addr, x, y);
  } else if (random(2) == 1) {  // 随机向左或向右移动
    goLeft(addr, x, y);
  } else {
    goRight(addr, x, y);
  }
  return true;
}

// 在指定地址的点阵屏上填充指定数量的粒子
void fill(int addr, int maxcount) {
  int n = 8;
  byte x, y;
  int count = 0;
  for (byte slice = 0; slice < 2 * n - 1; ++slice) {
    byte z = slice < n ? 0 : slice - n + 1;
    for (byte j = z; j <= slice - z; ++j) {
      y = 7 - j;
      x = (slice - j);
      lc.setXY(addr, x, y, (++count <= maxcount));
    }
  }
}

// 获取当前的重力方向
int getGravity() {
  mpu6050.update();
  float x = mpu6050.getAccX();  // 获取x方向的加速度
  float y = mpu6050.getAccY();  // 获取y方向的加速度
  if (y > 0.8) {
    return 180;  // 重力方向朝下
  } else if (x > 0.8) {
    return 90;   // 重力方向朝左
  } else if (y < -0.8) {
    return 0;    // 
  } else if (x < -0.8) {
    return 270;  // 重力方向朝右
  } else {
    return last_direction; // 其他情况保持上一次的方向
  }
}

// 获取上方的点阵屏地址
int getTopMatrix() {
  return (getGravity() == 90) ? MATRIX_A : MATRIX_B; // 如果重力方向是向左，返回MATRIX_A，否则返回MATRIX_B
}

// 获取下方的点阵屏地址
int getBottomMatrix() {
  return (getGravity() != 90) ? MATRIX_A : MATRIX_B; // 如果重力方向不是向左，返回MATRIX_A，否则返回MATRIX_B
}

// 重置时间并在上方的点阵屏上填充粒子
void resetTime() {
  for (byte i = 0; i < 2; i++) {
    lc.clearDisplay(i);
  }
  fill(getTopMatrix(), 64);  // 在上方的点阵屏上填充64个粒子
  d.Delay(1 * 1000);
}

// 更新粒子的运动状态
bool updateMatrix() {
  int n = 8;
  bool somethingMoved = false;
  byte x, y;
  bool direction;
  for (byte slice = 0; slice < 2 * n - 1; ++slice) {
    direction = (random(2) == 1);  // 随机确定运动方向
    byte z = slice < n ? 0 : slice - n + 1;
    for (byte j = z; j <= slice - z; ++j) {
      y = direction ? (7 - j) : (7 - (slice - j));
      x = direction ? (slice - j) : j;
      if (moveParticle(MATRIX_B, x, y)) {
        somethingMoved = true;
      };
      if (moveParticle(MATRIX_A, x, y)) {
        somethingMoved = true;
      }
    }
  }
  return somethingMoved; // 返回是否有粒子移动
}

// 在重力方向为上/下时，实现粒子的跨屏移动
bool dropParticle() {
  if (d.Timeout()) {
    d.Delay(1 * 1000);

    if (gravity == 0) { // 重力方向朝上
      bool particleMoved = false;

      if (lc.getRawXY(MATRIX_A, 0, 0) && !lc.getRawXY(MATRIX_B, 7, 7)) {
        lc.invertRawXY(MATRIX_A, 0, 0);
        lc.invertRawXY(MATRIX_B, 7, 7);
        return true; // 发生了跨屏移动，返回true
      }
    } else if (gravity == 180) { // 重力方向朝下
      bool particleMoved = false;
      if (!lc.getRawXY(MATRIX_A, 0, 0) && lc.getRawXY(MATRIX_B, 7, 7)) {
        lc.invertRawXY(MATRIX_A, 0, 0);
        lc.invertRawXY(MATRIX_B, 7, 7);
        return true; // 发生了跨屏移动，返回true
      }
    }
  }
  return false; // 没有发生跨屏移动，返回false
}

void setup() {
  Serial.begin(9600);

  Wire.setSDA(12);  // 更改I2C引脚，将SDA设置为12引脚
  Wire.setSCL(13);  // 更改I2C引脚，将SCL设置为13引脚
  Wire.begin();
  mpu6050.begin();
  // mpu6050.calcGyroOffsets(true);  // 陀螺仪校准，需静止3秒，不用也行

  randomSeed(analogRead(14));  // 读悬空引脚，获得随机数种子

  for (byte i = 0; i < 2; i++) {
    lc.shutdown(i, false);
    lc.setIntensity(i, 2);  // 控制粒子的亮度强度
  }
  resetTime(); // 初始化点阵屏上的粒子
}

void loop() {
  delay(50);
  gravity = getGravity(); // 获取当前重力方向
  last_direction = gravity; // 保存当前重力方向为上一次的方向
  lc.setRotation((ROTATION_OFFSET + gravity) % 360); // 设置点阵屏的显示方向

  bool moved = updateMatrix(); // 更新粒子的运动状态
  bool dropped = dropParticle(); // 实现粒子的跨屏移动
}

