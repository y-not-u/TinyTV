//TFT屏幕输出函数
bool tft_output(int16_t x, int16_t y, uint16_t w, uint16_t h, uint16_t* bitmap)
{
  if ( y >= tft.height() ) return 0;
  tft.pushImage(x, y, w, h, bitmap);
  // Return 1 to decode next block
  return 1;
}

//进度条函数
byte loadNum = 6;
void loading(byte delayTime)//绘制进度条
{
  clk.setColorDepth(8);
  
  clk.createSprite(200, 100);//创建窗口
  clk.fillSprite(0x0000);   //填充率

  clk.drawRoundRect(0,0,200,16,8,0xFFFF);       //空心圆角矩形
  clk.fillRoundRect(3,3,loadNum,10,5,0xFFFF);   //实心圆角矩形
  clk.setTextDatum(CC_DATUM);   //设置文本数据
  clk.setTextColor(TFT_GREEN, 0x0000); 
  clk.drawString("Connecting to WiFi......",100,40,2);
  clk.setTextColor(TFT_WHITE, 0x0000); 
  clk.drawRightString(Version,180,60,2);
  clk.pushSprite(20,120);  //窗口位置
  
  //clk.setTextDatum(CC_DATUM);
  //clk.setTextColor(TFT_WHITE, 0x0000); 
  //clk.pushSprite(130,180);
  
  clk.deleteSprite();
  loadNum += 1;
  delay(delayTime);
}

//湿度图标显示函数
void humidityWinAt(int x, int y)
{
  clk.setColorDepth(8);
  
  huminum = huminum/2;
  clk.createSprite(52, 6);  //创建窗口
  clk.fillSprite(0x0000);    //填充率
  clk.drawRoundRect(0,0,52,6,3,0xFFFF);  //空心圆角矩形  起始位x,y,长度，宽度，圆弧半径，颜色
  clk.fillRoundRect(1,1,huminum,4,2,humicol);   //实心圆角矩形
  clk.pushSprite(x,y);  //窗口位置
  clk.deleteSprite();
}

void humidityWin()
{
  humidityWinAt(45,222);
}

//温度图标显示函数
void tempWinAt(int x, int y)
{
  clk.setColorDepth(8);
  
  clk.createSprite(52, 6);  //创建窗口
  clk.fillSprite(0x0000);    //填充率
  clk.drawRoundRect(0,0,52,6,3,0xFFFF);  //空心圆角矩形  起始位x,y,长度，宽度，圆弧半径，颜色
  clk.fillRoundRect(1,1,tempnum,4,2,tempcol);   //实心圆角矩形
  clk.pushSprite(x,y);  //窗口位置
  clk.deleteSprite();
}

void tempWin()
{
  tempWinAt(45,192);
}
