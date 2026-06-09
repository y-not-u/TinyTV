/* *****************************************************************
 * 
 * SmallDesktopDisplay
 *    小型桌面显示器
 * 
 * 原  作  者：Misaka
 * 修      改：微车游
 * 讨  论  群：811058758、887171863
 * 创 建 日 期：2021.07.19
 * 最后更改日期：2021.09.18
 * 更 改 说 明：V1.1添加串口调试，波特率115200\8\n\1；增加版本号显示。
 *            V1.2亮度和城市代码保存到EEPROM，断电可保存
 *            V1.3.1 更改smartconfig改为WEB配网模式，同时在配网的同时增加亮度、屏幕方向设置。
 *            V1.3.2 增加wifi休眠模式，仅在需要连接的情况下开启wifi，其他时间关闭wifi。增加wifi保存至eeprom（目前仅保存一组ssid和密码）
 *            V1.3.3  修改WiFi保存后无法删除的问题。目前更改为使用串口控制，输入 0x05 重置WiFi数据并重启。
 *                    增加web配网以及串口设置天气更新时间的功能。
 *            V1.3.4  修改web配网页面设置，将wifi设置页面以及其余设置选项放入同一页面中。
 *                    增加web页面设置是否使用DHT传感器。（使能DHT后才可使用）
 *            V1.4    增加web服务器，使用web网页进行设置。由于使用了web服务器，无法开启WiFi休眠。
 *                    注意，此版本中的DHT11传感器和太空人图片选择可以通过web网页设置来进行选择，无需通过使能标志来重新编译。
 * 
 * 引 脚 分 配： SCK  GPIO14
 *             MOSI  GPIO13
 *             RES   GPIO2
 *             DC    GPIO0
 *             LCDBL GPIO5
 *             
 *             增加DHT11温湿度传感器，传感器接口为 GPIO 12
 * 
 *    感谢群友 @你别失望  提醒发现WiFi保存后无法重置的问题，目前已解决。详情查看更改说明！
 * *****************************************************************/
#define Version  "SDD V1.4"
/* *****************************************************************
 *  库文件、头文件
 * *****************************************************************/
#include "ArduinoJson.h"
#include <TimeLib.h>
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <ESP8266WebServer.h>
#include <WiFiUdp.h>
#include <ESP8266mDNS.h>
#include <TFT_eSPI.h> 
#include <SPI.h>
#include <TJpg_Decoder.h>
#include <EEPROM.h>
#include "qr.h"
#include "number.h"
#include "weathernum.h"


/* *****************************************************************
 *  配置使能位
 * *****************************************************************/
//WEB配网使能标志位----WEB配网打开后会默认关闭smartconfig功能
#define WM_EN   1
//Web服务器使能标志位----打开后将无法使用wifi休眠功能。
#define WebSever_EN  1



//注意，此版本中的DHT11传感器和太空人图片选择可以通过web网页设置来进行选择，无需通过使能标志来重新编译。
//设定DHT11温湿度传感器使能标志
#define DHT_EN  1
//设置太空人图片是否使用
#define imgAst_EN 1



#if WM_EN
#include <WiFiManager.h>
//WiFiManager 参数
WiFiManager wm; // global wm instance
// WiFiManagerParameter custom_field; // global param ( for non blocking w params )
#endif

#if DHT_EN
#include "DHT.h"
#define DHTPIN  12
#define DHTTYPE DHT11
DHT dht(DHTPIN,DHTTYPE);
#endif





/* *****************************************************************
 *  字库、图片库
 * *****************************************************************/
#include "font/ZdyLwFont_20.h"
#include "img/misaka.h"
#include "img/temperature.h"
#include "img/humidity.h"

#if imgAst_EN
#include "img/pangzi/i0.h"
#include "img/pangzi/i1.h"
#include "img/pangzi/i2.h"
#include "img/pangzi/i3.h"
#include "img/pangzi/i4.h"
#include "img/pangzi/i5.h"
#include "img/pangzi/i6.h"
#include "img/pangzi/i7.h"
#include "img/pangzi/i8.h"
#include "img/pangzi/i9.h"

int Anim = 0;           //太空人图标显示指针记录
int AprevTime = 0;      //太空人更新时间记录
#endif



/* *****************************************************************
 *  参数设置
 * *****************************************************************/

struct config_type
{
  char stassid[32];//定义配网得到的WIFI名长度(最大32字节)
  char stapsw[64];//定义配网得到的WIFI密码长度(最大64字节)
};

//---------------修改此处""内的信息--------------------
//如开启WEB配网则可不用设置这里的参数，前一个为wifi ssid，后一个为密码
config_type wificonf ={{""},{""}};


int updateweater_time = 10; //天气更新时间  X 分钟
int LCD_Rotation = 0;   //LCD屏幕方向
int LCD_BL_PWM = 100;//屏幕亮度20-100，默认100
String cityCode = "101010100";  //天气城市代码 北京:101010100
String stockCode = "AAPL";
String stockCodes[2][3] = {
  {"AAPL", "MSFT", "GOOG"},
  {"TSLA", "NVDA", "AMZN"}
};
//----------------------------------------------------

