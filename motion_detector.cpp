#include "motion_detector.h"
#include "wifi_drv_api/mt76_api.h"
#include "parsers/parser_mt76.h"
#include <chrono>
#include <iostream>
#include <thread>
#include <cstring>
#include <unistd.h>

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
                    //fprintf(stderr, "!!!!!!!!!!! found data: %d !!!!!!!!!!!!!!!\n", parsed_data.size());
                    for (int j = 0; j < parsed_data.size(); j++) {
                        //fprintf(stderr, "!!!!!!!!!!! found packets inside data: #[%d] %d !!!!!!!!!!!!!!!\n", j, parsed_data[j].size());
                    }
                    // Send CSI data via UDP if server is running
                    if (udpServerRunning) {
                        // Send each packet separately instead of concatenating
                        for (const auto& packet : parsed_data) {
                            if (!packet.empty()) {
                                std::vector<std::vector<double>> singlePacket = { packet };
                                sendCsiDataUdp(singlePacket, i);
                            }
                        }
                    }
                } else {
                    //fprintf(stderr, "Dump List Empty!\n");
                }
            } else {
                //fprintf(stderr, "Dump List NULL!\n");
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

int MotionDetector::startUdpServer(int port)
{
    if (udpServerRunning) {
        std::cerr << "UDP server is already running" << std::endl;
        return -1;
    }

    // Create UDP socket
    udpSocket = socket(AF_INET, SOCK_DGRAM, 0);
    if (udpSocket < 0) {
        std::cerr << "Failed to create UDP socket: " << strerror(errno) << std::endl;
        return -1;
    }

    // Set socket options
    int opt = 1;
    if (setsockopt(udpSocket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        std::cerr << "Failed to set socket options: " << strerror(errno) << std::endl;
        close(udpSocket);
        return -1;
    }

    udpServerRunning = true;
    std::cout << "UDP server started successfully on port " << port << std::endl;
    return 0;
}

int MotionDetector::stopUdpServer()
{
    if (!udpServerRunning) {
        return 0;
    }

    udpServerRunning = false;
    
    if (udpSocket >= 0) {
        close(udpSocket);
        udpSocket = -1;
    }

    udpMutex.lock();
    udpClients.clear();
    udpMutex.unlock();

    std::cout << "UDP server stopped" << std::endl;
    return 0;
}

void MotionDetector::addUdpClient(const std::string& clientIp, int clientPort)
{
    udpMutex.lock();
    udpClients.push_back(std::make_pair(clientIp, clientPort));
    udpMutex.unlock();
    
    std::cout << "Added UDP client: " << clientIp << ":" << clientPort << std::endl;
}

void MotionDetector::removeUdpClient(const std::string& clientIp, int clientPort)
{
    udpMutex.lock();
    auto it = std::find(udpClients.begin(), udpClients.end(), std::make_pair(clientIp, clientPort));
    if (it != udpClients.end()) {
        udpClients.erase(it);
        std::cout << "Removed UDP client: " << clientIp << ":" << clientPort << std::endl;
    }
    udpMutex.unlock();
}

void MotionDetector::sendCsiDataUdp(const std::vector<std::vector<double>>& data, int antennaIdx)
{
    if (!udpServerRunning || udpSocket < 0) {
        return;
    }

    udpMutex.lock();
    auto clients = udpClients; // Copy to avoid holding lock too long
    udpMutex.unlock();

    if (clients.empty()) {
        return;
    }

    // Calculate total number of samples
    uint32_t totalSamples = 0;
    for (const auto& packet : data) {
        totalSamples += packet.size();
    }

    // Create header
    CsiPacketHeader header;
    header.timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    header.antenna_idx = static_cast<uint32_t>(antennaIdx);
    header.packet_count = static_cast<uint32_t>(data.size());
    header.total_samples = totalSamples;

    // Calculate total message size
    size_t messageSize = sizeof(CsiPacketHeader) + totalSamples * sizeof(CsiSample);
    
    // Allocate buffer
    std::vector<uint8_t> buffer(messageSize);
    uint8_t* ptr = buffer.data();
    
    // Copy header
    memcpy(ptr, &header, sizeof(CsiPacketHeader));
    ptr += sizeof(CsiPacketHeader);
    
    // Copy CSI data
    for (const auto& packet : data) {
        for (double value : packet) {
            CsiSample sample;
            sample.value = value;
            memcpy(ptr, &sample, sizeof(CsiSample));
            ptr += sizeof(CsiSample);
        }
    }

    // Send to all registered clients
    for (const auto& client : clients) {
        struct sockaddr_in clientAddr;
        memset(&clientAddr, 0, sizeof(clientAddr));
        clientAddr.sin_family = AF_INET;
        clientAddr.sin_port = htons(client.second);
        
        if (inet_pton(AF_INET, client.first.c_str(), &clientAddr.sin_addr) <= 0) {
            std::cerr << "Invalid client IP address: " << client.first << std::endl;
            continue;
        }

        ssize_t sent = sendto(udpSocket, buffer.data(), buffer.size(), 0,
                             (struct sockaddr*)&clientAddr, sizeof(clientAddr));
        
        if (sent < 0) {
            std::cerr << "Failed to send UDP data to " << client.first << ":" 
                      << client.second << " - " << strerror(errno) << std::endl;
        } else {
            std::cout << "Sent " << sent << " bytes to " << client.first << ":" 
                      << client.second << " (antenna " << antennaIdx 
                      << ", " << data.size() << " packets, " << totalSamples << " samples)" << std::endl;
        }
    }
}
