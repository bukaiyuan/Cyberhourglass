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

int last_direction = 0;  //记录上一次的方向

int gravity;
LedControl lc = LedControl(PIN_DATAIN, PIN_CLK, PIN_LOAD, 2);
NonBlockDelay d;
int resetCounter = 0;



coord getDown(int x, int y) {
  coord xy;
  xy.x = x - 1;
  xy.y = y + 1;
  return xy;
}
coord getLeft(int x, int y) {
  coord xy;
  xy.x = x - 1;
  xy.y = y;
  return xy;
}
coord getRight(int x, int y) {
  coord xy;
  xy.x = x;
  xy.y = y + 1;
  return xy;
}

bool canGoLeft(int addr, int x, int y) {
  if (x == 0) return false;               
  return !lc.getXY(addr, getLeft(x, y)); 
}
bool canGoRight(int addr, int x, int y) {
  if (y == 7) return false;                
  return !lc.getXY(addr, getRight(x, y));  
}
bool canGoDown(int addr, int x, int y) {
  if (y == 7) return false;  
  if (x == 0) return false;  
  if (!canGoLeft(addr, x, y)) return false;
  if (!canGoRight(addr, x, y)) return false;
  return !lc.getXY(addr, getDown(x, y));  
}

void goDown(int addr, int x, int y) {
  lc.setXY(addr, x, y, false);
  lc.setXY(addr, getDown(x, y), true);
}
void goLeft(int addr, int x, int y) {
  lc.setXY(addr, x, y, false);
  lc.setXY(addr, getLeft(x, y), true);
}
void goRight(int addr, int x, int y) {
  lc.setXY(addr, x, y, false);
  lc.setXY(addr, getRight(x, y), true);
}

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

bool moveParticle(int addr, int x, int y) {
  if (!lc.getXY(addr, x, y)) {
    return false;
  }

  bool can_GoLeft = canGoLeft(addr, x, y);
  bool can_GoRight = canGoRight(addr, x, y);

  if (!can_GoLeft && !can_GoRight) {
    return false;  // we're stuck
  }

  bool can_GoDown = canGoDown(addr, x, y);

  if (can_GoDown) {
    goDown(addr, x, y);
  } else if (can_GoLeft && !can_GoRight) {
    goLeft(addr, x, y);
  } else if (can_GoRight && !can_GoLeft) {
    goRight(addr, x, y);
  } else if (random(2) == 1) {  
    goLeft(addr, x, y);
  } else {
    goRight(addr, x, y);
  }
  return true;
}

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


int getGravity() {
  mpu6050.update();
  float x = mpu6050.getAccX();
  float y = mpu6050.getAccY();
  if (y > 0.8) {
    return 180;
  } 
  else if (x > 0.8) {
    return 90;
  } 
  else if (y < -0.8) {
    return 0;
  } 
  else if (x < -0.8) {
    return 270;
  } 
  else {
    return last_direction;
  }
}

int getTopMatrix() {
  return (getGravity() == 90) ? MATRIX_A : MATRIX_B;
}
int getBottomMatrix() {
  return (getGravity() != 90) ? MATRIX_A : MATRIX_B;
}

void resetTime() {
  for (byte i = 0; i < 2; i++) {
    lc.clearDisplay(i);
  }
  fill(getTopMatrix(), 64);  
  d.Delay(1 * 1000);
}

bool updateMatrix() {  
  int n = 8;
  bool somethingMoved = false;
  byte x, y;
  bool direction;
  for (byte slice = 0; slice < 2 * n - 1; ++slice) {
    direction = (random(2) == 1);  
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
  return somethingMoved;
}

bool dropParticle() {  //沙子跨屏移动
  if (d.Timeout()) 
  {
    d.Delay(1 * 1000);

    if (gravity == 0 )   // 只有方向为上/下的时候才跨屏移动
    {
      bool particleMoved = false;

      if (lc.getRawXY(MATRIX_A, 0, 0) && !lc.getRawXY(MATRIX_B, 7, 7)) {       
      lc.invertRawXY(MATRIX_A, 0, 0);
      lc.invertRawXY(MATRIX_B, 7, 7);
      return true;
      }
    }
    else if (gravity == 180)  // 下
    {
      bool particleMoved = false;
      if (!lc.getRawXY(MATRIX_A, 0, 0) && lc.getRawXY(MATRIX_B, 7, 7)) {       
      lc.invertRawXY(MATRIX_A, 0, 0);
      lc.invertRawXY(MATRIX_B, 7, 7);
      return true;
      }
    }
  }
  return false;
}


void setup() {

  Serial.begin(9600);

  Wire.setSDA(12);  // 更改I2C引脚 
  Wire.setSCL(13);
  Wire.begin();
  mpu6050.begin();
  // mpu6050.calcGyroOffsets(true);  // 陀螺仪校准，需静止3秒，不用也行

  randomSeed(analogRead(14));  // 读悬空引脚，获得随机数种子

  for (byte i = 0; i < 2; i++) {
    lc.shutdown(i, false);
    lc.setIntensity(i, 2);  //控制粒子的亮度强度
  }
  resetTime();
}



void loop() {
  delay(50);
  gravity = getGravity();
  last_direction = gravity;
  lc.setRotation((ROTATION_OFFSET + gravity) % 360);

  bool moved = updateMatrix();
  bool dropped = dropParticle();
}
