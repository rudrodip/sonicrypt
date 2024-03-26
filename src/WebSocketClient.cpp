#include <ArduinoWebsockets.h>
#include "WebSocketClient.h"

using namespace websockets;

const char* wsEndpoint = "api.devnet.solana.com";
const char* wsPath = "/";
const int wsPort = 80;
const char* address = "7LwsCzvPoJJD8d15yiH9D411RPpQJTb3QTePR7HgBQKH";
int finalizeSub;
int confirmSub;

WebsocketsClient client;
HTTPClient http;

void finalizeTx();

void onEventsCallback(WebsocketsEvent event, String data) {
    if(event == WebsocketsEvent::ConnectionOpened) {
        Serial.println("Connnection Opened");
    } else if(event == WebsocketsEvent::ConnectionClosed) {
        Serial.println("Connnection Closed");
    } else if(event == WebsocketsEvent::GotPing) {
        Serial.println("Got a Ping!");
    } else if(event == WebsocketsEvent::GotPong) {
        Serial.println("Got a Pong!");
    }
}

void onMessageCallback(WebsocketsMessage message) {
    Serial.print("Got Message: ");
    Serial.println(message.data());

    parseSubId(message.data());
    parseTx(message.data());
}

void parseSubId(String json) {
    JsonDocument doc;
    const char* jsonChar = json.c_str();
    DeserializationError err = deserializeJson(doc, jsonChar);

    if(err) {
        Serial.print("Error parsing JSON: ");
        Serial.println(err.c_str());
        return;
    }

    if (!doc.containsKey("result")) {
        return;
    }

    int id = doc["id"];
    if(id == 1) {
        finalizeSub = doc["result"];
        Serial.print("Finalized subscription ID: ");
        Serial.println(finalizeSub);
    } else if(id == 2) {
        confirmSub = doc["result"];
        Serial.print("Confirmed subscription ID: ");
        Serial.println(confirmSub);
    }
}

void parseTx(String json) {
    JsonDocument doc;
    const char* jsonChar = json.c_str();

    DeserializationError err = deserializeJson(doc, jsonChar);

    if (err) {
        Serial.print("Error parsing JSON: ");
        Serial.println(err.c_str());
        return;
    }

    if (!doc.containsKey("params")) {
        return;
    }

    int subId = doc["params"]["subscription"];
    if(subId == finalizeSub) {
        Serial.println("Finalized transaction");
        delay(500);
        finalizeTx();
    } else if(subId == confirmSub) {
        Serial.println("Confirmed transaction");
    }
}

void initWebSocket() {
    Serial.println("Initializing WebSocket...");
    
    client.onEvent(onEventsCallback);
    client.onMessage(onMessageCallback);

    client.connect(wsEndpoint, wsPort, wsPath);

    // Send subscription requests
    String addressStr = String(address);
    String subscribeRequestFinalizedStr = "{\"jsonrpc\":\"2.0\",\"id\":1,\"method\":\"accountSubscribe\",\"params\":[\"" + addressStr + "\",{\"encoding\":\"jsonParsed\",\"commitment\":\"finalized\"}]}";
    String subscribeRequestConfirmedStr = "{\"jsonrpc\":\"2.0\",\"id\":2,\"method\":\"accountSubscribe\",\"params\":[\"" + addressStr + "\",{\"encoding\":\"jsonParsed\",\"commitment\":\"confirmed\"}]}";
    const char* subscribeRequestFinalized = subscribeRequestFinalizedStr.c_str();
    const char* subscribeRequestConfirmed = subscribeRequestConfirmedStr.c_str();

    Serial.println("Subscription request (finalized):");
    Serial.println(subscribeRequestFinalized);
    client.send(subscribeRequestFinalized);

    Serial.println("Subscription request (confirmed):");
    Serial.println(subscribeRequestConfirmed);
    client.send(subscribeRequestConfirmed);
}

void handleWebSocket() {
    client.poll();

    // retry connection if disconnected every second
    if(!client.available()) {
        delay(1000);
        Serial.println("Reconnecting...");
        initWebSocket();
    }
}

void finalizeTx() {
    JsonDocument doc;
    http.begin("http://api.devnet.solana.com");
    http.addHeader("Content-Type", "application/json");
    String body = "{\"jsonrpc\":\"2.0\",\"id\":1,\"method\":\"getConfirmedSignaturesForAddress2\",\"params\":[\"" + String(address) + "\",{\"limit\":1}]}";

    int httpCode = http.POST(body);
    Serial.println("HTTP Code: " + String(httpCode));
    if (httpCode <= 0) {
        Serial.println("Error on HTTP request");
        return;
    }

    String response = http.getString();
    const char* responseChar = response.c_str();
    DeserializationError err = deserializeJson(doc, responseChar);

    if(err) {
        Serial.print("Error parsing JSON: ");
        Serial.println(err.c_str());
        return;
    }

    if (!doc.containsKey("result")) {
        Serial.println("No result key in response");
        return;
    }

    String signature = doc["result"][0]["signature"];
    Serial.print("Signature: ");
    Serial.println(signature);

    http.begin("http://api.devnet.solana.com");
    http.addHeader("Content-Type", "application/json");
    body = "{\"jsonrpc\":\"2.0\",\"id\":1,\"method\":\"getTransaction\",\"params\":[\"" + signature + "\"]}";
    httpCode = http.POST(body);
    Serial.println("HTTP Code: " + String(httpCode));

    if (httpCode <= 0) {
        Serial.println("Error on HTTP request");
        return;
    }

    response = http.getString();
    responseChar = response.c_str();
    err = deserializeJson(doc, responseChar);

    if(err) {
        Serial.print("Error parsing JSON: ");
        Serial.println(err.c_str());
        return;
    }

    if (!doc.containsKey("result")) {
        Serial.println("No result key in response");
        return;
    }

    String sender = doc["result"]["transaction"]["message"]["accountKeys"][0];
    String receiver = doc["result"]["transaction"]["message"]["accountKeys"][1];

    if (sender == address) {
        Serial.println("You've sent a transaction");
        return;
    } else if (receiver == address) {
        Serial.println("You've received a transaction");
        return;
    } else {
        Serial.println("You're not involved in this transaction");
        return;
    }

    http.end();
}