/*
* @brief Contains board specific variables and initialization functions
*/

#include <board.h>

#include <odrive_main.h>
#include <low_level.h>

#include <Drivers/STM32/stm32_timer.hpp>

#include <adc.h>
#include <dma.h>
#include <tim.h>
#include <usart.h>
#include <freertos_vars.h>

extern "C" void SystemClock_Config(void); // defined in main.c generated by CubeMX

Stm32SpiArbiter spi3_arbiter{&hspi3};
Stm32SpiArbiter& ext_spi_arbiter = spi3_arbiter;

UART_HandleTypeDef* uart0 = &huart4;
UART_HandleTypeDef* uart1 = nullptr; // TODO: this could be supported in ODrive v3.6 (or similar) using STM32's USART2
UART_HandleTypeDef* uart2 = nullptr;

const float thermistor_poly_coeffs[] =
    {363.93910201f, -462.15369634f, 307.55129571f, -27.72569531f};
const size_t thermistor_num_coeffs = sizeof(thermistor_poly_coeffs)/sizeof(thermistor_poly_coeffs[1]);

Drv8301 m0_gate_driver{
    &spi3_arbiter,
    {M0_nCS_GPIO_Port, M0_nCS_Pin}, // nCS
    {EN_GATE_GPIO_Port, EN_GATE_Pin}, // EN pin (shared between both motors)
    {nFAULT_GPIO_Port, nFAULT_Pin} // nFAULT pin (shared between both motors)
};

Drv8301 m1_gate_driver{
    &spi3_arbiter,
    {M1_nCS_GPIO_Port, M1_nCS_Pin}, // nCS
    {EN_GATE_GPIO_Port, EN_GATE_Pin}, // EN pin (shared between both motors)
    {nFAULT_GPIO_Port, nFAULT_Pin} // nFAULT pin (shared between both motors)
};

Motor motors[AXIS_COUNT] = {
    {
        &htim1, // timer
        TIM_1_8_PERIOD_CLOCKS, // control_deadline
        1.0f / SHUNT_RESISTANCE, // shunt_conductance [S]
        15, // inverter_thermistor_adc_ch
        m0_gate_driver, // gate_driver
        m0_gate_driver // opamp
    },
    {
        &htim8, // timer
        (3 * TIM_1_8_PERIOD_CLOCKS) / 2, // control_deadline
        1.0f / SHUNT_RESISTANCE, // shunt_conductance [S]
#if HW_VERSION_MAJOR == 3 && HW_VERSION_MINOR >= 3
        4, // inverter_thermistor_adc_ch
#else
        1, // inverter_thermistor_adc_ch
#endif
        m1_gate_driver, // gate_driver
        m1_gate_driver // opamp
    }
};

Encoder encoders[AXIS_COUNT] = {
    {
        &htim3, // timer
        {M0_ENC_Z_GPIO_Port, M0_ENC_Z_Pin}, // index_gpio
        {M0_ENC_A_GPIO_Port, M0_ENC_A_Pin}, // hallA_gpio
        {M0_ENC_B_GPIO_Port, M0_ENC_B_Pin}, // hallB_gpio
        {M0_ENC_Z_GPIO_Port, M0_ENC_Z_Pin}, // hallC_gpio
        &spi3_arbiter // spi_arbiter
    },
    {
        &htim4, // timer
        {M1_ENC_Z_GPIO_Port, M1_ENC_Z_Pin}, // index_gpio
        {M1_ENC_A_GPIO_Port, M1_ENC_A_Pin}, // hallA_gpio
        {M1_ENC_B_GPIO_Port, M1_ENC_B_Pin}, // hallB_gpio
        {M1_ENC_Z_GPIO_Port, M1_ENC_Z_Pin}, // hallC_gpio
        &spi3_arbiter // spi_arbiter
    }
};

// TODO: this has no hardware dependency and should be allocated depending on config
Endstop endstops[2 * AXIS_COUNT];

SensorlessEstimator sensorless_estimators[AXIS_COUNT];
Controller controllers[AXIS_COUNT];
TrapezoidalTrajectory trap[AXIS_COUNT];

