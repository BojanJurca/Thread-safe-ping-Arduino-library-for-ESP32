/*
    ThreadSafePing.cpp

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


#include "ThreadSafePing.h"
#include <errno.h>
#include <fcntl.h>


// missing function in LwIP
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


// internal data structure - one record per each available socket
ThreadSafePing_t::__pingReply_t__ ThreadSafePing_t::__pingReplies__ [MEMP_NUM_NETCONN] = {};

// constructor with specified target (the one without target specified is in ThreadSafePing_t.h)
ThreadSafePing_t::ThreadSafePing_t (const char *pingTarget) {
    __errText__ = __resolveTargetName__ (pingTarget);
}

// constructor with specified target (the one without target specified is in ThreadSafePing_t.h)
ThreadSafePing_t::ThreadSafePing_t (const IPAddress& pingTarget) {
    snprintf (__pingTargetIp__, sizeof (__pingTargetIp__), "%u.%u.%u.%u", pingTarget [0], pingTarget [1], pingTarget [2], pingTarget [3]);
    __errText__ = __resolveTargetName__ (__pingTargetIp__);
}

// returns error text or NULL if OK
const char *ThreadSafePing_t::__resolveTargetName__ (const char *pingTarget) {
    if (!WiFi.isConnected () || WiFi.localIP () == IPAddress (0, 0, 0, 0)) // esp32 can crash without this check
        return "not connected";

    struct addrinfo hints, *res, *p;
    memset (&hints, 0, sizeof (hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_DGRAM;

    xSemaphoreTake (getLwIpMutex (), portMAX_DELAY);
        int e = getaddrinfo (pingTarget, NULL, &hints, &res);
    xSemaphoreGive (getLwIpMutex ());
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
    xSemaphoreTake (getLwIpMutex (), portMAX_DELAY);
        freeaddrinfo (res);
    xSemaphoreGive (getLwIpMutex ());

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

// returns error text or NULL if OK
const char *ThreadSafePing_t::ping (const char *pingTarget, int count, int interval, int size, int timeout) {
    __errText__ = __resolveTargetName__ (pingTarget);
    if (__errText__)
        return __errText__;
    return ping (count, interval, size, timeout);
}

// returns error text or NULL if OK
const char *ThreadSafePing_t::ping (const IPAddress& pingTarget, int count, int interval, int size, int timeout) {
    snprintf (__pingTargetIp__, sizeof (__pingTargetIp__), "%u.%u.%u.%u", pingTarget [0], pingTarget [1], pingTarget [2], pingTarget [3]);
    __errText__ = __resolveTargetName__ (__pingTargetIp__);
    if (__errText__)
        return __errText__;
    return ping (count, interval, size, timeout);
}

// returns error text or NULL if OK
const char *ThreadSafePing_t::ping (int count, int interval, int size, int timeout) {
    if (!WiFi.isConnected () || WiFi.localIP () == IPAddress (0, 0, 0, 0))
        return "not connected";

    // check argument values
    if (count < 0) return "invalid value";
    if (interval < 1 || interval > 3600) return "invalid value";
    if (size < 4 || size > 256) return "invalid value";
    if (timeout < 1 || timeout > 30) return "invalid value";

    // initialize measuring variables
    __size__ = size;
    __sent__ = __received__ = __lost__ = 0;
    __stopped__ = false;
    __min_time__ = 1e9; // FLT_MAX;
    __max_time__ = 0;
    __mean_time__ = 0;
    __var_time__ = 0;

    // create socket
    xSemaphoreTake (getLwIpMutex (), portMAX_DELAY);
        int sockfd = __isIPv6__ ? socket (AF_INET6, SOCK_RAW, IPPROTO_ICMPV6) : socket (AF_INET, SOCK_RAW, IPPROTO_ICMP);
        
        if (sockfd < 0) {
            __errText__ = strerror (errno);
            xSemaphoreGive (getLwIpMutex ());
            return strerror (errno);
        }

        // make the socket non-blocking, so we can detect time-out later     
        if (fcntl (sockfd, F_SETFL, O_NONBLOCK) == -1) {
            __errText__ = strerror (errno);
            close (sockfd);
            xSemaphoreGive (getLwIpMutex ());
            return __errText__;
        }
    xSemaphoreGive (getLwIpMutex ());

    // begin ping ...
    for (uint16_t seqno = 1; (seqno <= count || count == 0) && !__stopped__; seqno++) {
        unsigned long sendMillis = millis ();

        __errText__ = __ping_send__ (sockfd, seqno, size);
        if (__errText__)
            return __errText__;

        __sent__++;

        int bytesReceived;
        __ping_recv__ (sockfd, &bytesReceived, 1000000 * timeout);

        if (__pingReplies__ [sockfd - LWIP_SOCKET_OFFSET].elapsed_time) {
            // Update statistics
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

        // report intermediate results 
        onReceive (bytesReceived);

        if (seqno <= count || count == 0) {
            while ((millis () - sendMillis < 1000L * interval) && !__stopped__) {
                // report waiting 
                onWait ();
                delay (10);
            }
        }
    }

    xSemaphoreTake (getLwIpMutex (), portMAX_DELAY);
        closesocket (sockfd);
    xSemaphoreGive (getLwIpMutex ());
    return NULL; // OK
}

// returns error text or NULL if OK
const char *ThreadSafePing_t::__ping_send__ (int sockfd, uint16_t seqno, int size) {
    int sent;
    int ping_size;

    if (__isIPv6__) {
        struct icmp6_echo_hdr *iecho;
        ping_size = sizeof (struct icmp6_echo_hdr) + size;

        // construct ping block
        // - first there is struct icmp_echo_hdr (https://github.com/ARMmbed/lwip/blob/master/src/include/lwip/prot/icmG.h). We'll use these fields:
        //    - uint16_t id      - this is where we'll keep the socket number so that we would know from where ping packet has been send when we receive a reply 
        //    - uint16_t seqno   - each packet gets it sequence number so we can distinguish one packet from another when we receive a reply
        //    - uint16_t chksum  - needs to be calcualted
        // - then we'll add the payload:
        //    - unsigned long micros  - this is where we'll keep the time packet has been sent so we can calcluate round-trip time when we receive a reply
        //    - unimportant data, just to fill the payload to the desired length

        iecho = (struct icmp6_echo_hdr *) mem_malloc (ping_size);
        if (!iecho)
            return "out of memory";

        // initialize the data structure where the reply information will be stored when it arrives
        __pingReplies__ [sockfd - LWIP_SOCKET_OFFSET] = { seqno, 0 };

        // prepare echo packet
        size_t data_len = ping_size - sizeof (struct icmp6_echo_hdr);
        iecho->type = ICMP6_ECHO_REQUEST;
        iecho->code = 0;
        iecho->chksum = 0;
        iecho->id = sockfd;
        iecho->seqno = seqno;

        // store micros at they are at send time
        unsigned long sendMicros = micros ();
        *(unsigned long *) (((char *) iecho) + sizeof (struct icmp6_echo_hdr)) = sendMicros;

        // fill the additional data buffer with some data
        for (int i = sizeof (sendMicros); i < data_len; i++)
            ((char *) iecho) [sizeof (struct icmp6_echo_hdr) + i] = (char) i;

        // claculate checksum
        iecho->chksum = inet_chksum (iecho, ping_size);

        // send the packet
        sent = sendto (sockfd, iecho, ping_size, 0, (struct sockaddr *) &__target_addr_IPv6__, sizeof (__target_addr_IPv6__));

        mem_free (iecho);

    } else {
        struct icmp_echo_hdr *iecho;
        ping_size = sizeof (struct icmp_echo_hdr) + size;

        // construct ping block
        // - first there is struct icmp_echo_hdr (https://github.com/ARMmbed/lwip/blob/master/src/include/lwip/prot/icmG.h). We'll use these fields:
        //    - uint16_t id      - this is where we'll keep the socket number so that we would know from where ping packet has been send when we receive a reply 
        //    - uint16_t seqno   - each packet gets it sequence number so we can distinguish one packet from another when we receive a reply
        //    - uint16_t chksum  - needs to be calcualted
        // - then we'll add the payload:
        //    - unsigned long micros  - this is where we'll keep the time packet has been sent so we can calcluate round-trip time when we receive a reply
        //    - unimportant data, just to fill the payload to the desired length

        iecho = (struct icmp_echo_hdr *) mem_malloc (ping_size);
        if (!iecho)
            return "out of memory";

        // initialize the structure where the reply information will be stored when it arrives
        __pingReplies__ [sockfd - LWIP_SOCKET_OFFSET] = { seqno, 0 };

        // prepare echo packet
        size_t data_len = ping_size - sizeof (struct icmp_echo_hdr);
        iecho->type = ICMP_ECHO;
        iecho->code = 0;
        iecho->chksum = 0;
        iecho->id = sockfd;
        iecho->seqno = seqno;

        // store micros as they are at send time
        unsigned long sendMicros = micros ();
        *(unsigned long *) (((char *) iecho) + sizeof (struct icmp_echo_hdr)) = sendMicros;

        // fill the additional data buffer with some data
        for (int i = sizeof (sendMicros); i < data_len; i++)
            ((char *) iecho) [sizeof (struct icmp_echo_hdr) + i] = (char) i;

        // claculate checksum
        iecho->chksum = inet_chksum (iecho, ping_size);

        // send the packet
        xSemaphoreTake (getLwIpMutex (), portMAX_DELAY);
            sent = sendto (sockfd, iecho, ping_size, 0, (struct sockaddr *) &__target_addr_IPv4__, sizeof (__target_addr_IPv4__));
        xSemaphoreGive (getLwIpMutex ());
        
        mem_free (iecho);
    }

    if (sent != ping_size)
        return "couldn't sendto";

    return NULL; // OK
}

// returns error text or NULL if OK
const char *ThreadSafePing_t::__ping_recv__ (int sockfd, int *bytes, unsigned long timeoutMicros) {
    char buf [300];

    struct sockaddr_in  from_addr_IPv4;
    struct sockaddr_in6 from_addr_IPv6;
    int fromlen;

    unsigned long startMicros = micros ();

    // receive the echo packet
    while (true) {

        // did some other process poick up our echo reply and already done the job for us? 
        if (__pingReplies__ [sockfd - LWIP_SOCKET_OFFSET].elapsed_time)
            return NULL; // OK

        // read echo packet without waiting
        xSemaphoreTake (getLwIpMutex (), portMAX_DELAY);
             *bytes = __isIPv6__ ? recvfrom (sockfd, buf, sizeof (buf), 0, (struct sockaddr *) &from_addr_IPv6, (socklen_t *) &fromlen) : recvfrom (sockfd, buf, sizeof (buf), 0, (struct sockaddr *) &from_addr_IPv4, (socklen_t *) &fromlen);
        xSemaphoreGive (getLwIpMutex ());

        if (*bytes <= 0) {
            if ((errno == EAGAIN || errno == ENAVAIL) && (micros () - startMicros < timeoutMicros)) {
                delay (1);
                continue;
            }
            return "timeout";
        }

        // did we get at least all the data that we need?
        byte type;
        int id;
        uint16_t seqno;
        unsigned long sentMicros;

        if (__isIPv6__) {
            if (*bytes < (int) (40 + sizeof (struct icmp6_echo_hdr) + sizeof (unsigned long)))
                continue;

            // get the echo
            struct icmp6_echo_hdr *iecho = (struct icmp6_echo_hdr *) (buf + 40);

            type = iecho->type;
            id = iecho->id;
            seqno = iecho->seqno;
            sentMicros = *(unsigned long *) (((char *) iecho) + sizeof (struct icmp6_echo_hdr));

            // subtract IPv6 internet header and struct icmp6_echo_hdr header from *bytes
            *bytes -= (40 + sizeof (struct icmp6_echo_hdr));

        } else {
            struct ip_hdr *iphdr = (struct ip_hdr*) buf;
            int iphdr_len = IPH_HL (iphdr) * 4;

            if (*bytes < (int) (iphdr_len + sizeof (struct icmp_echo_hdr) + sizeof (unsigned long)))
                continue;

            // get the echo
            struct icmp_echo_hdr *iecho = (struct icmp_echo_hdr *) (buf + iphdr_len);

            type = iecho->type;
            id = iecho->id;
            seqno = iecho->seqno;
            sentMicros = *(unsigned long *) (((char *) iecho) + sizeof (struct icmp_echo_hdr));

            // subtract IPv4 internet header and icmp_echo_hdr from *bytes
            *bytes -= (iphdr_len + sizeof (struct icmp_echo_hdr));
        }

        // check if this is a reply we expected
        if (id < LWIP_SOCKET_OFFSET || id >= LWIP_SOCKET_OFFSET + MEMP_NUM_NETCONN || !(type == ICMP_ER || type == ICMP6_ECHO_REPLY))
            continue;

        // now we should consider several options:

        // did we pick up the echo packet that was send through the socket sockfd?
        if (id == sockfd) {

            // did we pick up the echo packet with the latest sequence number?
            if (__pingReplies__ [sockfd - LWIP_SOCKET_OFFSET].seqno == seqno) {
                // write information about the reply in the data structure
                __pingReplies__ [sockfd - LWIP_SOCKET_OFFSET].elapsed_time = micros () - sentMicros;
                return NULL; // OK
            } // else the sequence numbers do not match, ignore this echo packet, its time-out has probably already been reported
        } else {    // we picked up an echo packet that was sent from another socket
            // did we pick up the echo packet with the latest sequence number?
            if (__pingReplies__ [id - LWIP_SOCKET_OFFSET].seqno == seqno) {
                // write information about the reply in the data structure
                __pingReplies__ [id - LWIP_SOCKET_OFFSET].elapsed_time = micros () - sentMicros;
                // do not return now, continue waiting for the right echo packet
            } // else the sequence numbers do not match, ignore this echo packet, its time-out has probably already been reported
        }
    }
}
