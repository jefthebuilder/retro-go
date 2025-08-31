// Target definition
#define RG_TARGET_NAME             "JAF1"

// Storage
#define RG_STORAGE_ROOT             "/sd"
#define RG_STORAGE_SDSPI_HOST       SPI3_HOST
#define RG_STORAGE_SDSPI_SPEED      SDMMC_FREQ_DEFAULT
// #define RG_STORAGE_SDMMC_HOST       SDMMC_HOST_SLOT_1
// #define RG_STORAGE_SDMMC_SPEED      SDMMC_FREQ_DEFAULT
// #define RG_STORAGE_FLASH_PARTITION  "vfs"
#define RG_USB_MSC                  0
// GPIO Extender

// Audio
#define RG_AUDIO_USE_INT_DAC        0   // 0 = Disable, 1 = GPIO25, 2 = GPIO26, 3 = Both
#define RG_AUDIO_USE_EXT_DAC        1   // 0 = Disable, 1 = Enable

// Video
#define RG_SCREEN_DRIVER            0   // 0 = ILI9341/ST7789
#define RG_SCREEN_HOST              SPI2_HOST
#define RG_SCREEN_SPEED             SPI_MASTER_FREQ_40M
#define RG_SCREEN_BACKLIGHT         1
#define RG_SCREEN_WIDTH             320
#define RG_SCREEN_HEIGHT            240
#define RG_SCREEN_ROTATE            0
#define RG_SCREEN_VISIBLE_AREA      {0, 0, 0, 0}
#define RG_SCREEN_SAFE_AREA         {0, 0, 0, 0}
#define RG_SCREEN_INIT()                                                                                         \
    ILI9341_CMD(0xCF, 0x00, 0x83, 0X30);                                                                 \
    ILI9341_CMD(0xED, 0x64, 0x03, 0X12, 0X81);                                                           \
    ILI9341_CMD(0xE8, 0x85, 0x01, 0x79);                                                                 \
    ILI9341_CMD(0xCB, 0x39, 0x2C, 0x00, 0x34, 0x02);                                                     \
    ILI9341_CMD(0xF7, 0x20);                                                                             \
    ILI9341_CMD(0xEA, 0x00, 0x00);                                                                       \
    ILI9341_CMD(0xC0, 0x26);          /* Power control */                                                \
    ILI9341_CMD(0xC1, 0x11);          /* Power control */                                                \
    ILI9341_CMD(0xC5, 0x35, 0x3E);    /* VCM control */                                                  \
    ILI9341_CMD(0x36, 0xC0); /* 270° rotation */                                                         \
    ILI9341_CMD(0x3A, 0x55);          /* Pixel Format Set RGB565 */                                      \
    ILI9341_CMD(0xB1, 0x00, 0x1B);    /* Frame Rate Control (1B=70, 1F=61, 10=119) */                    \
    ILI9341_CMD(0xB6, 0x0A, 0xA2);    /* Display Function Control */                                     \
    ILI9341_CMD(0xF6, 0x01, 0x30);                                                                       \
    ILI9341_CMD(0xF2, 0x00);                                                                             \
    ILI9341_CMD(0x26, 0x01);                                                                             \
    ILI9341_CMD(0xE0, 0x1F, 0x1A, 0x18, 0x0A, 0x0F, 0x06, 0x45, 0X87, 0x32, 0x0A, 0x07, 0x02, 0x07, 0x05, 0x00); \
    ILI9341_CMD(0xE1, 0x00, 0x25, 0x27, 0x05, 0x10, 0x09, 0x3A, 0x78, 0x4D, 0x05, 0x18, 0x0D, 0x38, 0x3A, 0x1F); \
    ILI9341_CMD(0x2C, 0x00);                                                                             \
    ILI9341_CMD(0xB7, 0x07);                                                                             \
    ILI9341_CMD(0xB6, 0x0A, 0x82, 0x27, 0x00);                                                           \
    ILI9341_CMD(0x11, 0x80);                                                                             \
    ILI9341_CMD(0x29, 0x80);

// Input
// Refer to rg_input.h to see all available RG_KEY_* and RG_GAMEPAD_*_MAP types

#define RG_GAMEPAD_GPIO_MAP {\
    {RG_KEY_SELECT, GPIO_NUM_10, GPIO_PULLDOWN_ONLY, 0},\
    {RG_KEY_START,  GPIO_NUM_0, GPIO_PULLDOWN_ONLY, 0},\
    {RG_KEY_MENU,   GPIO_NUM_16, GPIO_PULLDOWN_ONLY, 0},\
    {RG_KEY_A,      GPIO_NUM_12, GPIO_PULLDOWN_ONLY, 0},\
    {RG_KEY_B,      GPIO_NUM_11,  GPIO_PULLDOWN_ONLY, 0},\
    {RG_KEY_X,      GPIO_NUM_47,  GPIO_PULLDOWN_ONLY, 0},\
    {RG_KEY_Y,      GPIO_NUM_48,  GPIO_PULLDOWN_ONLY, 0},\
    {RG_KEY_UP,      GPIO_NUM_6,  GPIO_PULLDOWN_ONLY,0},\
    {RG_KEY_DOWN,      GPIO_NUM_9, GPIO_PULLDOWN_ONLY, 0},\
    {RG_KEY_RIGHT,      GPIO_NUM_8,GPIO_PULLDOWN_ONLY,  0},\
    {RG_KEY_LEFT,      GPIO_NUM_7, GPIO_PULLDOWN_ONLY,  0},\
}
#define RG_GAMEPAD_VIRT_MAP {\
    {RG_KEY_OPTION,RG_KEY_MENU+RG_KEY_SELECT}\
}
// Battery
#define RG_BATTERY_DRIVER           1
#define RG_BATTERY_ADC_UNIT         ADC_UNIT_2
#define RG_BATTERY_ADC_CHANNEL      ADC_CHANNEL_3
#define RG_BATTERY_CALC_VOLTAGE(raw) ((raw) * 1.51f * 0.001f)
#define RG_BATTERY_CALC_PERCENT(raw) ((((raw) * 1.51f * 0.001f) - 3.5f) / (4.2f - 3.5f) * 100.f)


// Status LED
//#define RG_GPIO_LED                 GPIO_NUM_14

// I2C BUS

// SPI Display
#define RG_GPIO_LCD_MISO            GPIO_NUM_13
#define RG_GPIO_LCD_MOSI            GPIO_NUM_18
#define RG_GPIO_LCD_CLK             GPIO_NUM_17
#define RG_GPIO_LCD_CS              GPIO_NUM_5
#define RG_GPIO_LCD_DC              GPIO_NUM_2
#define RG_GPIO_LCD_BCKL            GPIO_NUM_15
#define RG_GPIO_LCD_RST             GPIO_NUM_4

// SPI SD Card
#define RG_GPIO_SDSPI_MISO          GPIO_NUM_40
#define RG_GPIO_SDSPI_MOSI          GPIO_NUM_39
#define RG_GPIO_SDSPI_CLK           GPIO_NUM_41
#define RG_GPIO_SDSPI_CS            GPIO_NUM_38

// External I2S DAC
#define RG_GPIO_SND_I2S_BCK         21
#define RG_GPIO_SND_I2S_WS          3
#define RG_GPIO_SND_I2S_DATA        46
#define RG_GPIO_SND_I2S_SCK         45
//#define RG_GPIO_SND_AMP_ENABLE_INVERT // Uncomment if the mute = HIGH
