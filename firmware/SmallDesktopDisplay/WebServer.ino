#if WebSever_EN
//web网站相关函数
//web设置页面
void handleconfig()
{
  String msg;
  int web_cc,web_setro,web_lcdbl,web_upt,web_dhten;
  int currentCity = cityCode.toInt();
  if(currentCity < 101000000 || currentCity > 102000000)
  {
    currentCity = 101010100;
    cityCode = "101010100";
    int defaultCity = currentCity;
    saveCityCodetoEEP(&defaultCity);
  }

  if (server.hasArg("web_ccode") || server.hasArg("web_bl") || \
      server.hasArg("web_upwe_t") || server.hasArg("web_DHT11_en") || \
      server.hasArg("web_set_rotation") || server.hasArg("web_stock")) 
  {
    web_cc    = server.arg("web_ccode").toInt();
    web_setro = server.arg("web_set_rotation").toInt();
    web_lcdbl = server.arg("web_bl").toInt();
    web_upt   = server.arg("web_upwe_t").toInt();
    web_dhten = server.arg("web_DHT11_en").toInt();
    String web_stock = server.arg("web_stock");
    Serial.println("");
    if(web_cc>=101000000 && web_cc<=102000000) 
    {
      saveCityCodetoEEP(&web_cc);
      readCityCodefromEEP(&web_cc);
      cityCode = String(web_cc);
      Serial.print("城市代码:");
      Serial.println(web_cc);
    }
    if(web_lcdbl>=LCD_BL_MIN && web_lcdbl<=LCD_BL_MAX)
    {
      EEPROM.write(BL_addr, web_lcdbl);//亮度地址写入亮度值
      EEPROM.commit();//保存更改的数据
      delay(5);
      LCD_BL_PWM = EEPROM.read(BL_addr); 
      delay(5);
      Serial.printf("亮度调整为：");
      setBacklight(LCD_BL_PWM, false);
      Serial.println(LCD_BL_PWM);
      Serial.println("");
    }
    if(web_upt > 0 && web_upt <= 60)
    {
      EEPROM.write(UpWeT_addr, web_upt);//亮度地址写入亮度值
      EEPROM.commit();//保存更改的数据
      delay(5);
      updateweater_time = web_upt;
      Serial.print("天气更新时间（分钟）:");
      Serial.println(web_upt);
    }

    EEPROM.write(DHT_addr, web_dhten);
    EEPROM.commit();//保存更改的数据
    delay(5);
    if(web_dhten != DHT_img_flag)
    {
      DHT_img_flag = web_dhten;
      tft.fillScreen(0x0000);
      Page_render(true);
      UpdateWeater_en = 1;
    }
    Serial.print("DHT Sensor Enable： ");
    Serial.println(DHT_img_flag);

    
    EEPROM.write(Ro_addr, web_setro);
    EEPROM.commit();//保存更改的数据
    delay(5);
    if(web_setro != LCD_Rotation)
    {
      LCD_Rotation = web_setro;
      tft.setRotation(LCD_Rotation);
      tft.fillScreen(0x0000);
      Page_render(true);
      UpdateWeater_en = 1;
    }
    Serial.print("LCD Rotation:");
    Serial.println(LCD_Rotation);
    web_stock.trim();
    if(web_stock.length() > 0 && web_stock.length() < 16)
    {
      stockCode = web_stock;
      saveStockConfig();
      Serial.print("Stock Code:");
      Serial.println(stockCode);
    }
    msg = "<div class='ok'>Saved</div>";
  }

  //网页界面代码段
  String content = "<!doctype html><html><head><meta charset='utf-8'><meta name='viewport' content='width=device-width,initial-scale=1'>";
        content += "<style>body{margin:0;background:#101418;color:#f6f7f8;font-family:-apple-system,BlinkMacSystemFont,Segoe UI,sans-serif}";
        content += ".wrap{max-width:560px;margin:auto;padding:18px}.title{font-size:24px;font-weight:700;margin:8px 0 16px}";
        content += ".card{background:#1b2229;border:1px solid #303841;border-radius:8px;padding:14px;margin:12px 0}";
        content += "label{display:block;color:#aeb6bf;font-size:13px;margin:12px 0 6px}input{box-sizing:border-box;width:100%;font-size:16px;padding:11px;border-radius:6px;border:1px solid #46515c;background:#0f1419;color:#fff}";
        content += ".row{display:grid;grid-template-columns:1fr 1fr;gap:10px}.radio label{display:inline-block;margin-right:12px;color:#f6f7f8}.radio input{width:auto}";
        content += "button,.btn{display:inline-block;text-align:center;text-decoration:none;color:#fff;background:#1e88e5;border:0;border-radius:6px;padding:11px 14px;font-size:16px;margin:6px 6px 0 0}.btn.alt{background:#37414b}.ok{background:#173d2b;color:#7ee2a8;border-radius:6px;padding:10px;margin-bottom:10px}@media(max-width:420px){.row{grid-template-columns:1fr}}</style>";
        content += "</head><body><main class='wrap'><div class='title'>TinyTV 设置</div>";
        content += msg;
        content += "<form action='/' method='POST'><section class='card'><h3>显示</h3>";
        content += "<div class='row'><div><label>亮度 20-100</label><input type='number' name='web_bl' min='20' max='100' value='" + String(LCD_BL_PWM) + "'></div>";
        content += "<div><label>刷新分钟</label><input type='number' name='web_upwe_t' min='1' max='60' value='" + String(updateweater_time) + "'></div></div>";
        content += "<label>屏幕方向</label><div class='radio'>";
        for(int i=0;i<4;i++){ content += "<label><input type='radio' name='web_set_rotation' value='" + String(i) + "'"; if(i==LCD_Rotation) content += " checked"; content += "> " + String(i) + "</label>"; }
        content += "</div></section><section class='card'><h3>天气与股票</h3>";
        content += "<label>城市代码</label><input type='number' name='web_ccode' value='" + cityCode + "'>";
        content += "<label>股票代码</label><input type='text' name='web_stock' value='" + stockCode + "'>";
        #if DHT_EN
        content += "<label>DHT11 传感器</label><div class='radio'>";
        content += "<label><input type='radio' name='web_DHT11_en' value='0'";
        if(DHT_img_flag == 0) content += " checked";
        content += "> 关闭</label><label><input type='radio' name='web_DHT11_en' value='1'";
        if(DHT_img_flag != 0) content += " checked";
        content += "> 开启</label></div>";
        #endif
        content += "</section><button type='submit'>保存设置</button></form>";
        content += "<section class='card'><h3>页面控制</h3><a class='btn' href='/SetPage?Click'>单击/切换</a><a class='btn' href='/SetPage?DoubleClick'>双击/确认</a><a class='btn alt' href='/SetPage?LongClick'>长按/返回</a></section>";
        content += "</main></body></html>";
  server.send(200, "text/html; charset=utf-8", content);
}