std::array<Axis, AXIS_COUNT> axes{{
    {
        0, // axis_num
        1, // step_gpio_pin
        2, // dir_gpio_pin
        (osPriority)(osPriorityHigh + (osPriority)1), // thread_priority
        encoders[0], // encoder
        sensorless_estimators[0], // sensorless_estimator
        controllers[0], // controller
        motors[0], // motor
        trap[0], // trap
        endstops[0], endstops[1], // min_endstop, max_endstop
    },
    {
        1, // axis_num
#if HW_VERSION_MAJOR == 3 && HW_VERSION_MINOR >= 5
        7, // step_gpio_pin
        8, // dir_gpio_pin
#else
        3, // step_gpio_pin
        4, // dir_gpio_pin
#endif
        osPriorityHigh, // thread_priority
        encoders[1], // encoder
        sensorless_estimators[1], // sensorless_estimator
        controllers[1], // controller
        motors[1], // motor
        trap[1], // trap
        endstops[2], endstops[3], // min_endstop, max_endstop
    },
}};



#if (HW_VERSION_MINOR == 1) || (HW_VERSION_MINOR == 2)
Stm32Gpio gpios[] = {
    {nullptr, 0}, // dummy GPIO0 so that PCB labels and software numbers match

    {GPIOB, GPIO_PIN_2},
    {GPIOA, GPIO_PIN_5},
    {GPIOA, GPIO_PIN_4},
    {GPIOA, GPIO_PIN_3},
    {nullptr, 0},
    {nullptr, 0},
    {nullptr, 0},
    {nullptr, 0}

    {GPIOB, GPIO_PIN_4}, // ENC0_A
    {GPIOB, GPIO_PIN_5}, // ENC0_B
    {GPIOA, GPIO_PIN_15}, // ENC0_Z
    {GPIOB, GPIO_PIN_6}, // ENC1_A
    {GPIOB, GPIO_PIN_7}, // ENC1_B
    {GPIOB, GPIO_PIN_3}, // ENC1_Z
    {GPIOB, GPIO_PIN_8}, // CAN_R
    {GPIOB, GPIO_PIN_9}, // CAN_D
};
#elif (HW_VERSION_MINOR == 3) || (HW_VERSION_MINOR == 4)
Stm32Gpio gpios[] = {
    {nullptr, 0}, // dummy GPIO0 so that PCB labels and software numbers match

    {GPIOA, GPIO_PIN_0},
    {GPIOA, GPIO_PIN_1},
    {GPIOA, GPIO_PIN_2},
    {GPIOA, GPIO_PIN_3},
    {GPIOB, GPIO_PIN_2},
    {nullptr, 0},
    {nullptr, 0},
    {nullptr, 0},

    {GPIOB, GPIO_PIN_4}, // ENC0_A
    {GPIOB, GPIO_PIN_5}, // ENC0_B
    {GPIOA, GPIO_PIN_15}, // ENC0_Z
    {GPIOB, GPIO_PIN_6}, // ENC1_A
    {GPIOB, GPIO_PIN_7}, // ENC1_B
    {GPIOB, GPIO_PIN_3}, // ENC1_Z
    {GPIOB, GPIO_PIN_8}, // CAN_R
    {GPIOB, GPIO_PIN_9}, // CAN_D
};
#elif (HW_VERSION_MINOR == 5) || (HW_VERSION_MINOR == 6)
Stm32Gpio gpios[GPIO_COUNT] = {
    {nullptr, 0}, // dummy GPIO0 so that PCB labels and software numbers match

    {GPIOA, GPIO_PIN_0},
    {GPIOA, GPIO_PIN_1},
    {GPIOA, GPIO_PIN_2},
    {GPIOA, GPIO_PIN_3},
    {GPIOC, GPIO_PIN_4},
    {GPIOB, GPIO_PIN_2},
    {GPIOA, GPIO_PIN_15},
    {GPIOB, GPIO_PIN_3},
    
    {GPIOB, GPIO_PIN_4}, // ENC0_A
    {GPIOB, GPIO_PIN_5}, // ENC0_B
    {GPIOC, GPIO_PIN_9}, // ENC0_Z
    {GPIOB, GPIO_PIN_6}, // ENC1_A
    {GPIOB, GPIO_PIN_7}, // ENC1_B
    {GPIOC, GPIO_PIN_15}, // ENC1_Z
    {GPIOB, GPIO_PIN_8}, // CAN_R
    {GPIOB, GPIO_PIN_9}, // CAN_D
};
#else
#error "unknown GPIOs"
#endif

#define GPIO_AF_NONE ((uint8_t)0xff)

