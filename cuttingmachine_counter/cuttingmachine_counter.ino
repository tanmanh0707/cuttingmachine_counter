#include <EEPROM.h>
#include <Adafruit_GFX.h>
#include <Adafruit_TFTLCD.h>

// EEPROM address
#define EEPROM_IDENTIFIER_ADDR      0
#define EEPROM_IDENTIFIER_SIZE      1   //bytes
#define EEPROM_WEEK_COUNT_ADDR      (EEPROM_IDENTIFIER_ADDR + EEPROM_IDENTIFIER_SIZE)
#define EEPROM_WEEK_COUNT_SIZE      4   //bytes
#define EEPROM_MONTH_COUNT_ADDR     (EEPROM_WEEK_COUNT_ADDR + EEPROM_WEEK_COUNT_SIZE)
#define EEPROM_MONTH_COUNT_SIZE     4   //bytes
#define EEPROM_NUMBER_OF_YEAR_ADDR  (EEPROM_MONTH_COUNT_ADDR + EEPROM_MONTH_COUNT_SIZE)
#define EEPROM_NUMBER_OF_YEAR_SIZE  1   //bytes
#define EERPOM_YEAR_COUNT_BASE_ADDR (EEPROM_NUMBER_OF_YEAR_ADDR + EEPROM_NUMBER_OF_YEAR_SIZE)
#define EEPROM_YEAR_COUNT_SIZE      4   //bytes

// EEPROM Identifier
#define EEPROM_IDENTIFIER           0x55

// Define the pins for the TFT screen
#define LCD_CS    A3
#define LCD_CD    A2
#define LCD_WR    A1
#define LCD_RD    A0
#define LCD_RESET A4
#define LCD_ID    0x9341

// Define the pin for counting
#define CUTTING_COUNT_PIN 8

// Define the colors
#define BLACK 0x0000
#define WHITE 0xFFFF

#define CUTTING_SAMPLE_COUNT      5
#define NUMBER_OF_YEAR_MAX        50

typedef struct {
  byte year;
  uint32_t count;
} YearCount_st;

// Create an instance of the TFT library
Adafruit_TFTLCD tft(LCD_CS, LCD_CD, LCD_WR, LCD_RD, LCD_RESET);
YearCount_st g_yearCountList[NUMBER_OF_YEAR_MAX];

// Define variables
int count = 0;
int totalCount = 0;
unsigned long g_lcd_refresh_time = 0;
int numCounts = 0;
int g_thisWeekCount = 0;
int g_thisMonthCount = 0;
byte g_numberOfYear = 0;
unsigned long count_timestamp[CUTTING_SAMPLE_COUNT];

void LCD_DisplayCountPage();
void LCD_DisplayCountWeekPage();
void LCD_DisplayCountYearPage();

void EEPROM_Write32(int addr, uint32_t value)
{
  EEPROM.update(addr++, (byte)(value >> 24));
  EEPROM.update(addr++, (byte)(value >> 16));
  EEPROM.update(addr++, (byte)(value >> 8));
  EEPROM.update(addr,   (byte)(value));
}

uint32_t EEPROM_Read32(int addr)
{
  uint32_t result = 0;
  uint32_t byte0 = (uint32_t)EEPROM.read(addr++);
  uint32_t byte1 = (uint32_t)EEPROM.read(addr++);
  uint32_t byte2 = (uint32_t)EEPROM.read(addr++);
  uint32_t byte3 = (uint32_t)EEPROM.read(addr);

  result = (byte0 << 24) | (byte1 << 16) | (byte2 << 8) | byte3;

  return result;
}

void InitEEPROM()
{
  int base_addr = EERPOM_YEAR_COUNT_BASE_ADDR;
  byte eeprom_id = EEPROM.read(EEPROM_IDENTIFIER_ADDR);

  Serial.println("EEPROM init...");

  if (EEPROM_IDENTIFIER == eeprom_id ) {
    g_thisWeekCount = EEPROM_Read32(EEPROM_WEEK_COUNT_ADDR);
    g_thisMonthCount = EEPROM_Read32(EEPROM_MONTH_COUNT_ADDR);
    g_numberOfYear = EEPROM.read(EEPROM_NUMBER_OF_YEAR_ADDR);

    Serial.print("EEPROM this week count : "); Serial.println(g_thisWeekCount, DEC);
    Serial.print("EEPROM this month count: "); Serial.println(g_thisMonthCount, DEC);
    Serial.print("EEPROM number of year  : "); Serial.println(g_numberOfYear, DEC);

    if (g_numberOfYear > 0)
    {
      if (g_numberOfYear > NUMBER_OF_YEAR_MAX) {
        g_numberOfYear = NUMBER_OF_YEAR_MAX;
      }

      for (byte i = 0; i < g_numberOfYear; i++)
      {
        g_yearCountList[i].year = EEPROM.read(base_addr++);
        g_yearCountList[i].count = EEPROM_Read32(base_addr);
        base_addr += sizeof(YearCount_st);
        Serial.print(g_yearCountList[i].year); Serial.print(" count: "); Serial.println(g_yearCountList[i].count, DEC);
      }
    }
  }
  else {
    Serial.println("EEPROM default settings...");
    EEPROM.write(EEPROM_IDENTIFIER_ADDR, EEPROM_IDENTIFIER);
    EEPROM_Write32(EEPROM_WEEK_COUNT_ADDR, 0);
    EEPROM_Write32(EEPROM_MONTH_COUNT_ADDR, 0);
    EEPROM.update(EEPROM_NUMBER_OF_YEAR_ADDR, 0);
    memset(g_yearCountList, 0, NUMBER_OF_YEAR_MAX * sizeof(YearCount_st));
  }

  Serial.println("EEPROM init done!");
}

