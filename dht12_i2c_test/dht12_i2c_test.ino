#include <Wire.h>

static int humidityInt     = 0;
static int humidityFrac    = 0;
static int temperatureInt  = 0;
static int temperatureFrac = 0;
constexpr uint8_t i2cAddr = 0x5c << 1;

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
  ReadDHT12Sensor();
  delay(3000);
}

bool i2cRead(uint8_t reg, uint8_t* buffer)
{
  Wire.beginTransmission(i2cAddr);
  Wire.write(reg);
  Wire.endTransmission();
  auto n = Wire.requestFrom(i2cAddr, 5);
  if (n != 5)
    return false;
  for (int i = 0; i < 5; ++i)
    buffer[i] = Wire.read();
  return true;
}

bool ReadDHT12Sensor(void)
{
    uint8_t buf[10] = {0};
    if (i2cRead(0, &buf[0]))
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
    humidityInt     = buf[0];
    humidityFrac    = buf[1];
    temperatureInt  = buf[2];
    temperatureFrac = buf[3];
    return true;
}

