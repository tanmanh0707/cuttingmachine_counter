#include <EEPROM.h>
#include <MCP7940.h>
#include <Adafruit_GFX.h>
#include <Adafruit_TFTLCD.h>

// EEPROM address
#define EEPROM_IDENTIFIER_ADDR      0
#define EEPROM_IDENTIFIER_SIZE      1   //bytes
#define EEPROM_CRT_YEAR_ADDR        (EEPROM_IDENTIFIER_ADDR + EEPROM_IDENTIFIER_SIZE)
#define EEPROM_CRT_YEAR_SIZE        4
#define EEPROM_CRT_MONTH_ADDR       (EEPROM_CRT_YEAR_ADDR + EEPROM_CRT_YEAR_SIZE)
#define EEPROM_CRT_MONTH_SIZE       1
#define EEPROM_CRT_WEEK_EPOCH_ADDR  (EEPROM_CRT_MONTH_ADDR + EEPROM_CRT_MONTH_SIZE)
#define EEPROM_CRT_WEEK_EPOCH_SIZE  4
#define EEPROM_WEEK_COUNT_ADDR      (EEPROM_CRT_WEEK_EPOCH_ADDR + EEPROM_CRT_WEEK_EPOCH_SIZE)
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

typedef struct {
  uint32_t crt_week_epoch;
  uint32_t week_count;
  uint32_t month_count;
  uint16_t crt_year;
  byte number_of_year;
  byte crt_month;
  YearCount_st year_counts[NUMBER_OF_YEAR_MAX];
} EepromData_st;

const uint32_t SECONDS_IN_ONE_WEEK = (7/*days*/ * 24/*hours*/ * 60/*minutes*/ * 60/*seconds*/);

// Create an instance of the TFT library
Adafruit_TFTLCD tft(LCD_CS, LCD_CD, LCD_WR, LCD_RD, LCD_RESET);
MCP7940_Class MCP7940;
EepromData_st g_database;

// Define variables
int count = 0;
int totalCount = 0;
unsigned long g_lcd_refresh_time = 0;
int numCounts = 0;
unsigned long count_timestamp[CUTTING_SAMPLE_COUNT];
unsigned long g_checking_counter_time = 0;
bool g_rtcReady = false;

// For LCD
void LCD_DisplayCountPage();
void LCD_DisplayCountWeekPage();
void LCD_DisplayCountYearPage();
// For EEPROM
void EEPROM_Write8(int addr, byte value);
byte EEPROM_Read8(int addr);
void EEPROM_Write32(int addr, uint32_t value);
uint32_t EEPROM_Read32(int addr);
// For RTC
DateTime RTC_Now();

