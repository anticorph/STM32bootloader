#ifndef APP_COMMIT_H
#define APP_COMMIT_H

/* Call this once from application, after it has completed enough
 * self-initialization / self-test to be confident it is healthy
 * (peripherals came up, sensors respond, whatever makes sense for product).
 *
 * Until this is called, the bootloader treats the current boot as
 * "on probation": if the device resets for any reason (crash, hard
 * fault, independent watchdog timeout, brown-out) before boot_commit()
 * runs, the bootloader will roll back to the previous known-good
 * firmware image on its next boot instead of retrying the broken one
 * forever.
 *
 * Typical usage, near the top of the application's main():
 *
 *     int main(void)
 *     {
 *         HAL_Init();
 *         SystemClock_Config();
 *         MX_GPIO_Init();
 *         // ... other init / self-checks ...
 *         boot_commit();          // "I'm alive and well"
 *         IWDG_Init();            // start feeding the watchdog only now
 *         for (;;) { ... }
 *     }
 */
void boot_commit(void);

#endif // APP_COMMIT_H