//LCD屏幕相关设置
TFT_eSPI tft = TFT_eSPI();  // 引脚请自行配置tft_espi库中的 User_Setup.h文件
TFT_eSprite clk = TFT_eSprite(&tft);
#define LCD_BL_PIN 5    //LCD背光引脚
#define LCD_BL_MIN 20
#define LCD_BL_MAX 100
#define TOUCH_EN 1
#define TOUCH_AUTO_PIN 1
#define TOUCH_PIN 4           // GPIO4 / D2, used when TOUCH_AUTO_PIN is disabled.
#define TOUCH_DEBOUNCE_MS 30
#define TOUCH_DOUBLE_CLICK_MS 450
#define TOUCH_LONG_PRESS_MS 1800
uint16_t bgColor = 0x0000;

//其余状态标志位
uint8_t Wifi_en = 1; //wifi状态标志位  1：打开    0：关闭
uint8_t UpdateWeater_en = 0; //更新时间标志位
int prevTime = 0;       //滚动显示更新标志位
int DHT_img_flag = 0;   //DHT传感器使用标志位


//EEPROM参数存储地址位
int BL_addr = 1;//被写入数据的EEPROM地址编号  1亮度
int Ro_addr = 2; //被写入数据的EEPROM地址编号  2 旋转方向
int DHT_addr = 3;//3 DHT使能标志位
int UpWeT_addr = 4; //4 更新时间记录
int CC_addr = 10;//被写入数据的EEPROM地址编号  10城市
int wifi_addr = 30; //被写入数据的EEPROM地址编号  20wifi-ssid-psw
int stock_marker_addr = 149; //股票配置版本标记
int stock_addr = 150; //股票代码，每个最多15字节，白天/晚上各3个

time_t prevDisplay = 0;       //显示时间显示记录
unsigned long weaterTime = 0; //天气更新时间记录
String SMOD = "";//串口数据存储


/*** Component objects ***/
Number      dig;
WeatherNum  wrat;


uint32_t targetTime = 0;   

int tempnum = 0;   //温度百分比
int huminum = 0;   //湿度百分比
int tempcol =0xffff;   //温度显示颜色
int humicol =0xffff;   //湿度显示颜色
unsigned char Hour_sign   = 60;
unsigned char Minute_sign = 60;
unsigned char Second_sign = 60;

//Web网站服务器
ESP8266WebServer server(80);// 建立esp8266网站服务器对象


//NTP服务器参数
static const char ntpServerName[] = "ntp6.aliyun.com";
static const char* ntpServerNames[] = {
  "ntp.aliyun.com",
  "ntp6.aliyun.com",
  "pool.ntp.org"
};
const int timeZone = 8;     //东八区

//wifi连接UDP设置参数
WiFiUDP Udp;
WiFiClient wificlient;
unsigned int localPort = 8000;
float duty=0;

enum AppPage
{
  PAGE_CLOCK = 0,
  PAGE_STOCK,
  PAGE_WEATHER,
  PAGE_SETTINGS
};

AppPage currentPage = PAGE_CLOCK;
const char* pageNames[] = {"Clock", "Stock", "Weather", "Settings"};
const int pageCount = sizeof(pageNames) / sizeof(pageNames[0]);
unsigned long pageRenderTime = 0;

extern byte loadNum;


//函数声明
time_t getNtpTime();
bool syncClock(uint8_t retryCount);
void digitalClockDisplay(int reflash_en);
String week();
String monthDay();
void sendNTPpacket(IPAddress &address);
bool tft_output(int16_t x, int16_t y, uint16_t w, uint16_t h, uint16_t* bitmap);
void loading(byte delayTime);
void humidityWin();
void humidityWinAt(int x, int y);
void tempWin();
void tempWinAt(int x, int y);
void LCD_reflash(int en);
void Weather_drawDetails(const String details[], int detailCount);
void Weather_showStatus(const char* title, const char* message);
void savewificonfig();
void readwificonfig();
void deletewificonfig();
void getCityCode();
void getCityWeater();
bool weaterData(String *cityDZ,String *dataSK,String *dataFC);
void saveStockConfig();
void readStockConfig();
String getStockCode(uint8_t group, uint8_t index);
void setStockCode(uint8_t group, uint8_t index, String code);
uint8_t currentStockGroup();
void Serial_set();
void setBacklight(int value, bool saveToEeprom);
void Page_next();
void Page_confirm();
void Page_home();
void Page_render(bool force);
void Page_renderClock(bool force);
void Page_renderStock(bool force);
void Page_renderWeather(bool force);
void Page_renderSettings(bool force);
void Page_drawHeader(const char* title);
#if imgAst_EN
void imgAnim();
#endif
#if DHT_EN
void IndoorTem();
#endif
#if TOUCH_EN
void Touch_init();
void Touch_poll();
void Touch_onSingleClick();
void Touch_onDoubleClick();
void Touch_onLongPress();
#endif
#if WebSever_EN
void Web_Sever_Init();
void Web_Sever();
void handleSetPage();
void Web_sever_Win();
#endif
#if WM_EN
void Web_win();
void Webconfig();
String getParam(String name);
void saveParamCallback();
#endif
#if !WM_EN
void SmartConfig(void);
#endif
void saveCityCodetoEEP(int * citycode);
void readCityCodefromEEP(int * citycode);

