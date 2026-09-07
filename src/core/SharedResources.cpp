// Shared RTOS handles and module readiness flags; devices own their readiness flags.
#include "SharedResources.hpp"

// Storage and display synchronization plus application task/queue handles.
SemaphoreHandle_t sdSemaphore = nullptr;
SemaphoreHandle_t displaySemaphore = NULL;
TaskHandle_t t_handleAudio = NULL;
TaskHandle_t t_handleDisplay = NULL;
TaskHandle_t runnet = NULL;
QueueHandle_t display_command = NULL;
QueueHandle_t audio_command = NULL;
QueueHandle_t api_event_queue = NULL;
