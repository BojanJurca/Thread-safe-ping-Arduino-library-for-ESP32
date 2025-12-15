#include <WiFi.h>
#include <ThreadSafePing.h>


void setup () {
    Serial.begin (115200);
    while (!Serial)
        delay (10);

    WiFi.begin ("YourSSID", "YourPassword"); // use your WiFi credentials

    Serial.print ("Connecting ... ");
    while (WiFi.status () != WL_CONNECTED) delay (100);
    Serial.print ("connected\nGetting IP address ... ");
    while (WiFi.localIP () == IPAddress (0, 0, 0, 0)) delay (100);
    Serial.println (WiFi.localIP ());
    Serial.print ("Router's IP address is ");
    Serial.println (WiFi.gatewayIP ());
}

void loop () {
    static unsigned long lastWifiCheck = millis ();
    if (millis () - lastWifiCheck >= 3600000) { // 3600000 ms = 1 hour
        lastWifiCheck = millis ();

        ThreadSafePing ping;
        // ping router 4 times
        ping.ping (WiFi.gatewayIP (), 4); // optional arguments: int count = PING_DEFAULT_COUNT, int interval = PING_DEFAULT_INTERVAL, int size = PING_DEFAULT_SIZE, int timeout = PING_DEFAULT_TIMEOUT
        if (ping.received () == 0) {
            Serial.printf ("Not connected ... reconnecting\n");
            WiFi.disconnect ();
            WiFi.reconnect ();             
        }
    }

    // ...

}