/* *****************************************************************
 *  函数
 * *****************************************************************/

void setup()
{
  Serial.begin(115200);
  EEPROM.begin(1024);
  readStockConfig();
  #if TOUCH_EN
  Touch_init();
  #endif
  // WiFi.forceSleepWake();
  // wm.resetSettings();    //在初始化中使wifi重置，需重新配置WiFi
  
 #if DHT_EN
  dht.begin();
  //从eeprom读取DHT传感器使能标志
  DHT_img_flag = EEPROM.read(DHT_addr);
 #endif
 //从eeprom读取背光亮度设置
  if(EEPROM.read(BL_addr)>=LCD_BL_MIN&&EEPROM.read(BL_addr)<=LCD_BL_MAX)
    LCD_BL_PWM = EEPROM.read(BL_addr); 
  pinMode(LCD_BL_PIN, OUTPUT);
  setBacklight(LCD_BL_PWM, false);
  //从eeprom读取屏幕方向设置
  if(EEPROM.read(Ro_addr)>=0&&EEPROM.read(Ro_addr)<=3)
    LCD_Rotation = EEPROM.read(Ro_addr);
  
  //从eeprom读取天气更新时间
  updateweater_time = EEPROM.read(UpWeT_addr);
  if(updateweater_time <= 0 || updateweater_time > 60)
  {
    updateweater_time = 10;
    EEPROM.write(UpWeT_addr, updateweater_time);
    EEPROM.commit();
  }
  
  

  tft.begin(); /* TFT init */
  tft.invertDisplay(1);//反转所有显示颜色：1反转，0正常
  tft.setRotation(LCD_Rotation);
  tft.fillScreen(0x0000);
  tft.setTextColor(TFT_BLACK, bgColor);

  targetTime = millis() + 1000; 
  readwificonfig();//读取存储的wifi信息
  Serial.print("正在连接WIFI ");
  Serial.println(wificonf.stassid);
  WiFi.begin(wificonf.stassid, wificonf.stapsw);
  
  TJpgDec.setJpgScale(1);
  TJpgDec.setSwapBytes(true);
  TJpgDec.setCallback(tft_output);

  while (WiFi.status() != WL_CONNECTED) 
  {
    loading(30);  
      
    if(loadNum>=194)
    {
      //使能web配网后自动将smartconfig配网失效
      #if WM_EN
      Web_win();
      Webconfig();
      #endif

      #if !WM_EN
      SmartConfig();
      #endif   
      break;
    }
  }
  delay(10); 
  while(loadNum < 194) //让动画走完
  { 
    loading(1);
  }

  if(WiFi.status() == WL_CONNECTED)
  {
    // Serial.print("SSID:");
    // Serial.println(WiFi.SSID().c_str());
    // Serial.print("PSW:");
    // Serial.println(WiFi.psk().c_str());
    strcpy(wificonf.stassid,WiFi.SSID().c_str());//名称复制
    strcpy(wificonf.stapsw,WiFi.psk().c_str());//密码复制
    savewificonfig();
    readwificonfig();
    #if WebSever_EN
    //开启web服务器初始化
    Web_Sever_Init();
    Web_sever_Win();
    delay(10000);
    #endif
  }

  // Serial.print("本地IP： ");
  // Serial.println(WiFi.localIP());
  Serial.println("启动UDP");
  Udp.begin(localPort);
  Serial.println("等待同步...");
  setSyncProvider(getNtpTime);
  setSyncInterval(300);
  syncClock(5);

  

  TJpgDec.setJpgScale(1);
  TJpgDec.setSwapBytes(true);
  TJpgDec.setCallback(tft_output);
  
  int CityCODE = 0;
  readCityCodefromEEP(&CityCODE);
  // for(int cnum=5;cnum>0;cnum--)
  // {          
  //   CityCODE = CityCODE*100;
  //   CityCODE += EEPROM.read(CC_addr+cnum-1); 
  //   delay(5);
  // }
  if(CityCODE>=101000000 && CityCODE<=102000000) 
    cityCode = String(CityCODE);
  else
  {
    cityCode = "101010100";
    int defaultCity = 101010100;
    saveCityCodetoEEP(&defaultCity);
  }
	   
  tft.fillScreen(TFT_BLACK);//清屏
  
  if(currentPage == PAGE_WEATHER)
    getCityWeater();
#if DHT_EN
  if(DHT_img_flag != 0)
  IndoorTem();
#endif
#if !WebSever_EN
  WiFi.forceSleepBegin(); //wifi off
  Serial.println("WIFI休眠......");
  Wifi_en = 0;
#endif
  Page_home();
}



void loop()
{
  #if WebSever_EN
  Web_Sever();
  #endif
  #if TOUCH_EN
  Touch_poll();
  #endif
  Page_render(false);
  Serial_set();
}