// The columns go like this:
//  UART0 | UART1 | UART2 | CAN0 | I2C0 | SPI0 | PWM0 | ENC0 | ENC1 | ENC2
uint8_t alternate_functions[GPIO_COUNT][10] = {
    {GPIO_AF_NONE, GPIO_AF_NONE, GPIO_AF_NONE, GPIO_AF_NONE, GPIO_AF_NONE, GPIO_AF_NONE, GPIO_AF_NONE, GPIO_AF_NONE, GPIO_AF_NONE, GPIO_AF_NONE},
    
#if HW_VERSION_MINOR >= 3
    {GPIO_AF8_UART4, GPIO_AF_NONE, GPIO_AF_NONE, GPIO_AF_NONE, GPIO_AF_NONE, GPIO_AF_NONE, GPIO_AF2_TIM5, GPIO_AF_NONE, GPIO_AF_NONE, GPIO_AF_NONE},
    {GPIO_AF8_UART4, GPIO_AF_NONE, GPIO_AF_NONE, GPIO_AF_NONE, GPIO_AF_NONE, GPIO_AF_NONE, GPIO_AF2_TIM5, GPIO_AF_NONE, GPIO_AF_NONE, GPIO_AF_NONE},
    {GPIO_AF_NONE, GPIO_AF_NONE, GPIO_AF_NONE, GPIO_AF_NONE, GPIO_AF_NONE, GPIO_AF_NONE, GPIO_AF2_TIM5, GPIO_AF_NONE, GPIO_AF_NONE, GPIO_AF_NONE},
#else
    {GPIO_AF_NONE, GPIO_AF_NONE, GPIO_AF_NONE, GPIO_AF_NONE, GPIO_AF_NONE, GPIO_AF_NONE, GPIO_AF_NONE, GPIO_AF_NONE, GPIO_AF_NONE, GPIO_AF_NONE},
    {GPIO_AF_NONE, GPIO_AF_NONE, GPIO_AF_NONE, GPIO_AF_NONE, GPIO_AF_NONE, GPIO_AF_NONE, GPIO_AF_NONE, GPIO_AF_NONE, GPIO_AF_NONE, GPIO_AF_NONE},
    {GPIO_AF_NONE, GPIO_AF_NONE, GPIO_AF_NONE, GPIO_AF_NONE, GPIO_AF_NONE, GPIO_AF_NONE, GPIO_AF_NONE, GPIO_AF_NONE, GPIO_AF_NONE, GPIO_AF_NONE},
#endif
    {GPIO_AF_NONE, GPIO_AF_NONE, GPIO_AF_NONE, GPIO_AF_NONE, GPIO_AF_NONE, GPIO_AF_NONE, GPIO_AF2_TIM5, GPIO_AF_NONE, GPIO_AF_NONE, GPIO_AF_NONE},
    {GPIO_AF_NONE, GPIO_AF_NONE, GPIO_AF_NONE, GPIO_AF_NONE, GPIO_AF_NONE, GPIO_AF_NONE, GPIO_AF_NONE, GPIO_AF_NONE, GPIO_AF_NONE, GPIO_AF_NONE},
    {GPIO_AF_NONE, GPIO_AF_NONE, GPIO_AF_NONE, GPIO_AF_NONE, GPIO_AF_NONE, GPIO_AF_NONE, GPIO_AF_NONE, GPIO_AF_NONE, GPIO_AF_NONE, GPIO_AF_NONE},
    {GPIO_AF_NONE, GPIO_AF_NONE, GPIO_AF_NONE, GPIO_AF_NONE, GPIO_AF_NONE, GPIO_AF_NONE, GPIO_AF_NONE, GPIO_AF_NONE, GPIO_AF_NONE, GPIO_AF_NONE},
    {GPIO_AF_NONE, GPIO_AF_NONE, GPIO_AF_NONE, GPIO_AF_NONE, GPIO_AF_NONE, GPIO_AF_NONE, GPIO_AF_NONE, GPIO_AF_NONE, GPIO_AF_NONE, GPIO_AF_NONE},

    {GPIO_AF_NONE, GPIO_AF_NONE, GPIO_AF_NONE, GPIO_AF_NONE, GPIO_AF_NONE, GPIO_AF_NONE, GPIO_AF_NONE, GPIO_AF2_TIM3, GPIO_AF_NONE, GPIO_AF_NONE},
    {GPIO_AF_NONE, GPIO_AF_NONE, GPIO_AF_NONE, GPIO_AF_NONE, GPIO_AF_NONE, GPIO_AF_NONE, GPIO_AF_NONE, GPIO_AF2_TIM3, GPIO_AF_NONE, GPIO_AF_NONE},
    {GPIO_AF_NONE, GPIO_AF_NONE, GPIO_AF_NONE, GPIO_AF_NONE, GPIO_AF_NONE, GPIO_AF_NONE, GPIO_AF_NONE, GPIO_AF_NONE, GPIO_AF_NONE, GPIO_AF_NONE},
    {GPIO_AF_NONE, GPIO_AF_NONE, GPIO_AF_NONE, GPIO_AF_NONE, GPIO_AF4_I2C1, GPIO_AF_NONE, GPIO_AF_NONE, GPIO_AF_NONE, GPIO_AF2_TIM4, GPIO_AF_NONE},
    {GPIO_AF_NONE, GPIO_AF_NONE, GPIO_AF_NONE, GPIO_AF_NONE, GPIO_AF4_I2C1, GPIO_AF_NONE, GPIO_AF_NONE, GPIO_AF_NONE, GPIO_AF2_TIM4, GPIO_AF_NONE},
    {GPIO_AF_NONE, GPIO_AF_NONE, GPIO_AF_NONE, GPIO_AF_NONE, GPIO_AF_NONE, GPIO_AF_NONE, GPIO_AF_NONE, GPIO_AF_NONE, GPIO_AF_NONE, GPIO_AF_NONE},
    {GPIO_AF_NONE, GPIO_AF_NONE, GPIO_AF_NONE, GPIO_AF9_CAN1, GPIO_AF4_I2C1, GPIO_AF_NONE, GPIO_AF_NONE, GPIO_AF_NONE, GPIO_AF_NONE, GPIO_AF_NONE},
    {GPIO_AF_NONE, GPIO_AF_NONE, GPIO_AF_NONE, GPIO_AF9_CAN1, GPIO_AF4_I2C1, GPIO_AF_NONE, GPIO_AF_NONE, GPIO_AF_NONE, GPIO_AF_NONE, GPIO_AF_NONE},
};