void InitEEPROM()
{
  int base_addr = EERPOM_YEAR_COUNT_BASE_ADDR;
  byte eeprom_id = EEPROM_Read8(EEPROM_IDENTIFIER_ADDR);

  Serial.println(F("EEPROM init..."));

  if (EEPROM_IDENTIFIER == eeprom_id ) {
    g_database.week_count = EEPROM_Read32(EEPROM_WEEK_COUNT_ADDR);
    g_database.month_count = EEPROM_Read32(EEPROM_MONTH_COUNT_ADDR);
    g_database.number_of_year = EEPROM_Read8(EEPROM_NUMBER_OF_YEAR_ADDR);
    g_database.crt_month = EEPROM_Read8(EEPROM_CRT_MONTH_ADDR);
    g_database.crt_year = EEPROM_Read32(EEPROM_CRT_YEAR_ADDR);
    g_database.crt_week_epoch = EEPROM_Read32(EEPROM_CRT_WEEK_EPOCH_ADDR);

    if (g_database.number_of_year > 0)
    {
      if (g_database.number_of_year > NUMBER_OF_YEAR_MAX) {
        g_database.number_of_year = NUMBER_OF_YEAR_MAX;
      }

      for (byte i = 0; i < g_database.number_of_year; i++)
      {
        g_database.year_counts[i].year = EEPROM_Read8(base_addr) + 2000;
        g_database.year_counts[i].count = EEPROM_Read32(base_addr + 1);
        base_addr += sizeof(YearCount_st);
      }
    }

    // Checking database with RTC
    DateTime now = RTC_Now();
    if (now.year() > g_database.crt_year)
    {
      Serial.print(F("New year detected! Database year: ")); Serial.print(g_database.crt_year); Serial.print(F(" - RTC year: ")); Serial.println(now.year());
      g_database.month_count = 0;
      g_database.crt_month = now.month();
      g_database.crt_year = now.year();
      
      EEPROM_Write32(EEPROM_CRT_YEAR_ADDR, g_database.crt_year);
      EEPROM_Write32(EEPROM_CRT_MONTH_ADDR, g_database.crt_month);
      EEPROM_Write32(EEPROM_MONTH_COUNT_ADDR, 0);
      
      UTIL_UpdateNewYearDatabase(now);
    }
    else if ( now.year() == g_database.crt_year)
    {
      if (now.month() != g_database.crt_month)
      {
        Serial.print(F("New month detected! Database month: ")); Serial.print(g_database.crt_month); Serial.print(F(" - RTC month: ")); Serial.println(now.month());
        g_database.month_count = 0;
        g_database.crt_month = now.month();
        EEPROM_Write32(EEPROM_CRT_MONTH_ADDR, g_database.crt_month);
        EEPROM_Write32(EEPROM_MONTH_COUNT_ADDR, 0);
      }
    }
    else if (now.year() < g_database.crt_year)
    {
      Serial.println(F("Something wrong with year!!!"));
      Serial.print(F("Database year: ")); Serial.print(g_database.crt_year); Serial.print(F(" - RTC year: ")); Serial.println(now.year());
    }

    // Checking week
    const uint32_t UNIXTIME_NOW = now.unixtime();
    if (UNIXTIME_NOW > g_database.crt_week_epoch)
    {
      if (UNIXTIME_NOW - g_database.crt_week_epoch >= SECONDS_IN_ONE_WEEK)
      {
        Serial.print(F("New week detected! Database week: ")); Serial.print(g_database.crt_week_epoch); Serial.print(F(" - RTC Unixtime: ")); Serial.println(UNIXTIME_NOW);
        g_database.crt_week_epoch = UTIL_GetBeginningOfWeekEpoch(now);
        Serial.print(F("Database new week: ")); Serial.print(g_database.crt_week_epoch);
        g_database.week_count = 0;
        EEPROM_Write32(EEPROM_CRT_WEEK_EPOCH_ADDR, g_database.crt_week_epoch);
        EEPROM_Write32(EEPROM_WEEK_COUNT_ADDR, 0);
      }
    }
    else {
      Serial.println(F("Something wrong with week EPOCH!!!"));
      Serial.print(F("Database week: ")); Serial.print(g_database.crt_week_epoch); Serial.print(F(" - RTC week: ")); Serial.println(UNIXTIME_NOW);
    }
  }
  else {
    Serial.print(F("Wrong EEPROM ID: ")); Serial.println(eeprom_id, HEX);
    Serial.println(F("EEPROM default settings..."));
    memset(&g_database, 0, sizeof(EepromData_st));
    EEPROM_Write8(EEPROM_IDENTIFIER_ADDR, EEPROM_IDENTIFIER);
    EEPROM_Write32(EEPROM_WEEK_COUNT_ADDR, 0);
    EEPROM_Write32(EEPROM_MONTH_COUNT_ADDR, 0);
    memset(g_database.year_counts, 0, NUMBER_OF_YEAR_MAX * sizeof(YearCount_st));

    DateTime now = RTC_Now();
    g_database.crt_week_epoch = UTIL_GetBeginningOfWeekEpoch(now);
    g_database.crt_month = now.month();
    g_database.crt_year = now.year();

    // Write init count
    UTIL_UpdateNewYearDatabase(now);
    EEPROM_Write32(EEPROM_CRT_WEEK_EPOCH_ADDR, g_database.crt_week_epoch);
    EEPROM_Write32(EEPROM_CRT_YEAR_ADDR, (uint32_t)g_database.crt_year);
    EEPROM_Write8(EEPROM_CRT_MONTH_ADDR, g_database.crt_month);
  }

  Serial.print(F("EEPROM current year:       ")); Serial.println(g_database.crt_year, DEC);
  Serial.print(F("EEPROM current month:      ")); Serial.println(g_database.crt_month, DEC);
  Serial.print(F("EEPROM current week EPOCH: ")); Serial.println(g_database.crt_week_epoch, DEC);
  Serial.print(F("EEPROM this week count :   ")); Serial.println(g_database.week_count, DEC);
  Serial.print(F("EEPROM this month count:   ")); Serial.println(g_database.month_count, DEC);
  Serial.print(F("EEPROM number of year  :   ")); Serial.println(g_database.number_of_year, DEC);

  if (g_database.number_of_year > 0)
  {
    Serial.println(F("EEPROM year count:"));
    for (byte i = 0; i < g_database.number_of_year; i++)
    {
      Serial.print("  "); Serial.print(g_database.year_counts[i].year + 2000, DEC); Serial.print(": "); Serial.println(g_database.year_counts[i].count, DEC);
    }
  }

  Serial.println(F("EEPROM init done!"));
}

