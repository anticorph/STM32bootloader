#include "stm32f4xx.h"
#include "uart_driver.h"
#include "timebase.h"

#define BOOT_UART            USART2
#define BOOT_UART_CLK_EN()   (RCC->APB1ENR  |= RCC_APB1ENR_USART2EN)
#define BOOT_UART_GPIO_EN()  (RCC->AHB1ENR  |= RCC_AHB1ENR_GPIOAEN)
#define BOOT_UART_PCLK_HZ    16000000UL   // HSI, no PLL - see timebase_init() call site

void uart_init(uint32_t baudrate)
{
    BOOT_UART_GPIO_EN();
    BOOT_UART_CLK_EN();

    // PA2 = USART2_TX, PA3 = USART2_RX, alternate function AF7
    GPIOA->MODER   &= ~((3U << (2U * 2U)) | (3U << (3U * 2U)));
    GPIOA->MODER   |=  ((2U << (2U * 2U)) | (2U << (3U * 2U)));
    GPIOA->AFR[0]  &= ~((0xFU << (4U * 2U)) | (0xFU << (4U * 3U)));
    GPIOA->AFR[0]  |=  ((7U   << (4U * 2U)) | (7U   << (4U * 3U)));
    GPIOA->OSPEEDR |=  ((3U << (2U * 2U)) | (3U << (3U * 2U)));

    BOOT_UART->CR1 = 0;
    uint32_t usartdiv = (BOOT_UART_PCLK_HZ + (baudrate / 2U)) / baudrate;
    BOOT_UART->BRR = usartdiv;
    BOOT_UART->CR1 = USART_CR1_TE | USART_CR1_RE | USART_CR1_UE;
}

void uart_send_byte(uint8_t b)
{
    while (!(BOOT_UART->SR & USART_SR_TXE)) { }
    BOOT_UART->DR = b;
    while (!(BOOT_UART->SR & USART_SR_TC)) { }
}

void uart_send_buf(const uint8_t *buf, uint32_t len)
{
    for (uint32_t i = 0; i < len; i++)
    {
        uart_send_byte(buf[i]);
    }
}

int uart_recv_byte_timeout(uint8_t *b, uint32_t timeout_ms)
{
    uint32_t start = millis();
    while ((millis() - start) < timeout_ms)
    {
        if (BOOT_UART->SR & USART_SR_RXNE)
        {
            *b = (uint8_t)BOOT_UART->DR;
            return 1;
        }
    }
    return 0;
}