#if HW_VERSION_MINOR <= 2
PwmInput pwm0_input{&htim5, {0, 0, 0, 4}}; // 0 means not in use
#else
PwmInput pwm0_input{&htim5, {1, 2, 3, 4}};
#endif

extern PCD_HandleTypeDef hpcd_USB_OTG_FS; // defined in usbd_conf.c
PCD_HandleTypeDef& usb_pcd_handle = hpcd_USB_OTG_FS;
extern USBD_HandleTypeDef hUsbDeviceFS;
USBD_HandleTypeDef& usb_dev_handle = hUsbDeviceFS;

void system_init() {
    // Reset of all peripherals, Initializes the Flash interface and the Systick.
    HAL_Init();

    // Configure the system clock
    SystemClock_Config();
}

void board_init() {
    // Initialize all configured peripherals
    MX_GPIO_Init();
    MX_DMA_Init();
    MX_ADC1_Init();
    MX_ADC2_Init();
    MX_TIM1_Init();
    MX_TIM8_Init();
    MX_TIM3_Init();
    MX_TIM4_Init();
    MX_SPI3_Init();
    MX_ADC3_Init();
    MX_TIM2_Init();
    MX_UART4_Init();
    MX_TIM5_Init();
    MX_TIM13_Init();

    HAL_UART_DeInit(uart0);
    uart0->Init.BaudRate = odrv.config_.uart0_baudrate;
    HAL_UART_Init(uart0);

    if (odrv.config_.enable_i2c0) {
        // Set up the direction GPIO as input
        get_gpio(3).config(GPIO_MODE_INPUT, GPIO_PULLUP);
        get_gpio(4).config(GPIO_MODE_INPUT, GPIO_PULLUP);
        get_gpio(5).config(GPIO_MODE_INPUT, GPIO_PULLUP);

        osDelay(1); // This has no effect but was here before.
        i2c_stats_.addr = (0xD << 3);
        i2c_stats_.addr |= get_gpio(3).read() ? 0x1 : 0;
        i2c_stats_.addr |= get_gpio(4).read() ? 0x2 : 0;
        i2c_stats_.addr |= get_gpio(5).read() ? 0x4 : 0;
        MX_I2C1_Init(i2c_stats_.addr);
    }

    if (odrv.config_.enable_can0) {
        // The CAN initialization will (and must) init its own GPIOs before the
        // GPIO modes are initialized. Therefore we ensure that the later GPIO
        // mode initialization won't override the CAN mode.
        if (odrv.config_.gpio_modes[15] != ODriveIntf::GPIO_MODE_CAN0 || odrv.config_.gpio_modes[16] != ODriveIntf::GPIO_MODE_CAN0) {
            odrv.misconfigured_ = true;
        } else {
            MX_CAN1_Init();
        }
    }

    // Ensure that debug halting of the core doesn't leave the motor PWM running
    __HAL_DBGMCU_FREEZE_TIM1();
    __HAL_DBGMCU_FREEZE_TIM8();
    __HAL_DBGMCU_FREEZE_TIM13();

    /*
    * Initial intention of the synchronization:
    * Synchronize TIM1, TIM8 and TIM13 such that:
    *  1. The triangle waveform of TIM1 leads the triangle waveform of TIM8 by a
    *     90° phase shift.
    *  2. The timer update events of TIM1 and TIM8 are symmetrically interleaved.
    *  3. Each TIM13 reload coincides with a TIM1 lower update event.
    * 
    * However right now this synchronization only ensures point (1) and (3) but because
    * TIM1 and TIM3 only trigger an update on every third reload, this does not
    * allow for (2).
    * 
    * TODO: revisit the timing topic in general.
    * 
    */
    Stm32Timer::start_synchronously<3>(
        {&htim1, &htim8, &htim13},
        {TIM_1_8_PERIOD_CLOCKS / 2 - 1 * 128 /* TODO: explain why this offset */, 0, TIM_1_8_PERIOD_CLOCKS / 2 - 1 * 128}
    );
}


