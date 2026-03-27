

/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include <string.h>
#include <stdio.h>

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define MAX_BUFFER_SIZE RPMSG_BUFFER_SIZE

/* HC-SR04 Pin Definitions */
#define TRIG_PORT       GPIOD
#define TRIG_PIN        GPIO_PIN_1      /* PD1 = Arduino D7 */
#define ECHO_PORT       GPIOE
#define ECHO_PIN        GPIO_PIN_14     /* PE14 = Arduino D11 */

/* Measurement settings */
#define SENSOR_READ_INTERVAL_MS   100   /* Read sensor every 100ms (10 Hz) */
#define ECHO_TIMEOUT_US           30000 /* 30ms timeout = ~5 meters max */

/* DWT (Data Watchpoint and Trace) registers for cycle counting */
#define DWT_CONTROL   (*(volatile uint32_t *)0xE0001000)
#define DWT_CYCCNT    (*(volatile uint32_t *)0xE0001004)
#define SCB_DEMCR     (*(volatile uint32_t *)0xE000EDFC)

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
IPCC_HandleTypeDef hipcc;

/* USER CODE BEGIN PV */
VIRT_UART_HandleTypeDef huart0;
VIRT_UART_HandleTypeDef huart1;

__IO FlagStatus VirtUart0RxMsg = RESET;
uint8_t VirtUart0ChannelBuffRx[MAX_BUFFER_SIZE];
uint16_t VirtUart0ChannelRxSize = 0;

__IO FlagStatus VirtUart1RxMsg = RESET;
uint8_t VirtUart1ChannelBuffRx[MAX_BUFFER_SIZE];
uint16_t VirtUart1ChannelRxSize = 0;

/* Sensor data */
volatile float last_distance_cm = 0.0f;
volatile uint8_t sensor_active = 1;

/* CPU clock frequency for microsecond conversion */
static uint32_t cpu_freq_mhz = 209;  /* M4 runs at ~209 MHz */

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_IPCC_Init(void);
static void MX_GPIO_Init_Sensor(void);
/* USER CODE BEGIN PFP */
void VIRT_UART0_RxCpltCallback(VIRT_UART_HandleTypeDef *huart);
void VIRT_UART1_RxCpltCallback(VIRT_UART_HandleTypeDef *huart);
static void DWT_Init(void);
static void delay_us(uint32_t us);
static uint32_t get_us(void);
static float HCSR04_ReadDistance(void);
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

/**
  * @brief  Initialize DWT cycle counter for microsecond timing
  */
static void DWT_Init(void)
{
  SCB_DEMCR |= 0x01000000;   /* Enable TRC (trace) */
  DWT_CYCCNT = 0;             /* Reset cycle counter */
  DWT_CONTROL |= 1;           /* Enable cycle counter */
}

/**
  * @brief  Get current time in microseconds from DWT counter
  * @retval Microseconds since DWT was initialized
  */
static uint32_t get_us(void)
{
  return DWT_CYCCNT / cpu_freq_mhz;
}

/**
  * @brief  Microsecond delay using DWT cycle counter
  * @param  us: microseconds to wait
  */
static void delay_us(uint32_t us)
{
  uint32_t start = DWT_CYCCNT;
  uint32_t cycles = us * cpu_freq_mhz;
  while ((DWT_CYCCNT - start) < cycles) {
    /* wait */
  }
}

/**
  * @brief  Read distance from HC-SR04 sensor
  * @retval Distance in centimeters, or -1.0 on timeout
  */
static float HCSR04_ReadDistance(void)
{
  uint32_t start_us, echo_start_us, echo_end_us;
  uint32_t echo_duration;

  /* 1. Send 10us trigger pulse */
  HAL_GPIO_WritePin(TRIG_PORT, TRIG_PIN, GPIO_PIN_RESET);
  delay_us(2);
  HAL_GPIO_WritePin(TRIG_PORT, TRIG_PIN, GPIO_PIN_SET);
  delay_us(10);
  HAL_GPIO_WritePin(TRIG_PORT, TRIG_PIN, GPIO_PIN_RESET);

  /* 2. Wait for Echo pin to go HIGH (start of echo pulse) */
  start_us = get_us();
  while (HAL_GPIO_ReadPin(ECHO_PORT, ECHO_PIN) == GPIO_PIN_RESET) {
    if ((get_us() - start_us) > ECHO_TIMEOUT_US) {
      return -1.0f;  /* timeout waiting for echo to start */
    }
  }
  echo_start_us = get_us();

  /* 3. Measure how long Echo stays HIGH */
  while (HAL_GPIO_ReadPin(ECHO_PORT, ECHO_PIN) == GPIO_PIN_SET) {
    if ((get_us() - echo_start_us) > ECHO_TIMEOUT_US) {
      return -1.0f;  /* timeout waiting for echo to end */
    }
  }
  echo_end_us = get_us();

  echo_duration = echo_end_us - echo_start_us;

  /* 4. Calculate distance: speed of sound = 343 m/s = 0.0343 cm/us
   *    Divide by 2 because sound travels to object and back
   *
   *    distance_cm = echo_duration_us * 0.0343 / 2
   *    distance_cm = echo_duration_us * 0.01715
   */
  return (float)echo_duration * 0.01715f;
}

