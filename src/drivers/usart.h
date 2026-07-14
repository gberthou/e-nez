#ifndef DRIVERS_USART_H
#define DRIVERS_USART_H

void usart_config_uart(unsigned index, unsigned baudrate);
void usart_uart_putc(unsigned index, char c);

#endif // DRIVERS_USART_H