//no need authentication
void handleNotFound() {
  String message = "File Not Found\n\n";
  message += "URI: ";
  message += server.uri();
  message += "\nMethod: ";
  message += (server.method() == HTTP_GET) ? "GET" : "POST";
  message += "\nArguments: ";
  message += server.args();
  message += "\n";
  for (uint8_t i = 0; i < server.args(); i++) {
    message += " " + server.argName(i) + ": " + server.arg(i) + "\n";
  }
  server.send(404, "text/plain", message);
}

void handleSetPage()
{
  if(server.hasArg("Click"))
  {
    Page_next();
    server.send(200, "text/plain", "Click");
    return;
  }
  if(server.hasArg("DoubleClick"))
  {
    Page_confirm();
    server.send(200, "text/plain", "DoubleClick");
    return;
  }
  if(server.hasArg("LongClick"))
  {
    Page_home();
    server.send(200, "text/plain", "LongClick");
    return;
  }
  server.send(400, "text/plain", "Use /SetPage?Click, /SetPage?DoubleClick or /SetPage?LongClick");
}

//Web服务初始化
void Web_Sever_Init()
{
  uint32_t counttime = 0;//记录创建mDNS的时间
  Serial.println("mDNS responder building...");
  counttime = millis();
  while (!MDNS.begin("SD3"))
  {
    if(millis() - counttime > 30000) ESP.restart();//判断超过30秒钟就重启设备
  }

  Serial.println("mDNS responder started");
  //输出连接wifi后的IP地址
  // Serial.print("本地IP： ");
  // Serial.println(WiFi.localIP());

  server.on("/", handleconfig);
  server.on("/SetPage", handleSetPage);
  server.onNotFound(handleNotFound);

  //开启TCP服务
  server.begin();
  Serial.println("HTTP服务器已开启");

  Serial.println("连接: http://sd3.local");
  Serial.print("本地IP： ");
  Serial.println(WiFi.localIP());
  //将服务器添加到mDNS
  MDNS.addService("http", "tcp", 80);
}
//Web网页设置函数
void Web_Sever()
{
  MDNS.update();
  server.handleClient();
}
//web服务打开后LCD显示登陆网址及IP
void Web_sever_Win()
{
  IPAddress IP_adr = WiFi.localIP();
  // strcpy(IP_adr,WiFi.localIP().toString());
  clk.setColorDepth(8);
  
  clk.createSprite(200, 70);//创建窗口
  clk.fillSprite(0x0000);   //填充率

  // clk.drawRoundRect(0,0,200,100,5,0xFFFF);       //空心圆角矩形
  clk.setTextDatum(CC_DATUM);   //设置文本数据
  clk.setTextColor(TFT_GREEN, 0x0000); 
  clk.drawString("Connect to Config:",70,10,2);
  // clk.drawString("IP:",45,60,2);
  clk.setTextColor(TFT_WHITE, 0x0000); 
  clk.drawString("http://sd3.local",100,40,4);
  // clk.drawString(&IP_adr,125,70,2);
  clk.pushSprite(20,40);  //窗口位置
    
  clk.deleteSprite();
}
#endif
