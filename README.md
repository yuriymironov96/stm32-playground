# stm32-playground

A bunch of learning projects using STM32F411 Black Pill developemt board.

- [Pinout](https://deepbluembedded.com/wp-content/uploads/2024/03/STM32F411CE-Black-Pill-Board-Pinout-Diagram.png);

## I2C communicaion

- Cloned [existing project](https://github.com/Vel11leV/STM32F401CC_I2C_display_ssd1306) with STM32, I2C and drivers for SSD1306 display;
- Wired up DS1307 RTC into the same I2C bus as SSD1306;
- Used STM32 to continuously read RTC values and write them onto OLED;
- Wrote a tiny driver `ds1307.c` for the RTC;


## Demo

![demo.gif](demo.gif)

## Fritzing

![circuit.png](circuit.png)