/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{
  /* USER CODE BEGIN 1 */
  uint32_t last_read_tick = 0;
  char msg_buffer[64];

  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initialize the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */
  if(IS_ENGINEERING_BOOT_MODE())
  {
    /* Configure the system clock */
    SystemClock_Config();
  }

  log_info("Cortex-M4 boot successful with STM32Cube FW version: v%ld.%ld.%ld \r\n",
                                            (long)((HAL_GetHalVersion() >> 24) & 0x000000FF),
                                            (long)((HAL_GetHalVersion() >> 16) & 0x000000FF),
                                            (long)((HAL_GetHalVersion() >> 8) & 0x000000FF));
  /*HW semaphore Clock enable*/
  __HAL_RCC_HSEM_CLK_ENABLE();

  BSP_LED_Init(LED7);
  BSP_LED_On(LED7);
  /* USER CODE END Init */

  /* IPCC initialisation */
  MX_IPCC_Init();
  /* OpenAmp initialisation ---------------------------------*/
  MX_OPENAMP_Init(RPMSG_REMOTE, NULL);

  /* USER CODE BEGIN SysInit */
  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */

  /* USER CODE BEGIN 2 */

  /* Initialize DWT cycle counter for microsecond timing */
  DWT_Init();

  /* Initialize sensor GPIO pins */
  MX_GPIO_Init_Sensor();

  log_info("HC-SR04 sensor GPIO initialized\r\n");
  log_info("  Trig: PD1 (Arduino D7)\r\n");
  log_info("  Echo: PE14 (Arduino D11) via voltage divider\r\n");

  /*
   * Create Virtual UART device
   * defined by a rpmsg channel attached to the remote device
   */
  log_info("Virtual UART0 OpenAMP-rpmsg channel creation\r\n");
  if (VIRT_UART_Init(&huart0) != VIRT_UART_OK) {
    log_err("VIRT_UART_Init UART0 failed.\r\n");
    Error_Handler();
  }

  log_info("Virtual UART1 OpenAMP-rpmsg channel creation\r\n");
  if (VIRT_UART_Init(&huart1) != VIRT_UART_OK) {
    log_err("VIRT_UART_Init UART1 failed.\r\n");
    Error_Handler();
  }

  /*Need to register callback for message reception by channels*/
  if(VIRT_UART_RegisterCallback(&huart0, VIRT_UART_RXCPLT_CB_ID, VIRT_UART0_RxCpltCallback) != VIRT_UART_OK)
  {
   Error_Handler();
  }
  if(VIRT_UART_RegisterCallback(&huart1, VIRT_UART_RXCPLT_CB_ID, VIRT_UART1_RxCpltCallback) != VIRT_UART_OK)
  {
    Error_Handler();
  }

  log_info("HC-SR04 distance sensor ready. Sending data on UART0.\r\n");

  while (1){
    OPENAMP_check_for_message();

    /* USER CODE END WHILE */

    /* Handle commands from Linux side (UART0) */
    if (VirtUart0RxMsg) {
      VirtUart0RxMsg = RESET;


      if (strncmp((char *)VirtUart0ChannelBuffRx, "stop", 4) == 0) {
        sensor_active = 0;
        VIRT_UART_Transmit(&huart0, (uint8_t *)"STOPPED\n", 8);
      }
      else if (strncmp((char *)VirtUart0ChannelBuffRx, "start", 5) == 0) {
        sensor_active = 1;
        VIRT_UART_Transmit(&huart0, (uint8_t *)"STARTED\n", 8);
      }
      else {
        //Echo back unknown commands
        VIRT_UART_Transmit(&huart0, VirtUart0ChannelBuffRx, VirtUart0ChannelRxSize);
      }
    }

    // Echo back anything on UART1 (keep for debugging)
    if (VirtUart1RxMsg) {
      VirtUart1RxMsg = RESET;
      VIRT_UART_Transmit(&huart1, VirtUart1ChannelBuffRx, VirtUart1ChannelRxSize);
    }

    // Read sensor at fixed interval and send distance over UART0
    if (sensor_active && (HAL_GetTick() - last_read_tick >= SENSOR_READ_INTERVAL_MS))
    {
      last_read_tick = HAL_GetTick();

      float distance = HCSR04_ReadDistance();

      if (distance >= 0.0f && distance < 400.0f) {
        /* Valid reading: format as "DIST:123.45\n" */
        int whole = (int)distance;
        int frac  = (int)((distance - whole) * 100);
        int len = snprintf(msg_buffer, sizeof(msg_buffer),
                           "DIST:%d.%02d\n", whole, frac);
        VIRT_UART_Transmit(&huart0, (uint8_t *)msg_buffer, len);
        last_distance_cm = distance;
      }
      else {
        /* Timeout or out of range */
        int len = snprintf(msg_buffer, sizeof(msg_buffer), "DIST:ERR\n");
        VIRT_UART_Transmit(&huart0, (uint8_t *)msg_buffer, len);
      }

      BSP_LED_Toggle(LED7);
    }


  }

}