void setup()
{
  Serial.begin(115200);
  delay(1000);

  // Initialize EEPROM
  InitEEPROM();
  
  // Initialize the TFT screen
  Serial.println("TFT init...");
  tft.reset();
  tft.begin(LCD_ID);
  tft.fillScreen(BLACK);
  tft.setRotation(1);
  tft.setTextColor(WHITE);
  tft.setTextSize(3);
  Serial.println("TFT init done!");

  // Set up the count and totalCount variables
  count = 0;
  totalCount = 0;

  // Set up the counts array
  for (int i = 0; i < CUTTING_SAMPLE_COUNT; i++) {
    count_timestamp[i] = 0;
  }

  // Set up the count pin
  pinMode(CUTTING_COUNT_PIN, INPUT_PULLUP);

  // Set up the start time
  g_lcd_refresh_time = millis();

  Serial.println("STARTED!!!");
}

void loop() {
  // Get the current time
  unsigned long currentTime = millis();

  // Increment the count if the count pin goes high
  if (digitalRead(CUTTING_COUNT_PIN) == HIGH) {
    Serial.print("Cutting detected at "); Serial.println(currentTime, DEC);
    count++;
    totalCount++;
    g_thisWeekCount++;
    g_thisMonthCount++;
    Serial.print("Total count: "); Serial.print(totalCount, DEC);
    EEPROM_Write32(EEPROM_WEEK_COUNT_ADDR, g_thisWeekCount);
    EEPROM_Write32(EEPROM_MONTH_COUNT_ADDR, g_thisMonthCount);

    // Shift the counts array
    for (int i = CUTTING_SAMPLE_COUNT - 1; i > 0; i--) {
      count_timestamp[i] = count_timestamp[i - 1];
    }
    count_timestamp[0] = currentTime;

    for (int i = CUTTING_SAMPLE_COUNT - 1; i > 0; i--) {
      Serial.print("("); Serial.print(i); Serial.print(") "); Serial.println(count_timestamp[i], DEC);
    }

    if (numCounts < CUTTING_SAMPLE_COUNT) {
      numCounts++;
    }

    delay(100);  //Debounce
  }

  // Update the display if one second has passed
  if (currentTime - g_lcd_refresh_time >= 1000) {
    g_lcd_refresh_time = currentTime;

    LCD_DisplayCountPage();
  }
}

void LCD_DisplayCountPage()
{
  float avgCount = 0.0;
  unsigned long time_diff = 0;

  // Display the counts on the TFT screen
  tft.fillScreen(BLACK);

  if (numCounts > 0) {
    time_diff = count_timestamp[0] - count_timestamp[CUTTING_SAMPLE_COUNT - 1];
    time_diff /= 1000.0;  //Convert to seconds
    avgCount = 3600.0 * (float)numCounts / (float)time_diff;  //Average based on the latest 5 cuttings
  }
  
  tft.setCursor(0, 0);
  tft.print("Count: ");
  tft.print(count);
  tft.setCursor(0, 60);
  tft.print("Avg/Hour: ");
  tft.print(avgCount, 1);
  tft.setCursor(0, 120);
  tft.print("Total: ");
  tft.print(totalCount);
}

void LCD_DisplayCountWeekPage()
{
  tft.fillScreen(BLACK);
  tft.setCursor(0, 0);
  tft.print("This week:  ");
  tft.print(g_thisWeekCount);
  tft.setCursor(0, 60);
  tft.print("This month: ");
  tft.print(g_thisMonthCount);
  tft.setCursor(0, 120);
  tft.print("This year:  ");
}

void LCD_DisplayCountYearPage()
{
  tft.fillScreen(BLACK);
  tft.setCursor(0, 0);
  tft.print("2023:  ");
  tft.setCursor(0, 60);
  tft.print("2024: ");
  tft.setCursor(0, 120);
  tft.print("2025:  ");
}
