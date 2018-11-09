
#include <iostream>
#include <string>
#include <sstream>


using namespace std;
#define MQTT_MAX_PACKET_SIZE 128
#define MQTT_MAX_HEADER_SIZE 5

#define MQTTCONNECT     1 << 4  // Client request to connect to Server
#define MQTTCONNACK     2 << 4  // Connect Acknowledgment
#define MQTTPUBLISH     3 << 4  // Publish message
#define MQTTPUBACK      4 << 4  // Publish Acknowledgment
#define MQTTPUBREC      5 << 4  // Publish Received (assured delivery part 1)
#define MQTTPUBREL      6 << 4  // Publish Release (assured delivery part 2)
#define MQTTPUBCOMP     7 << 4  // Publish Complete (assured delivery part 3)
#define MQTTSUBSCRIBE   8 << 4  // Client Subscribe request
#define MQTTSUBACK      9 << 4  // Subscribe Acknowledgment
#define MQTTUNSUBSCRIBE 10 << 4 // Client Unsubscribe request
#define MQTTUNSUBACK    11 << 4 // Unsubscribe Acknowledgment
#define MQTTPINGREQ     12 << 4 // PING Request
#define MQTTPINGRESP    13 << 4 // PING Response
#define MQTTDISCONNECT  14 << 4 // Client is Disconnecting
#define MQTTReserved    15 << 4 // Reserved

uint8_t buffer[MQTT_MAX_PACKET_SIZE];


const char topic[] = "$aws/things/arduino/update";
// const char topic[] = "cpht/lights";


template<typename T>
string ToString(T t) {
 
  stringstream ss;
 
  ss << t;
 
  return ss.str();
}

size_t write(const uint8_t* buf, size_t size)
{
  size_t written = 0;
  string command;

  while (size) {
    size_t chunkSize = size;

    if (chunkSize > 256) {
      chunkSize = 256;
    }

    command.reserve(19 + chunkSize * 2);
    string sizeString = ToString<size_t>(chunkSize);


    command += "AT+USOWR=";
    command += "0";
    command += ",";
    command += sizeString;
    command += ",\"";

    for (size_t i = 0; i < chunkSize; i++) {
      char b = buf[i + written];

      char n1 = (b >> 4) & 0x0f;
      char n2 = (b & 0x0f);

      command += (char)(n1 > 9 ? 'A' + n1 - 10 : '0' + n1);
      command += (char)(n2 > 9 ? 'A' + n2 - 10 : '0' + n2);
    }

    command += "\"";
    
    std::cout << command << endl;

    // for (uint16_t i=0; i<command.length(); i++){
    //     std::cout << command[i];
    // }

    // MODEM.send(command);
    // if (_writeSync) {
    //   if (MODEM.waitForResponse(10000) != 1) {
    //     break;
    //   }
    // }
    written += chunkSize;
    size -= chunkSize;
  }

  return written;
}

size_t write(const uint8_t *buf)
{
  return write(buf, strlen((const char*)buf));
}

size_t write(uint8_t c)
{
  return write(&c);
}


size_t buildHeader(uint8_t header, uint8_t* buf, uint16_t length) {
    uint8_t lenBuf[4];
    uint8_t llen = 0;
    uint8_t digit;
    uint8_t pos = 0;
    uint16_t len = length;
    do {
        digit = len % 128;
        len = len / 128;
        if (len > 0) {
            digit |= 0x80;
        }
        lenBuf[pos++] = digit;
        llen++;
    } while(len>0);

    buf[4-llen] = header;
    for (int i=0;i<llen;i++) {
        buf[MQTT_MAX_HEADER_SIZE-llen+i] = lenBuf[i];
    }
    return llen+1; // Full header size is variable length bit plus the 1-byte fixed header
}

bool writeMQTT(uint8_t header, uint8_t* buf, uint16_t length) {
    uint16_t rc;
    uint8_t hlen = buildHeader(header, buf, length);

#ifdef MQTT_MAX_TRANSFER_SIZE
    uint8_t* writeBuf = buf+(MQTT_MAX_HEADER_SIZE-hlen);
    uint16_t bytesRemaining = length+hlen;  //Match the length type
    uint8_t bytesToWrite;
    boolean result = true;
    while((bytesRemaining > 0) && result) {
        bytesToWrite = (bytesRemaining > MQTT_MAX_TRANSFER_SIZE)?MQTT_MAX_TRANSFER_SIZE:bytesRemaining;
        rc = _client->write(writeBuf,bytesToWrite);
        result = (rc == bytesToWrite);
        bytesRemaining -= rc;
        writeBuf += rc;
    }
    return result;
#else
    rc = write(buf+(MQTT_MAX_HEADER_SIZE-hlen),length+hlen);
    // lastOutActivity = millis();
    return (rc == hlen+length);
#endif
}

uint16_t writeString(const char* string, uint8_t* buf, uint16_t pos) {
    const char* idp = string;
    uint16_t i = 0;
    pos += 2;
    while (*idp) {
        buf[pos++] = *idp++;

        i++;
    }
    buf[pos-i-2] = (i >> 8);
    buf[pos-i-1] = (i & 0xFF);
    
     size_t size = pos;
    //  std::cout << (char *)buf;
    // for (size_t cnt = 0; cnt < size; cnt++) {
    //     std::cout << buf[cnt];
    // }

    return pos;
}


// 3037001A246177732F7468696E67732F61726475696E6F2F7570646174657B643A207B207374617475733A2022636F6E6E656374656421227D
bool publish(const char* topic, const uint8_t* payload, unsigned int plength, bool retained) {
    if (MQTT_MAX_PACKET_SIZE < 5 + 2+strlen(topic) + plength) {
        // Too long
        return false;
    }

    // Leave room in the buffer for header and variable length field
    uint16_t length = 5;
    length = writeString(topic,buffer,length);
    uint16_t i;
    for (i=0;i<plength;i++) {
        buffer[length++] = payload[i];
    }
    uint8_t header = MQTTPUBLISH;
    if (retained) {
        header |= 1;
    }
    uint16_t j;

    // print buffer
    // for (j=0; j<plength+length; j++){
    //     std::cout << unsigned(buffer[j]);
    // }
    
    //return false;
    return writeMQTT(header,buffer,length-5);
}

bool publish(const char* topic, const uint8_t* payload, unsigned int plength) {
    return publish(topic, payload, plength, false);
}

bool publish(const char* topic, const char* payload, bool retained) {
    return publish(topic,(const uint8_t*)payload,strlen(payload),retained);
}

bool publish(const char* topic, const char* payload) {
    return publish(topic,(const uint8_t*)payload,strlen(payload),false);
}

int main(int argc, char** argv) 
{

    publish(topic, "{d: { status: \"connected!\"}");
    // publish(topic, "on");
    return 0;
}

void handleArgs(int argc, char** argv) {
    if (argc < 3) {
        cout << "Welcome to the mqtt command generator." << endl;
        cout << "Usage: " << argv[0] << " <topic> <value>" << endl; 
    } else {
        publish(argv[1], argv[2]);
    }
    std::cout << "Hello, World!";
}
