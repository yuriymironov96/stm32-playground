# stm32-playground

A bunch of learning projects using STM32F411 Black Pill developemt board.

- [Pinout](https://deepbluembedded.com/wp-content/uploads/2024/03/STM32F411CE-Black-Pill-Board-Pinout-Diagram.png);

## Lessom 23 - led blink driven by timer interrupts

tim2 hardware clock is used to drive led blinks. With 15999 prescaler value and 99 counter value, it will tick every 100ms.

Since we need to control 3 LEDs, here is a config datatype to encapsulate all the logic for a single LED:

```c
typedef struct {
	GPIO_TypeDef *port;
	uint16_t pin;
	uint32_t counterMs;
	uint32_t toggleThresholdMs;
	volatile bool shouldToggle;
} BlinkingLedConfig;

volatile BlinkingLedConfig ledConfigs[] = {
		{GPIOA, GPIO_PIN_0, 0, 2, false},
		{GPIOA, GPIO_PIN_1, 0, 5, false},
		{GPIOA, GPIO_PIN_2, 0, 10, false},
};

#define LED_COUNT (sizeof(ledConfigs) / sizeof(ledConfigs[0]))
```

With this, we can use interrup to loop through led configs. With each interrupt trigger (which happens every 100ms), we update `counterMs`. When `counterMs` reaches `toggleThresholdMs`, `shouldToggle` is update. `shouldToggle` is read in main loop, triggering the toggle.

## Demo

![demo.gif](demo.gif)