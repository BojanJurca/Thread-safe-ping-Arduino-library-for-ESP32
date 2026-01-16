/*

  LwIpMutex.h

  This file is part of Multitasking HTTP, FTP, Telnet, NTP, SMTP servers and clients for ESP32 - Arduino library: https://github.com/BojanJurca/Multitasking-Http-Ftp-Telnet-Ntp-Smtp-Servers-and-clients-for-ESP32-Arduino-Library

  January 1, 2026, Bojan Jurca

*/


#pragma once
#ifndef __LWIP_MUTEX__
  #define __LWIP_MUTEX__

  #include <WiFi.h>

  // singleton mutex definition
  inline SemaphoreHandle_t getLwIpMutex () {
      static SemaphoreHandle_t semaphore = xSemaphoreCreateMutex ();
      return semaphore;
  }

#endif
