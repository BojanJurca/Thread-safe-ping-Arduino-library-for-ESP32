/*

  gai_strerror.h

  This file is part of Multitasking HTTP, FTP, Telnet, NTP, SMTP servers and clients for ESP32 - Arduino library: https://github.com/BojanJurca/Multitasking-Http-Ftp-Telnet-Ntp-Smtp-Servers-and-clients-for-ESP32-Arduino-Library


  Missing LwIP function

  February 6, 2026, Bojan Jurca

*/


#pragma once
#ifndef __GAI_STRERROR__
  #define __GAI_STRERROR__


  #include <WiFi.h>
  #include <lwip/netdb.h>


  static inline const char *gai_strerror (int err) {
      switch (err) {
          case EAI_AGAIN:     return "temporary failure in name resolution";
          case EAI_BADFLAGS:  return "invalid value for ai_flags field";
          case EAI_FAIL:      return "non-recoverable failure in name resolution";
          case EAI_FAMILY:    return "ai_family not supported";
          case EAI_MEMORY:    return "memory allocation failure";
          case EAI_NONAME:    return "name or service not known";
          case EAI_SERVICE:   return "service not supported for ai_socktype";
          case EAI_SOCKTYPE:  return "ai_socktype not supported";
          default:            return "invalid gai_errno code";
      }
  }

#endif