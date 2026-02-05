/*
    ThreadSafePing.h

    This file is part of the ThreadSafe ESP32 Ping class: https://github.com/BojanJurca/Thread-safe-ping-Arduino-library-for-ESP32

    The library is based on the work of Jaume Olivé: https://github.com/pbecchi/ESP32_ping
    and D. Varrel, whose implementation is included in the Arduino Library Manager: https://github.com/dvarrel

    These libraries lack some features:

      - true ThreadSafe support
      - intermediate result reporting
      - IPv6 support (experimental; full IPv6 functionality on Arduino is still limited)

    This library was created to address these limitations and provide a more robust, 
    task‑safe ping implementation for ESP32‑based ThreadSafe environments.

    January 1, 2026, Bojan Jurca

*/


#ifndef __ThreadSafePing_H__
    #define __ThreadSafePing_H__


    #include <WiFi.h>
    #include <lwip/netdb.h>
    #include <lwip/inet_chksum.h>
    #include <lwip/ip.h>
    #include <lwip/icmp.h>
    #include <LwIpMutex.h>


    #ifndef ICMP6_TYPES_H
        #define ICMP6_ECHO_REQUEST 128
        #define ICMP6_ECHO_REPLY   129
    #endif

    #define EAGAIN 11
    #define ENAVAIL 119


    // missing function in LwIP
    inline const char *gai_strerror (int err);
 
 
    #ifndef PING_DEFAULT_COUNT
        #define PING_DEFAULT_COUNT     10
    #endif
    #ifndef PING_DEFAULT_INTERVAL
        #define PING_DEFAULT_INTERVAL  1
    #endif
    #ifndef PING_DEFAULT_SIZE
        #define PING_DEFAULT_SIZE     32
    #endif
    #ifndef PING_DEFAULT_TIMEOUT
        #define PING_DEFAULT_TIMEOUT   1
    #endif


    class ThreadSafePing_t {

        private:
            bool __isIPv6__ = false;
            char __pingTargetIp__ [INET6_ADDRSTRLEN] = "";

            struct sockaddr_in  __target_addr_IPv4__ = {};
            struct sockaddr_in6 __target_addr_IPv6__ = {};

            const char *__errText__ = nullptr;

            int __size__;
            uint32_t __sent__;
            uint32_t __received__;
            uint32_t __lost__;
            bool __stopped__;

            float __elapsed_time__;
            float __min_time__;
            float __max_time__;
            float __mean_time__;
            float __var_time__;
            float __last_mean_time__;

            struct __pingReply_t__ {
                uint16_t seqno;
                unsigned long elapsed_time;
            };

            static __pingReply_t__ __pingReplies__ [MEMP_NUM_NETCONN];

            const char *__resolveTargetName__ (const char *pingTarget);
            const char *__ping_send__ (int sockfd, uint16_t seqno, int size);
            const char *__ping_recv__ (int sockfd, int *bytes, unsigned long timeoutMicros);

            err_t __errno__ = ERR_OK;

        public:
            ThreadSafePing_t () {}
            ThreadSafePing_t (const char *pingTarget);
            ThreadSafePing_t (const IPAddress& pingTarget);

            const char *ping (const char *pingTarget, int count = PING_DEFAULT_COUNT,
                              int interval = PING_DEFAULT_INTERVAL,
                              int size = PING_DEFAULT_SIZE,
                              int timeout = PING_DEFAULT_TIMEOUT);

            const char *ping (const IPAddress& pingTarget, int count = PING_DEFAULT_COUNT,
                              int interval = PING_DEFAULT_INTERVAL,
                              int size = PING_DEFAULT_SIZE,
                              int timeout = PING_DEFAULT_TIMEOUT);

            // if the target is set by constructor
            const char *ping (int count = PING_DEFAULT_COUNT,
                              int interval = PING_DEFAULT_INTERVAL,
                              int size = PING_DEFAULT_SIZE,
                              int timeout = PING_DEFAULT_TIMEOUT);

            inline char *target () { return __pingTargetIp__; }
            inline int size () { return __size__; }
            inline void stop () { __stopped__ = true; }

            inline uint32_t sent () { return __sent__; }
            inline uint32_t received () { return __received__; }
            inline uint32_t lost () { return __lost__; }
            inline float elapsed_time () { return __elapsed_time__; }
            inline float min_time () { return __min_time__; }
            inline float max_time () { return __max_time__; }
            inline float mean_time () { return __mean_time__; }
            inline float var_time () { return __var_time__; }

            inline const char *errText () { return __errText__; }

            virtual void onReceive (int bytes) {}
            virtual void onWait () {}
    };

#endif
