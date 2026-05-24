#include <Arduino.h>
#include <WiFi.h>
#include <time.h>
#include <FastLED.h>

// ================= 配置区 =================
const char *ssid = "wifi名称";
const char *password = "wifi密码";

const char *ntpServer = "ntp.aliyun.com";
const long gmtOffset_sec = 8 * 3600;
const int daylightOffset_sec = 0;

#define LED_PIN 13			// WS2812B 数据线连接到 GPIO 13
#define NUM_LEDS 72		// 72个灯珠，分12个小时，每6个灯珠一个刻度
#define BRIGHTNESS 255 // 亮度范围 0-255
#define LED_TYPE WS2812B
#define COLOR_ORDER GRB

#define BUTTON_PIN 0 // 按键

CRGB leds[NUM_LEDS];

// ================= 状态变量 =================
int currentMode = 0;	 // 当前显示的模式 (0=钟表, 1=彩虹, 2=炫彩流星)
const int MAX_MODES = 3; // 总模式数量

uint8_t gHue = 0;			 // 动态色彩的基准色相 (用于动画颜色渐变)
uint32_t lastButtonTime = 0; // 用于按键防抖

// ================= 模式 0：钟表模式 =================
void drawClock()
{
	struct tm timeinfo;
	if (!getLocalTime(&timeinfo, 10))
	{
		return;
	}

	int h = timeinfo.tm_hour;
	int m = timeinfo.tm_min;
	int s = timeinfo.tm_sec;

	FastLED.clear();

	// ================= 绘制【表盘刻度】 =================
	// FastLED 提供的函数 beatsin8(BPM, 亮度最低值, 亮度最高值)
	// 它会根据时间自动生成一个非常顺滑的正弦波数值，这里用来控制呼吸的亮度
	uint8_t mainBreath = beatsin8(15, 15, 70); // 15次/分钟的速度，亮度在 15~70 波动
	uint8_t subBreath = beatsin8(15, 2, 20);   // 次刻度的呼吸波动范围较小 (2~20)

	// 72个灯珠，分12个小时，每6个灯珠一个刻度
	for (int i = 0; i < NUM_LEDS; i += 6)
	{
		// 72/4=18，所以 i 能被 18 整除的就是 12、3、6、9 点钟方向
		if (i % 18 == 0)
		{
			// 主刻度（12、3、6、9 点钟）：饱和度255，亮度较高
			leds[i] = CHSV(gHue + (i * 2), 255, mainBreath);
		}
		else
		{
			// 其他整点刻度：饱和度150(略微偏白，看起来像高级仪表盘)，亮度低
			leds[i] = CHSV(gHue + (i * 2), 150, subBreath);
		}
	}

	// ================= 计算指针 =================
	int secPos = (s * 72) / 60;
	int minPos = (m * 72) / 60;
	int hourPos = ((h % 12) * 6) + (m / 10);

	// ================= 绘制指针 =================
	leds[(hourPos - 1 + NUM_LEDS) % NUM_LEDS] += CRGB(255, 0, 0);
	leds[hourPos] += CRGB(255, 0, 0);
	leds[(hourPos + 1) % NUM_LEDS] += CRGB(255, 0, 0);

	leds[minPos] += CRGB(0, 255, 0);
	leds[secPos] += CRGB(0, 0, 255);
}

// ================= 模式 1：彩虹模式 =================
void drawRainbow()
{
	fill_rainbow(leds, NUM_LEDS, gHue, 255 / NUM_LEDS);
}

// ================= 模式 2：炫彩流星 =================
void drawFlow()
{
	// 制造拖尾效果：让所有灯珠变暗，而不是完全清空 (参数20表示变暗程度)
	fadeToBlackBy(leds, NUM_LEDS, 20);

	// 利用 beat8 制造一个随时间循环的变量 (0-255)。参数 30 和 45 是转圈速度
	uint8_t pos1 = (beat8(30) * NUM_LEDS) / 256;
	uint8_t pos2 = (beat8(45) * NUM_LEDS) / 256;

	// 在算出的位置画上高亮度的动态颜色
	leds[pos1] += CHSV(gHue, 255, 255);
	// 第二个流星跟第一个流星相距半圈，且颜色为对比色 (gHue+128)
	leds[(pos2 + NUM_LEDS / 2) % NUM_LEDS] += CHSV(gHue + 128, 255, 255);
}

// ================= 按键检测 =================
void handleButton()
{
	// GPIO 0 按下时是 LOW
	if (digitalRead(BUTTON_PIN) == LOW)
	{
		// 300ms 的防抖时间，防止按一次被识别为很多次
		if (millis() - lastButtonTime > 300)
		{
			currentMode = (currentMode + 1) % MAX_MODES;
			FastLED.clear(); // 切换模式时清空画布
			Serial.printf("Switching to Mode: %d\n", currentMode);
			lastButtonTime = millis();
		}
	}
}

void setup()
{
	Serial.begin(115200);

	pinMode(BUTTON_PIN, INPUT_PULLUP);

	FastLED.addLeds<LED_TYPE, LED_PIN, COLOR_ORDER>(leds, NUM_LEDS).setCorrection(TypicalLEDStrip);
	FastLED.setBrightness(BRIGHTNESS);

	Serial.printf("\nConnecting to %s ", ssid);
	WiFi.begin(ssid, password);

	int loadingPos = 0;
	while (WiFi.status() != WL_CONNECTED)
	{
		delay(100);
		FastLED.clear();
		leds[loadingPos] = CRGB::White;
		FastLED.show();
		loadingPos = (loadingPos + 1) % NUM_LEDS;
	}
	Serial.println("\nWiFi CONNECTED!");

	configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
}

void loop()
{
	handleButton();

	EVERY_N_MILLISECONDS(20)
	{
		gHue++;
	}

	EVERY_N_MILLISECONDS(33)
	{
		switch (currentMode)
		{
		case 0:
			drawClock();
			break;
		case 1:
			drawRainbow();
			break;
		case 2:
			drawFlow();
			break;
		}
		FastLED.show();
	}
}