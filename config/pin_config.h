#ifndef PIN_CONFIG_H
#define PIN_CONFIG_H

// $[CMU]
// [CMU]$

// $[LFXO]
// [LFXO]$

// $[PRS.ASYNCH0]
// [PRS.ASYNCH0]$

// $[PRS.ASYNCH1]
// [PRS.ASYNCH1]$

// $[PRS.ASYNCH2]
// [PRS.ASYNCH2]$

// $[PRS.ASYNCH3]
// [PRS.ASYNCH3]$

// $[PRS.ASYNCH4]
// [PRS.ASYNCH4]$

// $[PRS.ASYNCH5]
// [PRS.ASYNCH5]$

// $[PRS.ASYNCH6]
// [PRS.ASYNCH6]$

// $[PRS.ASYNCH7]
// [PRS.ASYNCH7]$

// $[PRS.ASYNCH8]
// [PRS.ASYNCH8]$

// $[PRS.ASYNCH9]
// [PRS.ASYNCH9]$

// $[PRS.ASYNCH10]
// [PRS.ASYNCH10]$

// $[PRS.ASYNCH11]
// [PRS.ASYNCH11]$

// $[PRS.SYNCH0]
// [PRS.SYNCH0]$

// $[PRS.SYNCH1]
// [PRS.SYNCH1]$

// $[PRS.SYNCH2]
// [PRS.SYNCH2]$

// $[PRS.SYNCH3]
// [PRS.SYNCH3]$

// $[GPIO]
// [GPIO]$

// $[TIMER0]
// [TIMER0]$

// $[TIMER1]
// [TIMER1]$

// $[TIMER2]
// [TIMER2]$

// $[TIMER3]
// [TIMER3]$

// $[USART0]
// USART0 RX on PC01
#ifndef USART0_RX_PORT                          
#define USART0_RX_PORT                           gpioPortC
#endif
#ifndef USART0_RX_PIN                           
#define USART0_RX_PIN                            1
#endif

// USART0 TX on PC00
#ifndef USART0_TX_PORT                          
#define USART0_TX_PORT                           gpioPortC
#endif
#ifndef USART0_TX_PIN                           
#define USART0_TX_PIN                            0
#endif

// [USART0]$

// $[USART1]
// [USART1]$

// $[USART2]
// [USART2]$

// $[I2C1]
// [I2C1]$

// $[LETIMER0]
// [LETIMER0]$

// $[ACMP0]
// [ACMP0]$

// $[ACMP1]
// [ACMP1]$

// $[I2C0]
// [I2C0]$

// $[PTI]
// [PTI]$

// $[MODEM]
// [MODEM]$

// $[CUSTOM_PIN_NAME]
#ifndef LED_CH4_PORT                            
#define LED_CH4_PORT                             gpioPortA
#endif
#ifndef LED_CH4_PIN                             
#define LED_CH4_PIN                              3
#endif

#ifndef LED_CH3_PORT                            
#define LED_CH3_PORT                             gpioPortA
#endif
#ifndef LED_CH3_PIN                             
#define LED_CH3_PIN                              4
#endif

#ifndef VCC_SENSE_PORT                          
#define VCC_SENSE_PORT                           gpioPortC
#endif
#ifndef VCC_SENSE_PIN                           
#define VCC_SENSE_PIN                            3
#endif

#ifndef LED_AUX_PORT                            
#define LED_AUX_PORT                             gpioPortC
#endif
#ifndef LED_AUX_PIN                             
#define LED_AUX_PIN                              5
#endif

#ifndef LED_CH1_PORT                            
#define LED_CH1_PORT                             gpioPortD
#endif
#ifndef LED_CH1_PIN                             
#define LED_CH1_PIN                              0
#endif

#ifndef LED_CH2_PORT                            
#define LED_CH2_PORT                             gpioPortD
#endif
#ifndef LED_CH2_PIN                             
#define LED_CH2_PIN                              1
#endif

// [CUSTOM_PIN_NAME]$

#endif // PIN_CONFIG_H

// $[IADC0]
// [IADC0]$

