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
#include <mutex>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "wifi_drv_api/mt76_api.h"

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
    int startUdpServer(int port);
    int stopUdpServer();
    void addUdpClient(const std::string& clientIp, int clientPort);
    void removeUdpClient(const std::string& clientIp, int clientPort);

private:
    void runMonitoring();
    void udpServerListen();
    void sendCsiDataUdp(const std::vector<std::vector<double>>& data, int antennaIdx);

    static MotionDetector* instance;
    MotionDetector() : isMonitoring(false), stopFlag(false), antMonIdx(0), motion_result(0.0),
                       interval(0), udpServerRunning(false), udpSocket(-1) {}

    std::string ifname;
    unsigned interval;
    unsigned antMonIdx;
    std::atomic<bool> stopFlag;
    std::chrono::time_point<std::chrono::steady_clock> startMon;
    MT76API wifi;

    double motion_result;
    bool isMonitoring;
    std::thread monitorWorker;
    std::thread udpServerWorker;
    std::mutex dataMutex;
    std::mutex udpMutex;
    std::vector<std::pair<std::string, int>> udpClients;
    int udpSocket;
    bool udpServerRunning;
};