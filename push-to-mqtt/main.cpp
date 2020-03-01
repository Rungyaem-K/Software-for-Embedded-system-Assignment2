#include <mbed.h>
#include "MQTTNetwork.h"
#include "MQTTmbed.h"
#include "MQTTClient.h"
DigitalOut led(LED1);
InterruptIn button(USER_BUTTON);
Thread t;
EventQueue queue(5 * EVENTS_EVENT_SIZE);
Serial pc(USBTX, USBRX);
WiFiInterface *wifi;

int arrivedcount = 0;

void messageArrived(MQTT::MessageData& md)
{
    MQTT::Message &message = md.message;
    pc.printf("Message arrived: qos %d, retained %d, dup %d, packetid %d\r\n", message.qos, message.retained, message.dup, message.id);
    pc.printf("Payload %.*s\r\n", message.payloadlen, (char*)message.payload);
    ++arrivedcount;
}
 
//  int mqttcall(int argc, char* argv[],network)
int mqttcall(NetworkInterface* network)
{
    float version = 0.6;
    char* topic = "@msg/Topic";
 
    pc.printf("HelloMQTT: version is %.2f\r\n", version);
 
    MQTTNetwork mqttNetwork(network);
 
    MQTT::Client<MQTTNetwork, Countdown> client(mqttNetwork);
 
    const char* hostname = "mqtt.netpie.io";
    int port = 1883;
    pc.printf("Connecting to %s:%d\r\n", hostname, port);
    // mqttNetwork.connect(hostname, port);
    int rc = mqttNetwork.connect(hostname, port);
 
    MQTTPacket_connectData data = MQTTPacket_connectData_initializer;
    data.MQTTVersion = 3;
    data.clientID.cstring = "11f5c54e-1c4d-4d69-a354-8c0cff20f0f3";
    data.username.cstring = "EfJx1uZLFB4LGtKaHxQBAk7WzweJyWk2";
    // client.connect(data);
    if ((rc = client.connect(data)) != 0)
        pc.printf("rc from MQTT connect is %d\r\n", rc);
 
    if ((rc = client.subscribe(topic, MQTT::QOS2, messageArrived)) != 0)
        pc.printf("rc from MQTT subscribe is %d\r\n", rc);
     
    MQTT::Message message;
 
    // QoS 0
    char buf[100];
    sprintf(buf, "Hello World!  QoS 0 message \r\n");
    message.qos = MQTT::QOS0;
    message.retained = false;
    message.dup = false;
    message.payload = (void*)buf;
    message.payloadlen = strlen(buf)+1;
    rc = client.publish(topic, message);
    while (arrivedcount < 1)
        client.yield(100);
 
    // QoS 1
    sprintf(buf, "Hello World!  QoS 1 message \r\n");
    message.qos = MQTT::QOS1;
    message.payloadlen = strlen(buf)+1;
    rc = client.publish(topic, message);
    while (arrivedcount < 2)
        client.yield(100);
 
    // QoS 2
    sprintf(buf, "Hello World!  QoS 2 message \r\n");
    message.qos = MQTT::QOS2;
    message.payloadlen = strlen(buf)+1;
    rc = client.publish(topic, message);
    while (arrivedcount < 3)
        client.yield(100);
 
    if ((rc = client.unsubscribe(topic)) != 0)
        pc.printf("rc from unsubscribe was %d\r\n", rc);
 
    if ((rc = client.disconnect()) != 0)
        pc.printf("rc from disconnect was %d\r\n", rc);
 
    mqttNetwork.disconnect();
 
    pc.printf("Version %.2f: finish %d msgs\r\n", version, arrivedcount);
 
    return 0;
}



const char *sec2str(nsapi_security_t sec)
{
    switch (sec) {
        case NSAPI_SECURITY_NONE:
            return "None";
        case NSAPI_SECURITY_WEP:
            return "WEP";
        case NSAPI_SECURITY_WPA:
            return "WPA";
        case NSAPI_SECURITY_WPA2:
            return "WPA2";
        case NSAPI_SECURITY_WPA_WPA2:
            return "WPA/WPA2";
        case NSAPI_SECURITY_UNKNOWN:
        default:
            return "Unknown";
    }
}

int scan_wifi() {
    WiFiAccessPoint *ap;

    printf("Scan:\n");
    int count = wifi->scan(NULL,0);
    if (count <= 0) {
        printf("scan() failed with return value: %d\n", count);
        return 0;
    }

    /* Limit number of network arbitrary to 15 */
    count = count < 15 ? count : 15;
    ap = new WiFiAccessPoint[count];
    count = wifi->scan(ap, count);
    if (count <= 0) {
        printf("scan() failed with return value: %d\n", count);
        return 0;
    }

    for (int i = 0; i < count; i++) {
        printf("Network: %s secured: %s BSSID: %hhX:%hhX:%hhX:%hhx:%hhx:%hhx RSSI: %hhd Ch: %hhd\n", ap[i].get_ssid(),
               sec2str(ap[i].get_security()), ap[i].get_bssid()[0], ap[i].get_bssid()[1], ap[i].get_bssid()[2],
               ap[i].get_bssid()[3], ap[i].get_bssid()[4], ap[i].get_bssid()[5], ap[i].get_rssi(), ap[i].get_channel());
    }
    printf("%d networks available.\n", count);

    delete[] ap;   

    return count; 
}


void pressed_handler() {
    int count;

    count = scan_wifi();
    if (count == 0) {
        pc.printf("No WIFI APs found - can't continue further.\n");
        return;
    }

    pc.printf("\nConnecting to %s...\n", MBED_CONF_APP_WIFI_SSID);
    int ret = wifi->connect(MBED_CONF_APP_WIFI_SSID, MBED_CONF_APP_WIFI_PASSWORD, NSAPI_SECURITY_WPA_WPA2);
    if (ret != 0) {
        pc.printf("\nConnection error: %d\n", ret);
        return;
    }

    pc.printf("Success\n\n");
    pc.printf("MAC: %s\n", wifi->get_mac_address());
    
    pc.printf("IP: %s\n", wifi->get_ip_address());
    pc.printf("Netmask: %s\n", wifi->get_netmask());
    pc.printf("Gateway: %s\n", wifi->get_gateway());
    pc.printf("RSSI: %d\n\n", wifi->get_rssi());
    mqttcall(wifi);
    wifi->disconnect();
    pc.printf("\nDone\n");    
}





int main() {
    wifi = WiFiInterface::get_default_instance();
    if (!wifi) {
        printf("ERROR: No WiFiInterface found.\n");
        return -1;
    }

    t.start(callback(&queue, &EventQueue::dispatch_forever));
    button.fall(queue.event(pressed_handler));
    pc.printf("Starting\n");
    while(1) {
        led = !led;
        ThisThread::sleep_for(500);
    }
}