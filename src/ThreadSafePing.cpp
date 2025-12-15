/*
    ThreadSafePing.cpp

    This file is part of the ThreadSafe ESP32 Ping class: https://github.com/BojanJurca/ThreadSafe-Esp32-ping-class

    The library is based on the work of Jaume Olivé: https://github.com/pbecchi/ESP32_ping
    and D. Varrel, whose implementation is included in the Arduino Library Manager: https://github.com/dvarrel

    These libraries lack some features:

      - true ThreadSafe support
      - intermediate result reporting
      - IPv6 support (experimental; full IPv6 functionality on Arduino is still limited)

    This library was created to address these limitations and provide a more robust, 
    task‑safe ping implementation for ESP32‑based ThreadSafe environments.

    December 25, 2025, Bojan Jurca

*/


#include "ThreadSafePing.h"
#include <errno.h>
#include <fcntl.h>


// missing function
static const char *gai_strerror (int err) {
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


ThreadSafePing::__pingReply_t__ ThreadSafePing::__pingReplies__ [MEMP_NUM_NETCONN];

ThreadSafePing::ThreadSafePing (const char *pingTarget) {
    __errText__ = __resolveTargetName__ (pingTarget);
}

ThreadSafePing::ThreadSafePing (const IPAddress& pingTarget) {
    snprintf (__pingTargetIp__, sizeof (__pingTargetIp__), "%u.%u.%u.%u", pingTarget [0], pingTarget [1], pingTarget [2], pingTarget [3]);
    __errText__ = __resolveTargetName__ (__pingTargetIp__);
}

// returns error text or NULL if OK
const char *ThreadSafePing::__resolveTargetName__ (const char *pingTarget) {
    if (!WiFi.isConnected () || WiFi.localIP () == IPAddress (0, 0, 0, 0))
        return "not connected";

    struct addrinfo hints, *res, *p;
    memset (&hints, 0, sizeof (hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_DGRAM;

    int e = getaddrinfo (pingTarget, NULL, &hints, &res);
    if (e)
        return gai_strerror (e);

    for (p = res; p != NULL; p = p->ai_next) {
        void *addr;
        if (p->ai_family == AF_INET) {
            __isIPv6__ = false;
            struct sockaddr_in *ipv4 = (struct sockaddr_in*) p->ai_addr;
            addr = &(ipv4->sin_addr);
        } else {
            __isIPv6__ = true;
            struct sockaddr_in6 *ipv6 = (struct sockaddr_in6 *) p->ai_addr;
            addr = &(ipv6->sin6_addr);
        }
        inet_ntop (p->ai_family, addr, __pingTargetIp__, sizeof (__pingTargetIp__));
        break;
    }
    freeaddrinfo (res);

    if (__isIPv6__) {
        __target_addr_IPv6__ = {};
        __target_addr_IPv6__.sin6_family = AF_INET6;
        __target_addr_IPv6__.sin6_len = sizeof (__target_addr_IPv6__);
        if (inet_pton (AF_INET6, __pingTargetIp__, &__target_addr_IPv6__.sin6_addr) <= 0)
            return "invalid network address";
    } else {
        __target_addr_IPv4__ = {};
        __target_addr_IPv4__.sin_family = AF_INET;
        __target_addr_IPv4__.sin_len = sizeof (__target_addr_IPv4__);
        if (inet_pton (AF_INET, __pingTargetIp__, &__target_addr_IPv4__.sin_addr) <= 0)
            return "invalid network address";
    }

    return NULL; // OK
}

const char *ThreadSafePing::ping (const char *pingTarget, int count, int interval, int size, int timeout) {
    __errText__ = __resolveTargetName__ (pingTarget);
    if (__errText__)
        return __errText__;
    return ping (count, interval, size, timeout);
}

const char *ThreadSafePing::ping (const IPAddress& pingTarget, int count, int interval, int size, int timeout) {
    snprintf (__pingTargetIp__, sizeof (__pingTargetIp__), "%u.%u.%u.%u", pingTarget [0], pingTarget [1], pingTarget [2], pingTarget [3]);
    __errText__ = __resolveTargetName__ (__pingTargetIp__);
    if (__errText__)
        return __errText__;
    return ping (count, interval, size, timeout);
}

const char *ThreadSafePing::ping (int count, int interval, int size, int timeout) {
    if (!WiFi.isConnected () || WiFi.localIP () == IPAddress (0, 0, 0, 0))
        return "not connected";

    if (count < 0) return "invalid value";
    if (interval < 1 || interval > 3600) return "invalid value";
    if (size < 4 || size > 256) return "invalid value";
    if (timeout < 1 || timeout > 30) return "invalid value";

    __size__ = size;
    __sent__ = __received__ = __lost__ = 0;
    __stopped__ = false;
    __min_time__ = 1e9;
    __max_time__ = 0;
    __mean_time__ = 0;
    __var_time__ = 0;

    int sockfd;
    if (__isIPv6__) {
        if ((sockfd = socket (AF_INET6, SOCK_RAW, IPPROTO_ICMPV6)) < 0)
            return strerror (errno);
    } else {
        if ((sockfd = socket (AF_INET, SOCK_RAW, IPPROTO_ICMP)) < 0)
            return strerror (errno);
    }

    if (fcntl (sockfd, F_SETFL, O_NONBLOCK) == -1) {
        __errText__ = strerror (errno);
        close (sockfd);
        return __errText__;
    }

    for (uint16_t seqno = 1; (seqno <= count || count == 0) && !__stopped__; seqno++) {
        unsigned long sendMillis = millis ();

        __errText__ = __ping_send__ (sockfd, seqno, size);
        if (__errText__)
            return __errText__;

        __sent__++;

        int bytesReceived;
        __ping_recv__ (sockfd, &bytesReceived, 1000000 * timeout);

        if (__pingReplies__ [sockfd - LWIP_SOCKET_OFFSET].elapsed_time) {
            __received__++;
            __elapsed_time__ = (float) __pingReplies__ [sockfd - LWIP_SOCKET_OFFSET].elapsed_time / 1000.0f;

            if (__elapsed_time__ < __min_time__) __min_time__ = __elapsed_time__;
            if (__elapsed_time__ > __max_time__) __max_time__ = __elapsed_time__;

            __last_mean_time__ = __mean_time__;
            __mean_time__ = (((__received__ - 1) * __mean_time__) + __elapsed_time__) / __received__;

            if (__received__ > 1)
                __var_time__ += (__elapsed_time__ - __last_mean_time__) * (__elapsed_time__ - __mean_time__);

        } else {
            __lost__++;
            __elapsed_time__ = 0;
        }

        onReceive (bytesReceived);

        if (seqno <= count || count == 0) {
            while ((millis () - sendMillis < 1000L * interval) && !__stopped__) {
                onWait ();
                delay (10);
            }
        }
    }

    closesocket (sockfd);
    return NULL;
}

const char *ThreadSafePing::__ping_send__ (int sockfd, uint16_t seqno, int size) {
    int sent;
    int ping_size;

    if (__isIPv6__) {
        struct icmp6_echo_hdr *iecho;
        ping_size = sizeof (struct icmp6_echo_hdr) + size;

        iecho = (struct icmp6_echo_hdr *) mem_malloc (ping_size);
        if (!iecho)
            return "out of memory";

        __pingReplies__ [sockfd - LWIP_SOCKET_OFFSET] = { seqno, 0 };

        size_t data_len = ping_size - sizeof (struct icmp6_echo_hdr);
        iecho->type = ICMP6_ECHO_REQUEST;
        iecho->code = 0;
        iecho->chksum = 0;
        iecho->id = sockfd;
        iecho->seqno = seqno;

        unsigned long sendMicros = micros ();
        *(unsigned long *) (((char *)iecho) + sizeof (struct icmp6_echo_hdr)) = sendMicros;

        for (int i = sizeof (sendMicros); i < data_len; i++)
            ((char *)iecho) [sizeof (struct icmp6_echo_hdr) + i] = (char) i;

        iecho->chksum = inet_chksum (iecho, ping_size);

        sent = sendto (sockfd, iecho, ping_size, 0,
                      (struct sockaddr *)&__target_addr_IPv6__,
                      sizeof (__target_addr_IPv6__));

        mem_free (iecho);

    } else {
        struct icmp_echo_hdr *iecho;
        ping_size = sizeof (struct icmp_echo_hdr) + size;

        iecho = (struct icmp_echo_hdr *) mem_malloc (ping_size);
        if (!iecho)
            return "out of memory";

        __pingReplies__ [sockfd - LWIP_SOCKET_OFFSET] = { seqno, 0 };

        size_t data_len = ping_size - sizeof (struct icmp_echo_hdr);
        iecho->type = ICMP_ECHO;
        iecho->code = 0;
        iecho->chksum = 0;
        iecho->id = sockfd;
        iecho->seqno = seqno;

        unsigned long sendMicros = micros ();
        *(unsigned long *) (((char *) iecho) + sizeof (struct icmp_echo_hdr)) = sendMicros;

        for (int i = sizeof (sendMicros); i < data_len; i++)
            ((char *) iecho) [sizeof (struct icmp_echo_hdr) + i] = (char) i;

        iecho->chksum = inet_chksum (iecho, ping_size);

        sent = sendto (sockfd, iecho, ping_size, 0,
                      (struct sockaddr *) &__target_addr_IPv4__,
                      sizeof (__target_addr_IPv4__));

        mem_free (iecho);
    }

    if (sent != ping_size)
        return "couldn't sendto";

    return NULL;
}

const char *ThreadSafePing::__ping_recv__ (int sockfd, int *bytes, unsigned long timeoutMicros) {
    char buf [300];

    struct sockaddr_in  from_addr_IPv4;
    struct sockaddr_in6 from_addr_IPv6;
    int fromlen;

    unsigned long startMicros = micros ();

    while (true) {

        if (__pingReplies__ [sockfd - LWIP_SOCKET_OFFSET].elapsed_time)
            return NULL;

        if (__isIPv6__)
            *bytes = recvfrom (sockfd, buf, sizeof (buf), 0,
                              (struct sockaddr *) &from_addr_IPv6,
                              (socklen_t *) &fromlen);
        else
            *bytes = recvfrom (sockfd, buf, sizeof (buf), 0,
                              (struct sockaddr *) &from_addr_IPv4,
                              (socklen_t *) &fromlen);

        if (*bytes <= 0) {
            if ((errno == EAGAIN || errno == ENAVAIL) &&
                (micros () - startMicros < timeoutMicros)) {
                delay (1);
                continue;
            }
            return "timeout";
        }

        byte type;
        int id;
        uint16_t seqno;
        unsigned long sentMicros;

        if (__isIPv6__) {
            if (*bytes < (int) (40 + sizeof (struct icmp6_echo_hdr) + sizeof (unsigned long)))
                continue;

            struct icmp6_echo_hdr *iecho = (struct icmp6_echo_hdr *) (buf + 40);

            type = iecho->type;
            id = iecho->id;
            seqno = iecho->seqno;
            sentMicros = *(unsigned long *) (((char *) iecho) + sizeof (struct icmp6_echo_hdr));

            *bytes -= (40 + sizeof (struct icmp6_echo_hdr));

        } else {
            struct ip_hdr *iphdr = (struct ip_hdr*) buf;
            int iphdr_len = IPH_HL (iphdr) * 4;

            if (*bytes < (int) (iphdr_len + sizeof (struct icmp_echo_hdr) + sizeof (unsigned long)))
                continue;

            struct icmp_echo_hdr *iecho = (struct icmp_echo_hdr *) (buf + iphdr_len);

            type = iecho->type;
            id = iecho->id;
            seqno = iecho->seqno;
            sentMicros = *(unsigned long *) (((char *) iecho) + sizeof (struct icmp_echo_hdr));

            *bytes -= (iphdr_len + sizeof (struct icmp_echo_hdr));
        }

        if (id < LWIP_SOCKET_OFFSET || id >= LWIP_SOCKET_OFFSET + MEMP_NUM_NETCONN || !(type == ICMP_ER || type == ICMP6_ECHO_REPLY))
            continue;

        if (id == sockfd) {
            if (__pingReplies__ [sockfd - LWIP_SOCKET_OFFSET].seqno == seqno) {
                __pingReplies__ [sockfd - LWIP_SOCKET_OFFSET].elapsed_time = micros () - sentMicros;
                return NULL;
            }
        } else {
            if (__pingReplies__ [id - LWIP_SOCKET_OFFSET].seqno == seqno) {
                __pingReplies__ [id - LWIP_SOCKET_OFFSET].elapsed_time = micros () - sentMicros;
            }
        }
    }
}
