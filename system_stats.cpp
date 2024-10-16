#include "apptools/system_stats.h"
#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_idf_version.h"
#include <string.h>

#if ( configUSE_TRACE_FACILITY == 1 ) && ( configGENERATE_RUN_TIME_STATS == 1 )

cpu_usage_t get_cpu_load() {
    static unsigned long last_total_time[2] = {0, 0};
    static unsigned long last_idle_time[2] = {0, 0};

    TaskStatus_t *pxTaskStatusArray;
    volatile UBaseType_t uxArraySize, x;
    unsigned long ulTotalRunTime;

    cpu_usage_t usage = {0};

    // Take a snapshot of the number of tasks in case it changes while this
    // function is executing.
    uxArraySize = uxTaskGetNumberOfTasks();

    // Allocate a TaskStatus_t structure for each task.
    pxTaskStatusArray = (TaskStatus_t*) pvPortMalloc(uxArraySize * sizeof(TaskStatus_t));

    if (pxTaskStatusArray != NULL) {
        // Generate raw status information about each task.
        uxArraySize = uxTaskGetSystemState(pxTaskStatusArray, uxArraySize, &ulTotalRunTime);

        unsigned long idle_time[2] = {0, 0};
        unsigned long total_time[2] = {0, 0};

        // Calculate total time and idle time for each core
        for (x = 0; x < uxArraySize; x++) {
            int core;

            // Determine which core the task is running on
            #if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(4, 2, 0)
                core = xTaskGetAffinity(pxTaskStatusArray[x].xHandle) == 1 ? 1 : 0;
            #else
                // For older IDF versions, we'll assume tasks are evenly distributed
                core = x % 2;
            #endif

            total_time[core] += pxTaskStatusArray[x].ulRunTimeCounter;

            if (strcmp(pxTaskStatusArray[x].pcTaskName, "IDLE") == 0 ||
                strcmp(pxTaskStatusArray[x].pcTaskName, "IDLE0") == 0 ||
                strcmp(pxTaskStatusArray[x].pcTaskName, "IDLE1") == 0) {
                idle_time[core] += pxTaskStatusArray[x].ulRunTimeCounter;
            }
        }

        vPortFree(pxTaskStatusArray);

        for (int i = 0; i < 2; i++) {
            if (last_total_time[i] > 0) {
                unsigned long total_diff = total_time[i] - last_total_time[i];
                unsigned long idle_diff = idle_time[i] - last_idle_time[i];

                if (total_diff > 0) {
                    float core_usage = 100.0f * (1.0f - ((float)idle_diff / (float)total_diff));
                    if (i == 0) usage.core0 = core_usage;
                    else usage.core1 = core_usage;
                }
            }

            last_total_time[i] = total_time[i];
            last_idle_time[i] = idle_time[i];
        }

        usage.total = (usage.core0 + usage.core1) / 2.0f;
    }

    return usage;
}
#else
#warning "FreeRTOS trace facility or run time stats are not enabled. CPU load calculation will not be available."

/*
Component config -> FreeRTOS
Enable "Enable FreeRTOS trace facility"
Enable "Enable FreeRTOS stats formatting functions"
Enable "Generate run time stats"
*/

cpu_usage_t get_cpu_load() {
    return {-1.0f, -1.0f, -1.0f};  // Return sentinel values to indicate unavailability
}
#endif

