#include <M5GFX.h>
#include <driver/gpio.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <freertos/task.h>

/** Global display instance. */
M5GFX display;

/**
 * GPIO 23 is used by M5GFX at boot as an I2C address-select output (HIGH = 0x14).
 * After display.init() completes the ST7123 is fully initialised at that address,
 * so the pin can safely be reconfigured as an interrupt input.  The ST7123 drives
 * it LOW when touch co-ordinate data is available.
 *
 * NOTE: This behaviour should be verified against the specific ST7123 firmware
 * version on your hardware.  If the pin never asserts low, the semaphore will not
 * be signalled and touch will not be reported.  In that case, remove the interrupt
 * approach and revert to polling via getTouchRaw().
 */
static constexpr gpio_num_t TOUCH_INTERRUPT_PIN = GPIO_NUM_23;

/** Stack depth in words for the touch handler task. */
static constexpr uint32_t TOUCH_TASK_STACK_SIZE = 4096;

/** FreeRTOS priority for the touch handler task. */
static constexpr UBaseType_t TOUCH_TASK_PRIORITY = 5;

/** Binary semaphore signalled from the GPIO ISR when a touch interrupt fires. */
static SemaphoreHandle_t _touchSemaphore = nullptr;

/**
 * @brief GPIO ISR handler for the ST7123 touch interrupt pin.
 *
 * Runs in interrupt context on the falling edge of TOUCH_INTERRUPT_PIN.
 * Gives the binary semaphore to unblock TouchTask.
 *
 * @param arg Unused context pointer.
 */
static void IRAM_ATTR TouchInterruptHandler(void* arg)
{
    BaseType_t higherPriorityTaskWoken = pdFALSE;
    xSemaphoreGiveFromISR(_touchSemaphore, &higherPriorityTaskWoken);
    portYIELD_FROM_ISR(higherPriorityTaskWoken);
}

/**
 * @brief FreeRTOS task that processes touch events.
 *
 * Blocks indefinitely on _touchSemaphore.  When unblocked by TouchInterruptHandler,
 * reads raw touch co-ordinates from the ST7123 via getTouchRaw(), converts them to
 * screen co-ordinates and renders visual feedback on the display.
 *
 * All display access is contained within this task so no mutex is required.
 *
 * @param parameter Unused task parameter.
 */
static void TouchTask(void* parameter)
{
    static bool drawn = false;
    lgfx::touch_point_t tp[3];

    while (true)
    {
        if (xSemaphoreTake(_touchSemaphore, portMAX_DELAY) == pdTRUE)
        {
            int nums = display.getTouchRaw(tp, 3);
            if (nums)
            {
                display.startWrite();

                for (int i = 0; i < nums; ++i)
                {
                    display.setCursor(16, 16 + i * 24);
                    display.printf("Raw X:%04d  Y:%04d    ", tp[i].x, tp[i].y);
                }

                display.convertRawXY(tp, nums);

                for (int i = 0; i < nums; ++i)
                {
                    display.setCursor(16, 128 + i * 24);
                    display.printf("Convert X:%04d  Y:%04d    ", tp[i].x, tp[i].y);
                }
                display.display();

                display.setColor(display.isEPD() ? TFT_BLACK : TFT_WHITE);
                for (int i = 0; i < nums; ++i)
                {
                    int s = tp[i].size + 3;
                    switch (tp[i].id)
                    {
                        case 0:
                            display.fillCircle(tp[i].x, tp[i].y, s);
                            break;
                        case 1:
                            display.drawLine(tp[i].x - s, tp[i].y - s, tp[i].x + s, tp[i].y + s);
                            display.drawLine(tp[i].x - s, tp[i].y + s, tp[i].x + s, tp[i].y - s);
                            break;
                        default:
                            display.fillTriangle(tp[i].x - s, tp[i].y + s, tp[i].x + s, tp[i].y + s, tp[i].x, tp[i].y - s);
                            break;
                    }
                    display.display();
                }

                display.endWrite();
                drawn = true;
            }
            else if (drawn)
            {
                drawn = false;
                display.startWrite();
                display.waitDisplay();
                display.clear();
                display.display();
                display.endWrite();
            }
        }
    }
}

/**
 * @brief Configures the touch interrupt GPIO and starts the touch handler task.
 *
 * Must be called after display.init() so that the ST7123 is fully initialised
 * and GPIO_NUM_23 can be safely reconfigured from address-select output to
 * falling-edge interrupt input.
 */
static void InitialiseTouchInterrupt(void)
{
    _touchSemaphore = xSemaphoreCreateBinary();

    gpio_config_t ioConfiguration = {};
    ioConfiguration.pin_bit_mask = (1ULL << TOUCH_INTERRUPT_PIN);
    ioConfiguration.mode         = GPIO_MODE_INPUT;
    ioConfiguration.pull_up_en   = GPIO_PULLUP_ENABLE;
    ioConfiguration.pull_down_en = GPIO_PULLDOWN_DISABLE;
    ioConfiguration.intr_type    = GPIO_INTR_NEGEDGE;
    gpio_config(&ioConfiguration);

    gpio_install_isr_service(0);
    gpio_isr_handler_add(TOUCH_INTERRUPT_PIN, TouchInterruptHandler, nullptr);

    xTaskCreate(TouchTask, "TouchTask", TOUCH_TASK_STACK_SIZE, nullptr, TOUCH_TASK_PRIORITY, nullptr);
}

/**
 * @brief One-time application setup.
 *
 * Initialises the display, renders a splash screen, and starts the
 * interrupt-driven touch handler.
 */
void Setup(void)
{
    display.init();
    display.setBrightness(128); // AXP2101 backlight — 0 by default, must be set explicitly
    display.setFont(&fonts::Font4);
    // Tab5 native panel is portrait (720×1280); rotation 3 = landscape rotated 180 degrees (1280×720)
    display.setRotation(3);

    // Confirm display pipeline with a visible splash
    display.fillScreen(TFT_WHITE);
    display.setTextColor(TFT_BLACK, TFT_WHITE);
    display.setTextDatum(textdatum_t::middle_center);
    display.drawString("Tab5 Ready", display.width() / 2, display.height() / 2 - 24);
    if (!display.touch())
    {
        display.drawString("Touch not found.", display.width() / 2, display.height() / 2 + 24);
    }
    display.display();
    display.setTextColor(TFT_WHITE, TFT_BLACK);

    // display.init() configured GPIO_NUM_23 as output-high to select the ST7123
    // I2C address.  Now that init is complete, reconfigure it as an interrupt input.
    InitialiseTouchInterrupt();
}

extern "C" void app_main(void)
{
    Setup();
    while (1)
    {
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}
