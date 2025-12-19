# Thread-safe Ping Arduino Library for ESP32

A robust, thread-safe ping implementation for ESP32, designed for multitasking environments.  

---

## ✅ Features

- **Thread‑safe**  
  Works reliably inside multiple FreeRTOS tasks without packet collisions.

- **Intermediate result reporting**  
  Override `onReceive()` and `onWait()` to display progress in real time.

- **Non‑blocking operation**  
  Uses raw sockets in non-blocking mode for precise timeout handling.

- **Accurate timing**  
  Round-trip time measured using `micros()` with microsecond precision.

- **Incremental statistics**  
  Mean, variance, min/max latency computed without storing all samples.

- **Compatible with Arduino IDE**

---

## ✅ Why This Library Exists

Existing ESP32 ping libraries are excellent foundations, but they lack:

 - thread-safety

 - non-blocking operation

 - intermediate result callbacks