extern "C" {

void HAL_SPI_TxCpltCallback(SPI_HandleTypeDef *hspi) {
    HAL_SPI_TxRxCpltCallback(hspi);
}

void HAL_SPI_RxCpltCallback(SPI_HandleTypeDef *hspi) {
    HAL_SPI_TxRxCpltCallback(hspi);
}

void HAL_SPI_TxRxCpltCallback(SPI_HandleTypeDef *hspi) {
    if (hspi == &hspi3) {
        spi3_arbiter.on_complete();
    }
}



void TIM1_UP_TIM10_IRQHandler(void) {
    __HAL_TIM_CLEAR_IT(&htim1, TIM_IT_UPDATE);
    motors[0].tim_update_cb();
}

void TIM8_UP_TIM13_IRQHandler(void) {
    __HAL_TIM_CLEAR_IT(&htim8, TIM_IT_UPDATE);
    motors[1].tim_update_cb();
}

void TIM5_IRQHandler(void) {
    pwm0_input.on_capture();
}

void ADC_IRQ_Dispatch(ADC_HandleTypeDef* hadc, void(*callback)(ADC_HandleTypeDef* hadc, bool injected)) {
    // Injected measurements
    uint32_t JEOC = __HAL_ADC_GET_FLAG(hadc, ADC_FLAG_JEOC);
    uint32_t JEOC_IT_EN = __HAL_ADC_GET_IT_SOURCE(hadc, ADC_IT_JEOC);
    if (JEOC && JEOC_IT_EN) {
        callback(hadc, true);
        __HAL_ADC_CLEAR_FLAG(hadc, (ADC_FLAG_JSTRT | ADC_FLAG_JEOC));
    }
    // Regular measurements
    uint32_t EOC = __HAL_ADC_GET_FLAG(hadc, ADC_FLAG_EOC);
    uint32_t EOC_IT_EN = __HAL_ADC_GET_IT_SOURCE(hadc, ADC_IT_EOC);
    if (EOC && EOC_IT_EN) {
        callback(hadc, false);
        __HAL_ADC_CLEAR_FLAG(hadc, (ADC_FLAG_STRT | ADC_FLAG_EOC));
    }
}

void ADC_IRQHandler(void) {
    // The HAL's ADC handling mechanism adds many clock cycles of overhead
    // So we bypass it and handle the logic ourselves.
    //@TODO add vbus measurement on adc1 here
    ADC_IRQ_Dispatch(&hadc1, &vbus_sense_adc_cb);
    ADC_IRQ_Dispatch(&hadc2, &pwm_trig_adc_cb);
    ADC_IRQ_Dispatch(&hadc3, &pwm_trig_adc_cb);
}

void I2C1_EV_IRQHandler(void) {
  HAL_I2C_EV_IRQHandler(&hi2c1);
}

void I2C1_ER_IRQHandler(void) {
  HAL_I2C_ER_IRQHandler(&hi2c1);
}

void OTG_FS_IRQHandler(void) {
    // Mask interrupt, and signal processing of interrupt by usb_cmd_thread
    // The thread will re-enable the interrupt when all pending irqs are clear.
    HAL_NVIC_DisableIRQ(OTG_FS_IRQn);
    osSemaphoreRelease(sem_usb_irq);
}

}
