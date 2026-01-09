


#ifndef __LWIP_MUTEX__
  #define __LWIP_MUTEX__

  #include <WiFi.h>

  // singleton mutex definition
  inline SemaphoreHandle_t getLwIpMutex () {
      static SemaphoreHandle_t semaphore = xSemaphoreCreateMutex ();
      return semaphore;
  }
#endif
