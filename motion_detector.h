#pragma once

#include <atomic>
#include <string>
#include <thread>
#include <mutex>
#include <chrono>
#include "wifi_drv_api/mt76_api.h"

class MotionDetector
{
public:
    static MotionDetector& getInstance();

    MotionDetector(const MotionDetector&) = delete;
    MotionDetector& operator=(const MotionDetector&) = delete;

    int startMonitoring(std::string ifname, std::string mac, unsigned interval);
    int stopMonitoring();

    int setAntennaIdx(unsigned idx);
    unsigned getAntennaIdx();
    double getMotion();
    bool getIsMonitoring();

private:
    std::string ifname;
    std::string mac;
    unsigned interval;
    unsigned antMonIdx;

    double motion_result;
    bool isMonitoring;
    std::thread monitorWorker;
    std::mutex dataMutex;
    std::atomic<bool> stopFlag;
    std::chrono::time_point<std::chrono::steady_clock> startMon;

    MT76API wifi;

    MotionDetector() = default;
    static MotionDetector* instance;

    void runMonitoring();
};