static void MX_GPIO_Init_Sensor(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};

  /* Enable GPIO clocks for ports D and E */
  __HAL_RCC_GPIOD_CLK_ENABLE();
  __HAL_RCC_GPIOE_CLK_ENABLE();

  /* Configure Trig pin (PD1) as output push-pull, no pull, low speed */
  GPIO_InitStruct.Pin   = TRIG_PIN;
  GPIO_InitStruct.Mode  = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull  = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(TRIG_PORT, &GPIO_InitStruct);

  /* Start with Trig LOW */
  HAL_GPIO_WritePin(TRIG_PORT, TRIG_PIN, GPIO_PIN_RESET);

  /* Configure Echo pin (PE14) as input, no pull */
  GPIO_InitStruct.Pin   = ECHO_PIN;
  GPIO_InitStruct.Mode  = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull  = GPIO_NOPULL;
  HAL_GPIO_Init(ECHO_PORT, &GPIO_InitStruct);
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /**Configure LSE Drive Capability
  */
  HAL_PWR_EnableBkUpAccess();
  __HAL_RCC_LSEDRIVE_CONFIG(RCC_LSEDRIVE_MEDIUMHIGH);

  /**Initializes the CPU, AHB and APB busses clocks
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI|RCC_OSCILLATORTYPE_HSE
                |RCC_OSCILLATORTYPE_LSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_BYPASS_DIG;
  RCC_OscInitStruct.LSEState = RCC_LSE_ON;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = 16;
  RCC_OscInitStruct.HSIDivValue = RCC_HSI_DIV1;

  /**PLL1 Config
  */
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLL12SOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLM = 3;
  RCC_OscInitStruct.PLL.PLLN = 81;
  RCC_OscInitStruct.PLL.PLLP = 1;
  RCC_OscInitStruct.PLL.PLLQ = 1;
  RCC_OscInitStruct.PLL.PLLR = 1;
  RCC_OscInitStruct.PLL.PLLFRACV = 0x800;
  RCC_OscInitStruct.PLL.PLLMODE = RCC_PLL_FRACTIONAL;
  RCC_OscInitStruct.PLL.RPDFN_DIS = RCC_RPDFN_DIS_DISABLED;
  RCC_OscInitStruct.PLL.TPDFN_DIS = RCC_TPDFN_DIS_DISABLED;

  /**PLL2 Config
  */
  RCC_OscInitStruct.PLL2.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL2.PLLSource = RCC_PLL12SOURCE_HSE;
  RCC_OscInitStruct.PLL2.PLLM = 3;
  RCC_OscInitStruct.PLL2.PLLN = 66;
  RCC_OscInitStruct.PLL2.PLLP = 2;
  RCC_OscInitStruct.PLL2.PLLQ = 1;
  RCC_OscInitStruct.PLL2.PLLR = 1;
  RCC_OscInitStruct.PLL2.PLLFRACV = 0x1400;
  RCC_OscInitStruct.PLL2.PLLMODE = RCC_PLL_FRACTIONAL;
  RCC_OscInitStruct.PLL2.RPDFN_DIS = RCC_RPDFN_DIS_DISABLED;
  RCC_OscInitStruct.PLL2.TPDFN_DIS = RCC_TPDFN_DIS_DISABLED;

  /**PLL3 Config
  */
  RCC_OscInitStruct.PLL3.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL3.PLLSource = RCC_PLL3SOURCE_HSE;
  RCC_OscInitStruct.PLL3.PLLM = 2;
  RCC_OscInitStruct.PLL3.PLLN = 34;
  RCC_OscInitStruct.PLL3.PLLP = 2;
  RCC_OscInitStruct.PLL3.PLLQ = 17;
  RCC_OscInitStruct.PLL3.PLLR = 37;
  RCC_OscInitStruct.PLL3.PLLRGE = RCC_PLL3IFRANGE_1;
  RCC_OscInitStruct.PLL3.PLLFRACV = 0x1A04;
  RCC_OscInitStruct.PLL3.PLLMODE = RCC_PLL_FRACTIONAL;
  RCC_OscInitStruct.PLL3.RPDFN_DIS = RCC_RPDFN_DIS_DISABLED;
  RCC_OscInitStruct.PLL3.TPDFN_DIS = RCC_TPDFN_DIS_DISABLED;

  /**PLL4 Config
  */
  RCC_OscInitStruct.PLL4.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL4.PLLSource = RCC_PLL4SOURCE_HSE;
  RCC_OscInitStruct.PLL4.PLLM = 4;
  RCC_OscInitStruct.PLL4.PLLN = 99;
  RCC_OscInitStruct.PLL4.PLLP = 6;
  RCC_OscInitStruct.PLL4.PLLQ = 8;
  RCC_OscInitStruct.PLL4.PLLR = 8;
  RCC_OscInitStruct.PLL4.PLLRGE = RCC_PLL4IFRANGE_0;
  RCC_OscInitStruct.PLL4.PLLFRACV = 0;
  RCC_OscInitStruct.PLL4.PLLMODE = RCC_PLL_INTEGER;
  RCC_OscInitStruct.PLL4.RPDFN_DIS = RCC_RPDFN_DIS_DISABLED;
  RCC_OscInitStruct.PLL4.TPDFN_DIS = RCC_TPDFN_DIS_DISABLED;

  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }
  /**RCC Clock Config
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_ACLK
                |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2
                |RCC_CLOCKTYPE_PCLK3|RCC_CLOCKTYPE_PCLK4
                |RCC_CLOCKTYPE_PCLK5|RCC_CLOCKTYPE_MPU;
  RCC_ClkInitStruct.MPUInit.MPU_Clock = RCC_MPUSOURCE_PLL1;
  RCC_ClkInitStruct.MPUInit.MPU_Div = RCC_MPU_DIV2;
  RCC_ClkInitStruct.AXISSInit.AXI_Clock = RCC_AXISSOURCE_PLL2;
  RCC_ClkInitStruct.AXISSInit.AXI_Div = RCC_AXI_DIV1;
  RCC_ClkInitStruct.MCUInit.MCU_Clock = RCC_MCUSSOURCE_PLL3;
  RCC_ClkInitStruct.MCUInit.MCU_Div = RCC_MCU_DIV1;
  RCC_ClkInitStruct.APB4_Div = RCC_APB4_DIV2;
  RCC_ClkInitStruct.APB5_Div = RCC_APB5_DIV4;
  RCC_ClkInitStruct.APB1_Div = RCC_APB1_DIV2;
  RCC_ClkInitStruct.APB2_Div = RCC_APB2_DIV2;
  RCC_ClkInitStruct.APB3_Div = RCC_APB3_DIV2;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /**Set the HSE division factor for RTC clock
  */
  __HAL_RCC_RTC_HSEDIV(24);
}

