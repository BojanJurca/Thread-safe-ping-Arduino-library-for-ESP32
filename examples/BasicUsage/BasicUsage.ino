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


    ThreadSafePing_t ping;

    Serial.printf ("Pinging %i times ...\n", PING_DEFAULT_COUNT);
    ping.ping ("arduino.com"); // optional arguments: int count = PING_DEFAULT_COUNT, int interval = PING_DEFAULT_INTERVAL, int size = PING_DEFAULT_SIZE, int timeout = PING_DEFAULT_TIMEOUT
    if (ping.errText () != NULL) {
        Serial.printf ("Error %s\n", ping.errText ());
    } else {
        Serial.printf ("Ping statistics for %s:\n"
                       "    Packets: Sent = %i, Received = %i, Lost = %i", ping.target (), ping.sent (), ping.received (), ping.lost ());
        if (ping.sent ()) {
            Serial.printf (" (%.2f%% loss)\nRound trip:\n"
                           "   Min = %.3fms, Max = %.3fms, Avg = %.3fms, Stdev = %.3fms\n", (float) ping.lost () / (float) ping.sent () * 100, ping.min_time (), ping.max_time (), ping.mean_time (), sqrt (ping.var_time () / ping.received ()));
        } else {
            Serial.printf ("\n");
        }
    }
}

void loop () {

}
