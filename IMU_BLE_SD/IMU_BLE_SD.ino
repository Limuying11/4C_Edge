#include <IIC.h>
#include <MPU6050.h>
#include <MPU6050_Reg.h>
#include <SPI.h>
#include <SD.h>

unsigned char MPU6050_ID = 0;
int16_t AX, AY, AZ, GX, GY, GZ;
// 校准偏移量（根据实际校准结果修改！）
const int16_t ax_offset = -1254;
const int16_t ay_offset = -506;
const int16_t az_offset = 1780;
const int16_t gx_offset = 66;
const int16_t gy_offset = 5;
const int16_t gz_offset = 16;
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>

std::string rx_buf;

// BLE 服务UUID和特征UUID（可自定义）
#define SERVICE_UUID        "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define CHARACTERISTIC_RX_UUID "beb5483e-36e1-4688-b7f5-ea07361b26a8"
#define CHARACTERISTIC_TX_UUID "beb5483e-36e1-4688-b7f5-ea07361b26a9"

BLECharacteristic *pTxCharacteristic;
bool deviceConnected = false;

// 自定义 SPI 引脚配置（可修改为你实际使用的引脚）
const uint8_t SD_CS_PIN   = 7;  // 片选引脚
const uint8_t SD_SCK_PIN  = 2;  // 时钟引脚
const uint8_t SD_MISO_PIN = 10;  // 主输入从输出
const uint8_t SD_MOSI_PIN = 3;  // 主输出从输入

// 自定义 SPI 实例（避免与默认 SPI 冲突）
SPIClass sd_spi(HSPI);  // 使用 HSPI 总线


bool initSD(uint8_t csPin, uint8_t sckPin, uint8_t misoPin, uint8_t mosiPin) {
  // 配置 SPI 引脚
  sd_spi.begin(sckPin, misoPin, mosiPin, csPin);
  
  // 初始化 SD 卡
  if (!SD.begin(csPin, sd_spi)) {
    Serial.println("SD 卡初始化失败！");
    return false;
  }
  
  // 打印 SPI 配置信息
  Serial.println("SD 卡 SPI 配置：");
  Serial.printf("SCK:GPIO%d, MISO:GPIO%d, MOSI:GPIO%d, CS:GPIO%d\n", 
                sckPin, misoPin, mosiPin, csPin);
  return true;
}

/**
 * @brief 写入数据到指定文件
 * @param filename 文件名
 * @param data     要写入的数据
 */
void writeFile(const char* filename, const char* data) {
  File file = SD.open(filename, FILE_WRITE);
  if (!file) {
    Serial.println("文件打开失败");
    return;
  }
  
  if (file.print(data)) {
    Serial.println("数据写入成功");
  } else {
    Serial.println("写入失败");
  }
  
  file.close();
}

// 文件读取函数
void readTestFile(const char* filename) {
  Serial.printf("\n尝试打开文件: %s\n", filename);

  File file = SD.open(filename);
  if (!file) {
    Serial.println("打开文件失败！");
    Serial.println("可能原因：");
    Serial.println("- 文件不存在");
    Serial.println("- 文件路径错误");
    Serial.println("- 文件系统损坏");
    return;
  }

  Serial.println("文件内容：");
  Serial.println("----------------------------------------");
  
  // 逐行读取文件
  while (file.available()) {
    String line = file.readStringUntil('\n');
    line.trim(); // 去除换行符
    if (line.length() > 0) {
      Serial.print(">> ");
      Serial.println(line);
    }
  }
  
  file.close();
  Serial.println("----------------------------------------");
  Serial.println("文件读取完成");
}


// BLE 服务器回调
class MyServerCallbacks: public BLEServerCallbacks {
  void onConnect(BLEServer* pServer) {
    deviceConnected = true;
    Serial.println("设备已连接");
  };

  void onDisconnect(BLEServer* pServer) {
    deviceConnected = false;
    Serial.println("设备已断开");
    // 重新启动广播以便重新连接
    pServer->startAdvertising();
  }
};

// BLE 特征回调（接收数据）
class MyCallbacks: public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic *pCharacteristic) {
    std::string rxValue = pCharacteristic->getValue();
    rx_buf = rxValue+"\n";
    //writeFile("/log.txt", rx_buf.c_str());
    if (rxValue.length() > 0) {
      Serial.print("收到数据: ");
      for (int i = 0; i < rxValue.length(); i++) {
        Serial.print(rxValue[i]);
      }
      Serial.println();
    }
  }
};

