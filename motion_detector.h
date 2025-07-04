#pragma once

#include <atomic>
#include <string>
#include <thread>
#include <mutex>
#include <chrono>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <vector>
#include <cstdint>
#include "wifi_drv_api/mt76_api.h"

// Simple structs for UDP data transmission
struct CsiPacketHeader {
    uint64_t timestamp;
    uint32_t antenna_idx;
    uint32_t packet_count;
    uint32_t total_samples;
} __attribute__((packed));

struct CsiSample {
    double value;
} __attribute__((packed));

class MotionDetector
{
public:
    static MotionDetector& getInstance();

    MotionDetector(const MotionDetector&) = delete;
    MotionDetector& operator=(const MotionDetector&) = delete;

    int startMonitoring(std::string ifname, unsigned interval);
    int stopMonitoring();

    int setAntennaIdx(unsigned idx);
    unsigned getAntennaIdx();
    double getMotion();
    bool getIsMonitoring();
    
    // UDP server methods
    int startUdpServer(int port);
    int stopUdpServer();
    void addUdpClient(const std::string& clientIp, int clientPort);
    void removeUdpClient(const std::string& clientIp, int clientPort);

private:
    std::string ifname;
    unsigned interval;
    unsigned antMonIdx;

    double motion_result;
    bool isMonitoring;
    std::thread monitorWorker;
    std::mutex dataMutex;
    std::atomic<bool> stopFlag;
    std::chrono::time_point<std::chrono::steady_clock> startMon;

    MT76API wifi;
    
    // UDP server variables
    int udpSocket;
    bool udpServerRunning;
    std::vector<std::pair<std::string, int>> udpClients;
    std::mutex udpMutex;

    MotionDetector() : udpSocket(-1), udpServerRunning(false) {}
    static MotionDetector* instance;

    void runMonitoring();
    void sendCsiDataUdp(const std::vector<std::vector<double>>& data, int antennaIdx);
};
