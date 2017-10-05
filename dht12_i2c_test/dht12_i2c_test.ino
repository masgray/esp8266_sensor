#include <Wire.h>

constexpr uint8_t i2cAddr = 0x5c;

static float temperature  = NAN;
static float humidity     = NAN;

void setup() 
{
  Serial.begin(115200);
  delay(100);
  Serial.println();
  Serial.println("DHT12 i2c test");
  Wire.begin(5, 4);
}

void loop() 
{
  if (ReadDHT12Sensor())
  {
    Serial.print("T = ");
    Serial.print(temperature);
    Serial.println(" 'C");
    Serial.print("H = ");
    Serial.print(humidity);
    Serial.println(" %");
  }
  delay(3000);
}

bool i2cRead(uint8_t reg, uint8_t* buffer)
{
  Wire.beginTransmission(i2cAddr);
  Wire.write(reg);
  Wire.endTransmission();
  auto n = Wire.requestFrom(i2cAddr, (uint8_t)5);
  if (n != 5)
    return false;
  for (int i = 0; i < 5; ++i)
    buffer[i] = Wire.read();
  return true;
}

bool ReadDHT12Sensor(void)
{
    uint8_t buf[10] = {0};
    if (!i2cRead(0, &buf[0]))
    {
      Serial.println("Error reading DHT12...");
      return false;
    }
    char s[16] = {0};
    uint8_t crc = 0;
    for (int i = 0; i < 4; ++i)
       crc += buf[i];
    if (crc != buf[4])
    {
        sprintf(s, "%02d %02d %02d %02d %02d", buf[0], buf[1], buf[2], buf[3], buf[4]);
        Serial.println(s);
        Serial.println("CRC wrong!");
        delay(3000);
        return false;
    }
    temperature  = buf[2] + (float)buf[3] / 10;
    humidity     = buf[0] + (float)buf[1] / 10;
    return true;
}

