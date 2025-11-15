#include "em_chip.h"
#include "em_cmu.h"
#include "em_gpio.h"
#include "em_usart.h"
#include "em_ldma.h"
#include "em_timer.h"
#include "em_emu.h"
#include <stdio.h>
#include <string.h>

#define BSP_TXPORT gpioPortA
#define BSP_RXPORT gpioPortA
#define BSP_TXPIN 5
#define BSP_RXPIN 6
#define BSP_ENABLE_PORT gpioPortD
#define BSP_ENABLE_PIN 4

#define RX_LDMA_CHANNEL 0
#define TX_LDMA_CHANNEL 1

#define BUF10   10
#define BUF100  100
#define BUF1000 1000

// ================== Biến toàn cục ======================
uint8_t data10[BUF10];
uint8_t data100[BUF100];
uint8_t data1000[BUF1000];
uint8_t rx_char;

LDMA_Descriptor_t txDesc;
LDMA_TransferCfg_t txCfg;

// ================== GPIO ======================
void initGPIO(void)
{
  GPIO_PinModeSet(BSP_TXPORT, BSP_TXPIN, gpioModePushPull, 1);
  GPIO_PinModeSet(BSP_RXPORT, BSP_RXPIN, gpioModeInput, 0);
  GPIO_PinModeSet(BSP_ENABLE_PORT, BSP_ENABLE_PIN, gpioModePushPull, 1);
}

// ================== USART ======================
void initUSART0(void)
{
  USART_InitAsync_TypeDef init = USART_INITASYNC_DEFAULT;
  USART_InitAsync(USART0, &init);

  GPIO->USARTROUTE[0].TXROUTE = (BSP_TXPORT << _GPIO_USART_TXROUTE_PORT_SHIFT)
                               | (BSP_TXPIN << _GPIO_USART_TXROUTE_PIN_SHIFT);
  GPIO->USARTROUTE[0].RXROUTE = (BSP_RXPORT << _GPIO_USART_RXROUTE_PORT_SHIFT)
                               | (BSP_RXPIN << _GPIO_USART_RXROUTE_PIN_SHIFT);

  GPIO->USARTROUTE[0].ROUTEEN = GPIO_USART_ROUTEEN_RXPEN | GPIO_USART_ROUTEEN_TXPEN;
  USART_Enable(USART0, usartEnable);
}

// ================== TIMER0 ======================
void initTIMER0(void)
{
  CMU_ClockEnable(cmuClock_TIMER0, true);
  TIMER_Init_TypeDef timerInit = TIMER_INIT_DEFAULT;
  TIMER_Init(TIMER0, &timerInit);
  TIMER_TopSet(TIMER0, 0xFFFFFFFF);
}

uint32_t calculatePeriod(uint32_t numClk)
{
  uint32_t timerClockMHz = CMU_ClockFreqGet(cmuClock_TIMER0) / 1000000;
  return (numClk / timerClockMHz);
}

uint32_t timeMeasure(uint8_t *data, uint32_t length)
{
  uint32_t numClk = 0;
  uint32_t elapsedTime = 0;

  // Reset DMA channel
  LDMA->CHEN &= ~(1 << TX_LDMA_CHANNEL);
  LDMA->CHDONE |= (1 << TX_LDMA_CHANNEL); // Clear done flag

  // Reset Timer counter
  TIMER_CounterSet(TIMER0, 0);
  TIMER_Enable(TIMER0, true);

  for (int i = 0; i < length; i++) {
    while (!(USART0->STATUS & USART_STATUS_TXBL));
    USART_Tx(USART0, data[i]);
  }

  // Đợi DMA hoàn thành
  while ((LDMA->CHDONE & (1 << TX_LDMA_CHANNEL)) == 0);
  for (volatile int d = 0; d < 10000; d++); // Delay ngắn
  TIMER_Enable(TIMER0, false);
  numClk = TIMER_CounterGet(TIMER0);
  elapsedTime = calculatePeriod(numClk);

  return elapsedTime;
}

// ================== LDMA ======================
void initLDMA(void)
{
  LDMA_Init_t init = LDMA_INIT_DEFAULT;
  LDMA_Init(&init);
}

// ================== MAIN ======================
int main(void)
{
  CHIP_Init();
  CMU_ClockEnable(cmuClock_GPIO, true);
  CMU_ClockEnable(cmuClock_USART0, true);
  CMU_ClockEnable(cmuClock_LDMA, true);

  initGPIO();
  initUSART0();
  initLDMA();
  initTIMER0();

  // Tạo dữ liệu ngẫu nhiên / mẫu
  for (int i = 0; i < BUF10; i++) data10[i] = 'A' + (i % 26);
  for (int i = 0; i < BUF100; i++) data100[i] = 'a' + (i % 26);
  for (int i = 0; i < BUF1000; i++) data1000[i] = '0' + (i % 10);

  while (1)
  {
    // Chờ người dùng gửi lệnh qua UART
    rx_char = USART_Rx(USART0);

    uint32_t elapsed = 0;
    char msg[64];

    if (rx_char == '1') {
      elapsed = timeMeasure(data10, BUF10);
      snprintf(msg, sizeof(msg), "\r\n%lu us\r\n", elapsed);
      for (int i = 0; i < BUF10; i++) USART_Tx(USART0, data10[i]);
      for (int i = 0; msg[i] != '\0'; i++) USART_Tx(USART0, msg[i]);
    }
    else if (rx_char == '2') {
      elapsed = timeMeasure(data100, BUF100);
      snprintf(msg, sizeof(msg), "\r\n%lu us\r\n", elapsed);
      for (int i = 0; i < BUF100; i++) USART_Tx(USART0, data100[i]);
      for (int i = 0; msg[i] != '\0'; i++) USART_Tx(USART0, msg[i]);
    }
    else if (rx_char == '3') {
      elapsed = timeMeasure(data1000, BUF1000);
      snprintf(msg, sizeof(msg), "\r\n%lu us\r\n", elapsed);
      for (int i = 0; i < BUF1000; i++) USART_Tx(USART0, data1000[i]);
      for (int i = 0; msg[i] != '\0'; i++) USART_Tx(USART0, msg[i]);
    }
    else {
      const char *err = "\r\nInvalid command. Send 1, 2, or 3.\r\n";
      for (int i = 0; err[i] != '\0'; i++) USART_Tx(USART0, err[i]);
    }
  }
}
