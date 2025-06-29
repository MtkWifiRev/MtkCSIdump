#include "motion_detector.h"
#include "wifi_drv_api/mt76_api.h"
#include "estimators/kurtosis_motion_estimator.h"
#include "parsers/parser_mt76.h"
#include <chrono>
#include <iostream>
#include <thread>

MotionDetector* MotionDetector::instance = nullptr;

MotionDetector& MotionDetector::getInstance()
{
    if (instance == nullptr)
        instance = new MotionDetector();

    return *instance;
}

void MotionDetector::runMonitoring()
{
    std::chrono::time_point<std::chrono::steady_clock> currTime;
    std::chrono::time_point<std::chrono::steady_clock> oldTime = startMon;
    int diff;
    int pkt_num;
    ParserMT76 parser;

    KurtosisMotionEstimator motionEstimator[ANTENNA_NUM];

    while (!stopFlag.load()) {
        // pkt_num = (time from last[ms] / interval) * 4
        currTime = std::chrono::steady_clock::now();
        diff = std::chrono::duration_cast<std::chrono::milliseconds>(currTime - oldTime).count();
        //pkt_num = (diff / interval) * 4;;
        pkt_num = 100;
        std::vector<csi_data *> *list = wifi.motion_detection_dump(ifname.c_str(), pkt_num);
        oldTime = currTime;

        dataMutex.lock();
        unsigned antIdx = antMonIdx;
        dataMutex.unlock();
        for (int i = 0 ; i < ANTENNA_NUM; i++) {
            if (list) {
                std::vector<std::vector<double>> parsed_data = parser.processRawData(list, i);
                if (parsed_data.size()) {
                    fprintf(stderr, "!!!!!!!!!!! found data: %d !!!!!!!!!!!!!!!\n", parsed_data.size());
                } else {
                    fprintf(stderr, "Dump List Empty!\n");
                }
            } else {
                fprintf(stderr, "Dump List NULL!\n");
            }
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(interval));
    }
}

int MotionDetector::startMonitoring(std::string ifname, unsigned interval)
{
    int ret;

    if (isMonitoring)
        stopMonitoring();

    if (interval == 0)
        return -1;

    this->ifname = ifname;
    this->interval = interval;

    ret = wifi.motion_detection_start(this->ifname.c_str(), this->interval);
    startMon = std::chrono::steady_clock::now();
    if (!ret)
    {
        isMonitoring = true;
        stopFlag.store(false);
        monitorWorker = std::thread(&MotionDetector::runMonitoring, this);
    }

    return ret;
}

int MotionDetector::stopMonitoring()
{
    int ret;

    stopFlag.store(true);
    if (monitorWorker.joinable())
        monitorWorker.join();

    ret = wifi.motion_detection_stop(ifname.c_str());
    if (!ret)
    {
        isMonitoring = false;
        ifname.clear();
        interval = 0;
    }

    return ret;
}

int MotionDetector::setAntennaIdx(unsigned idx)
{
    if (idx >= ANTENNA_NUM)
        return -1;

    dataMutex.lock();
    antMonIdx = idx;
    dataMutex.unlock();

    return 0;
}

unsigned MotionDetector::getAntennaIdx()
{
    dataMutex.lock();
    unsigned antIdx = antMonIdx;
    dataMutex.unlock();

    return antIdx;
}


double MotionDetector::getMotion()
{
    dataMutex.lock();
    double motion = motion_result;
    dataMutex.unlock();

    return motion;
}

bool MotionDetector::getIsMonitoring()
{
    return isMonitoring;
}