void UTIL_UpdateNewYearDatabase(DateTime &now)
{
  byte i = 0;
  int base_addr = EERPOM_YEAR_COUNT_BASE_ADDR;

  // Get the last year count address
  for (i = 0; i < g_database.number_of_year; i++)
  {
    base_addr += sizeof(YearCount_st);
  }

  // Increase number of year
  g_database.year_counts[g_database.number_of_year].year = now.year() - 2000;
  g_database.year_counts[g_database.number_of_year].count = 0;
  g_database.number_of_year++;
  EEPROM_Write8(EEPROM_NUMBER_OF_YEAR_ADDR, g_database.number_of_year);

  // Update year count
  EEPROM_Write8(base_addr++, now.year() - 2000);
  EEPROM_Write32(base_addr, 0);
}

void initRTC()
{
  byte rtc_init_cnt = 0;
  Serial.println(F("RTC init..."));

  g_rtcReady = true;
  while (!MCP7940.begin()) {  // Initialize RTC communications
    Serial.println(F("Unable to find MCP7940M. Checking again in 1s."));
    rtc_init_cnt++;

    if (rtc_init_cnt >= 3) {
      Serial.println(F("ERROR: MCP7940M not found!!!"));  // Show error text
      g_rtcReady = false;
      break;
    }
    delay(1000);
  }

  if (g_rtcReady == true)
  {
    rtc_init_cnt = 0;
    while (!MCP7940.deviceStatus())  // Turn oscillator on if necessary
    {
      Serial.println(F("Oscillator is off, turning it on."));
      bool deviceStatus = MCP7940.deviceStart();  // Start oscillator and return new state
      if (!deviceStatus) {
        rtc_init_cnt++;
        if (rtc_init_cnt >= 3) {
          Serial.println(F("Oscillator did not start!!!"));
          g_rtcReady = false;
          break;
        }
        Serial.println(F("Oscillator did not start, trying again."));
        delay(1000);
      }
    }
  }

  // Check if RTC is already configured
  if (g_rtcReady == true)
  {
    char buffer[128] = {0};
    DateTime now = MCP7940.now();
    DateTime compiled_time = DateTime(F(__DATE__), F(__TIME__));
    if ( (now.year() < compiled_time.year())
        || ((now.year() == compiled_time.year()) && (now.month() < compiled_time.month())))
    {
      Serial.println(F("RTC is not configured correctly."));
      sprintf(buffer, "RTC time: %02d/%02d/%02d - Compiled time: %02d/%02d/%02d",
              now.year(), now.month(), now.day(),
              compiled_time.year(), compiled_time.month(), compiled_time.day());
      Serial.println(buffer);
      Serial.println(F("Adjusting RTC to the compiled time..."));
      MCP7940.adjust();
      memset(buffer, 0 , 128);
    }

    now = MCP7940.now();
    sprintf(buffer, "Current RTC time: %02d/%02d/%02d  %02:%02:%02",
            now.year(), now.month(), now.day(), now.hour(), now.minute(), now.second());
    Serial.println(buffer);
    Serial.println(F("RTC init done!"));
  }
  else {
    Serial.println(F("RTC init failed!"));
  }
}

void setup()
{
  Serial.begin(115200);
  delay(1000);

  // Initialize RTC
  initRTC();

  // Initialize EEPROM
  InitEEPROM();

  // Initialize the TFT screen
  Serial.println(F("TFT init..."));
  tft.reset();
  tft.begin(LCD_ID);
  tft.fillScreen(BLACK);
  tft.setRotation(1);
  tft.setTextColor(WHITE);
  tft.setTextSize(3);
  Serial.println(F("TFT init done!"));

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

  Serial.println(F("CUTTING COUNTER STARTED!!!"));
}

