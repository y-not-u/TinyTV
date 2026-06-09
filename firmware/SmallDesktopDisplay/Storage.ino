//读取保存城市代码
void saveCityCodetoEEP(int * citycode)
{
  for(int cnum=0;cnum<5;cnum++)
  {
    EEPROM.write(CC_addr+cnum,*citycode%100);//城市地址写入城市代码
    EEPROM.commit();//保存更改的数据
    *citycode = *citycode/100;
    delay(5);
  }
}
void readCityCodefromEEP(int * citycode)
{
  for(int cnum=5;cnum>0;cnum--)
  {          
    *citycode = *citycode*100;
    *citycode += EEPROM.read(CC_addr+cnum-1); 
    delay(5);
  }
}

//wifi ssid，psw保存到eeprom
void savewificonfig()
{
  //开始写入
  uint8_t *p = (uint8_t*)(&wificonf);
  for (int i = 0; i < sizeof(wificonf); i++)
  {
    EEPROM.write(i + wifi_addr, *(p + i)); //在闪存内模拟写入
  }
  delay(10);
  EEPROM.commit();//执行写入ROM
  delay(10);
}
//删除原有eeprom中的信息
void deletewificonfig()
{
  config_type deletewifi ={{""},{""}};
  uint8_t *p = (uint8_t*)(&deletewifi);
  for (int i = 0; i < sizeof(deletewifi); i++)
  {
    EEPROM.write(i + wifi_addr, *(p + i)); //在闪存内模拟写入
  }
  delay(10);
  EEPROM.commit();//执行写入ROM
  delay(10);
}

//从eeprom读取WiFi信息ssid，psw
void readwificonfig()
{
  uint8_t *p = (uint8_t*)(&wificonf);
  for (int i = 0; i < sizeof(wificonf); i++)
  {
    *(p + i) = EEPROM.read(i + wifi_addr);
  }
  // EEPROM.commit();
  // ssid = wificonf.stassid;
  // pass = wificonf.stapsw;
  Serial.printf("Read WiFi Config.....\r\n");
  Serial.printf("SSID:%s\r\n",wificonf.stassid);
  Serial.printf("PSW:%s\r\n",wificonf.stapsw);
  Serial.printf("Connecting.....\r\n");
}

void saveStockConfig()
{
  EEPROM.write(stock_marker_addr, 0x53);
  for(uint8_t group = 0; group < 2; group++)
  {
    for(uint8_t index = 0; index < 3; index++)
    {
      int offset = stock_addr + ((group * 3 + index) * 16);
      for(int i = 0; i < 16; i++)
      {
        EEPROM.write(offset + i, stockCodes[group][index][i]);
      }
    }
  }
  EEPROM.commit();
}

void readStockConfig()
{
  bool hasSavedStock = false;
  bool hasStockMarker = EEPROM.read(stock_marker_addr) == 0x53;
  for(uint8_t group = 0; group < 2; group++)
  {
    for(uint8_t index = 0; index < 3; index++)
    {
      char code[16];
      int offset = stock_addr + ((group * 3 + index) * 16);
      for(int i = 0; i < 15; i++)
      {
        code[i] = EEPROM.read(offset + i);
        if(code[i] == (char)0xff)
          code[i] = '\0';
      }
      code[15] = '\0';
      if(hasStockMarker)
      {
        strncpy(stockCodes[group][index], code, 15);
        stockCodes[group][index][15] = '\0';
        hasSavedStock = true;
      }
      else if(strlen(code) > 0)
      {
        strncpy(stockCodes[group][index], code, 15);
        stockCodes[group][index][15] = '\0';
        hasSavedStock = true;
      }
    }
  }
}

const char* getStockCode(uint8_t group, uint8_t index)
{
  if(group > 1 || index > 2)
    return "";
  return stockCodes[group][index];
}

void setStockCode(uint8_t group, uint8_t index, String code)
{
  if(group > 1 || index > 2)
    return;
  code.trim();
  code.toUpperCase();
  if(code.length() > 15)
    code = code.substring(0, 15);
  code.toCharArray(stockCodes[group][index], 16);
}

uint8_t currentStockGroup()
{
  int currentHour = hour();
  if(currentHour >= 6 && currentHour < 18)
    return 0;
  return 1;
}
