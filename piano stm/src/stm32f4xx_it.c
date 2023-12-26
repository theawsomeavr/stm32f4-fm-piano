#include "stm32f4xx.h"
#include "stm32f4xx_ll_rcc.h"
#include "stm32f4xx_ll_system.h"
#include "stm32f4xx_ll_cortex.h"
#include "stm32f4xx_ll_utils.h"
#include "stm32f4xx_ll_pwr.h"
#include "stm32f4xx_ll_dma.h"

void NMI_Handler(void){ while (1); }
void HardFault_Handler(void) { while(1); }
void MemManage_Handler(void){ while(1); }
void BusFault_Handler(void){ while(1); }
void UsageFault_Handler(void){ while (1); }
void SVC_Handler(void){}
void DebugMon_Handler(void){}
void PendSV_Handler(void){}
void Error_Handler(void);

void SystemClock_Config(void){
    LL_FLASH_SetLatency(LL_FLASH_LATENCY_3);
    while(LL_FLASH_GetLatency()!= LL_FLASH_LATENCY_3){
    }
    LL_PWR_SetRegulVoltageScaling(LL_PWR_REGU_VOLTAGE_SCALE1);
    LL_RCC_HSE_Enable();

    /* Wait till HSE is ready */
    while(LL_RCC_HSE_IsReady() != 1);
    LL_RCC_PLL_ConfigDomain_SYS(LL_RCC_PLLSOURCE_HSE, LL_RCC_PLLM_DIV_25, 192, LL_RCC_PLLP_DIV_2);
    LL_RCC_PLL_ConfigDomain_48M(LL_RCC_PLLSOURCE_HSE, LL_RCC_PLLM_DIV_25, 192, LL_RCC_PLLQ_DIV_4);
    LL_RCC_PLL_Enable();

    /* Wait till PLL is ready */
    while(LL_RCC_PLL_IsReady() != 1);

    while(LL_PWR_IsActiveFlag_VOS() == 0);

    LL_RCC_SetAHBPrescaler(LL_RCC_SYSCLK_DIV_1);
    LL_RCC_SetAPB1Prescaler(LL_RCC_APB1_DIV_2);
    LL_RCC_SetAPB2Prescaler(LL_RCC_APB2_DIV_1);
    LL_RCC_SetSysClkSource(LL_RCC_SYS_CLKSOURCE_PLL);

    /* Wait till System clock is ready */
    while(LL_RCC_GetSysClkSource() != LL_RCC_SYS_CLKSOURCE_STATUS_PLL){

    }
    LL_SetSystemCoreClock(96000000);

    /* Update the time base */
    if(HAL_InitTick (TICK_INT_PRIORITY) != HAL_OK){
        Error_Handler();
    }
    LL_RCC_SetTIMPrescaler(LL_RCC_TIM_PRESCALER_TWICE);
}

void Error_Handler(void)
{
    __disable_irq();
    while(1);
}

extern PCD_HandleTypeDef hpcd_USB_OTG_FS;
void SysTick_Handler(void){ HAL_IncTick(); }
void OTG_FS_IRQHandler(void){ HAL_PCD_IRQHandler(&hpcd_USB_OTG_FS); }

void HAL_MspInit(void){
    __HAL_RCC_SYSCFG_CLK_ENABLE();
    __HAL_RCC_PWR_CLK_ENABLE();
}