void loop() {
  // Get the current time
  unsigned long currentTime = millis();

  // Increment the count if the count pin goes high
  if (digitalRead(CUTTING_COUNT_PIN) == HIGH) {
    Serial.print(F("Cutting detected at ")); Serial.println(currentTime, DEC);
    count++;
    totalCount++;
    g_database.week_count++;
    g_database.month_count++;
    Serial.print(F("Total count: ")); Serial.print(totalCount, DEC);
    EEPROM_Write32(EEPROM_WEEK_COUNT_ADDR, g_database.week_count);
    EEPROM_Write32(EEPROM_MONTH_COUNT_ADDR, g_database.month_count);

    // Shift the counts array
    for (int i = CUTTING_SAMPLE_COUNT - 1; i > 0; i--) {
      count_timestamp[i] = count_timestamp[i - 1];
    }
    count_timestamp[0] = currentTime;

    // Print
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

  // Check if new week, new month or new year during run time
  if (currentTime - g_checking_counter_time > 60000)  //Every minute
  {
    DateTime now = RTC_Now();
    const uint32_t unixtime_now = now.unixtime();

    // Check week
    if (unixtime_now > g_database.crt_week_epoch)
    {
      if (unixtime_now - g_database.crt_week_epoch >= SECONDS_IN_ONE_WEEK)
      {
        Serial.println(F("New week started!!!"));
        g_database.week_count = 0;
        g_database.crt_week_epoch = UTIL_GetBeginningOfWeekEpoch(now);
        EEPROM_Write32(EEPROM_WEEK_COUNT_ADDR, 0);
        EEPROM_Write32(EEPROM_CRT_WEEK_EPOCH_ADDR, g_database.crt_week_epoch);
        Serial.print(F("New week: ")); Serial.println(g_database.crt_week_epoch, DEC);
      }
    }

    // Check month
    if (now.month() != g_database.crt_month)
    {
      g_database.month_count = 0;
      g_database.crt_month = now.month();
      EEPROM_Write32(EEPROM_MONTH_COUNT_ADDR, 0);
      EEPROM_Write32(EEPROM_CRT_MONTH_ADDR, g_database.crt_month);
      Serial.print(F("New month detected: ")); Serial.println(g_database.crt_month, DEC);
    }

    // Check year
    if (now.year() > g_database.crt_year)
    {
      g_database.crt_year = now.year();
      EEPROM_Write32(EEPROM_CRT_YEAR_ADDR, 0);
      Serial.print(F("New year detected: ")); Serial.println(g_database.crt_year, DEC);
    }
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
    time_diff /= 1000;  //Convert to seconds
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
  tft.print(g_database.week_count);
  tft.setCursor(0, 60);
  tft.print("This month: ");
  tft.print(g_database.month_count);
  tft.setCursor(0, 120);
  tft.print("This year:  ");
  tft.print(UTIL_GetCurrentYearCount());
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

uint32_t UTIL_GetCurrentYearCount()
{
  uint32_t result = 0;
  byte i = 0, crt_year_pos = NUMBER_OF_YEAR_MAX;
  DateTime now = RTC_Now();

  if (g_database.number_of_year > 0)
  {
    for (i = 0; i < g_database.number_of_year; i++)
    {
      if (g_database.year_counts[i].year == (now.year() - 2000))
      {
        crt_year_pos = i;
        break;
      }
    }

    if (crt_year_pos >= NUMBER_OF_YEAR_MAX)
    {
      Serial.print(F("Not found current year: ")); Serial.println(now.year());
      crt_year_pos = g_database.number_of_year - 1; // Assign the latest year if no year found
    }

    result = g_database.year_counts[crt_year_pos].count;
  }
  else {
    Serial.println(F("ERROR: Wrong number of year!!!"));
  }

  return result;
}

uint32_t UTIL_GetBeginningOfWeekEpoch(DateTime now)
{
  uint32_t beginning_of_week_epoch = now.unixtime();
  
  if (beginning_of_week_epoch > 0) {
    beginning_of_week_epoch -= (uint32_t)now.second();
    beginning_of_week_epoch -= ((uint32_t)now.minute() * 60);
    beginning_of_week_epoch -= ((uint32_t)now.hour() * 60 * 60);
    beginning_of_week_epoch -= ((uint32_t)now.dayOfTheWeek() * 60 * 60 * 24);
  }
  
  return beginning_of_week_epoch;
}

void EEPROM_Write8(int addr, byte value)
{
  EEPROM.update(addr, value);
}

byte EEPROM_Read8(int addr)
{
  byte result = 0;
  result = EEPROM.read(addr);
  return result;
}

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

DateTime RTC_Now()
{
  if (g_rtcReady) {
    return MCP7940.now();
  }
  else {
    return DateTime(0); //Default year 1970
  }
}