void setup() {
 mpu6050_init();
  Serial.println("启动 BLE...");

  // 初始化 BLE
  BLEDevice::init("ESP32C3-BLE"); // 设备名称

  // 创建 BLE 服务器
  BLEServer *pServer = BLEDevice::createServer();
  pServer->setCallbacks(new MyServerCallbacks());

  // 创建 BLE 服务
  BLEService *pService = pServer->createService(SERVICE_UUID);

  // 创建发送特征（支持通知）
  pTxCharacteristic = pService->createCharacteristic(
    CHARACTERISTIC_TX_UUID,
    BLECharacteristic::PROPERTY_NOTIFY
  );
  pTxCharacteristic->addDescriptor(new BLE2902());

  // 创建接收特征（支持写入）
  BLECharacteristic *pRxCharacteristic = pService->createCharacteristic(
    CHARACTERISTIC_RX_UUID,
    BLECharacteristic::PROPERTY_WRITE
  );
  pRxCharacteristic->setCallbacks(new MyCallbacks());

  // 启动服务和广播
  pService->start();
  pServer->getAdvertising()->start();
  Serial.println("等待客户端连接...");

  // if (!initSD(SD_CS_PIN, SD_SCK_PIN, SD_MISO_PIN, SD_MOSI_PIN)) {
  //   Serial.println("系统启动失败，请检查硬件连接");
  //   while (1); // 停止执行
  // }
  // writeFile("/test.txt", "Hello Custom SPI Pins!");

  // readTestFile("/test.txt");
}

void loop() {
  static uint32_t last0 = 0;
  //static uint32_t last1 = 0;
  if(millis()-last0 >500)
  {
    mpu6050_pro();
    last0 = millis();
  }
  // if(millis()-last1 >2000)
  // {
  //   if (deviceConnected) {
    
  //   String txValue = "Hello BLE: " + String(millis() / 1000);
  //   pTxCharacteristic->setValue(txValue.c_str());
  //   pTxCharacteristic->notify();
  //   Serial.println("发送数据: " + txValue);
    
  //   }
  //   last1 = millis();
  // }
  // static uint32_t last2 = 0;
  // if (millis() - last2 > 5000) {
  //   String data = "Timestamp: " + String(millis()) + "\n";
  //   writeFile("/log.txt", data.c_str());
  //   last2 = millis();
}

void mpu6050_init()
{
  Serial.begin(921600);
  MPU6050_Init();
  delayMicroseconds(10);
  MPU6050_ID = MPU6050_GetID();
}

void mpu6050_pro()
{
  MPU6050_GetData(&AX, &AY, &AZ, &GX, &GY, &GZ);
 // 转换为实际物理量
  const float accel_scale = 	4096.0; // ±2g 量程（16384 LSB/g）
  const float gyro_scale = 131.0;    // ±250°/s 量程（131 LSB/°/s）

  float ax_g = AZ / accel_scale;
  float ay_g = AY / accel_scale;
  float az_g = AZ / accel_scale;

  float gx_dps = GX / gyro_scale;
  float gy_dps = GY / gyro_scale;
  float gz_dps = GZ / gyro_scale;

  Serial.print("加速度 (g): ");
  Serial.print(ax_g); Serial.print(", ");
  Serial.print(ay_g); Serial.print(", ");
  Serial.print(az_g); Serial.print("\t");

  Serial.print("陀螺仪 (°/s): ");
  Serial.print(gx_dps); Serial.print(", ");
  Serial.print(gy_dps); Serial.print(", ");
  Serial.println(gz_dps);
  String sensorData1 = "AX: " + String(ax_g, 6) + " g\n";
  String sensorData2=  "AY: " + String(ay_g, 6) + " g\n";
  String sensorData3 = "AZ: " + String(az_g, 6) + " g\n"; 
  //writeFile("/log1.txt", sensorData.c_str());
  String sensorData4 = "GX: " + String(gx_dps, 6) + "°/s\n";
  String sensorData5 = "GY: " + String(gy_dps, 6) + "°/s\n";
  String sensorData6 = "GZ: " + String(gz_dps, 6) + "°/s\n";
  if (deviceConnected) {
    pTxCharacteristic->setValue(sensorData1.c_str());
    pTxCharacteristic->notify();
    Serial.println("发送数据: " + sensorData1);
    pTxCharacteristic->setValue(sensorData2.c_str());
    pTxCharacteristic->notify();
    Serial.println("发送数据: " + sensorData2);
    pTxCharacteristic->setValue(sensorData3.c_str());
    pTxCharacteristic->notify();
    Serial.println("发送数据: " + sensorData3);
    pTxCharacteristic->setValue(sensorData4.c_str());
    pTxCharacteristic->notify();
    Serial.println("发送数据: " + sensorData4);
    pTxCharacteristic->setValue(sensorData5.c_str());
    pTxCharacteristic->notify();
    Serial.println("发送数据: " + sensorData5);
    pTxCharacteristic->setValue(sensorData6.c_str());
    pTxCharacteristic->notify();
    Serial.println("发送数据: " + sensorData6);
  }

}