/**
  * @brief IPPC Initialization Function
  * @param None
  * @retval None
  */
static void MX_IPCC_Init(void)
{
  hipcc.Instance = IPCC;
  if (HAL_IPCC_Init(&hipcc) != HAL_OK)
  {
     Error_Handler();
  }
}

/* USER CODE BEGIN 4 */
void VIRT_UART0_RxCpltCallback(VIRT_UART_HandleTypeDef *huart)
{
    log_info("Msg received on VIRTUAL UART0 channel:  %s \n\r", (char *) huart->pRxBuffPtr);

    /* copy received msg in a variable to sent it back to master processor in main infinite loop*/
    VirtUart0ChannelRxSize = huart->RxXferSize < MAX_BUFFER_SIZE? huart->RxXferSize : MAX_BUFFER_SIZE-1;
    memcpy(VirtUart0ChannelBuffRx, huart->pRxBuffPtr, VirtUart0ChannelRxSize);
    VirtUart0RxMsg = SET;
}

void VIRT_UART1_RxCpltCallback(VIRT_UART_HandleTypeDef *huart)
{
    log_info("Msg received on VIRTUAL UART1 channel:  %s \n\r", (char *) huart->pRxBuffPtr);

    /* copy received msg in a variable to sent it back to master processor in main infinite loop*/
    VirtUart1ChannelRxSize = huart->RxXferSize < MAX_BUFFER_SIZE? huart->RxXferSize : MAX_BUFFER_SIZE-1;
    memcpy(VirtUart1ChannelBuffRx, huart->pRxBuffPtr, VirtUart1ChannelRxSize);
    VirtUart1RxMsg = SET;
}
/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  log_err("Error_Handler");
  BSP_LED_Off(LED7);
  while(1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}

#ifdef  USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t* file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  log_err("OOOps: file %s, line %d\r\n", __FILE__, __LINE__);
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
