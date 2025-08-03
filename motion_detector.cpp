#include "motion_detector.h"
#include "wifi_drv_api/mt76_api.h"
#include "parsers/parser_mt76.h"
#include <chrono>
#include <iostream>
#include <thread>
#include <cstring>
#include <unistd.h>
#include <arpa/inet.h>

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

void MotionDetector::udpServerListen()
{
    char buffer[1024];
    struct sockaddr_in clientAddr;
    socklen_t clientAddrLen = sizeof(clientAddr);

    while (udpServerRunning) {
        ssize_t recvLen = recvfrom(udpSocket, buffer, sizeof(buffer) - 1, 0, (struct sockaddr*)&clientAddr, &clientAddrLen);
        if (recvLen > 0) {
            buffer[recvLen] = '\0';
            if (strcmp(buffer, "register") == 0) {
                char clientIp[INET_ADDRSTRLEN];
                inet_ntop(AF_INET, &clientAddr.sin_addr, clientIp, INET_ADDRSTRLEN);
                int clientPort = ntohs(clientAddr.sin_port);
                addUdpClient(clientIp, clientPort);
            }
        }
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

    // Bind socket
    struct sockaddr_in serverAddr;
    memset(&serverAddr, 0, sizeof(serverAddr));
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = INADDR_ANY;
    serverAddr.sin_port = htons(port);

    if (bind(udpSocket, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) < 0) {
        std::cerr << "Failed to bind UDP socket: " << strerror(errno) << std::endl;
        close(udpSocket);
        return -1;
    }

    udpServerRunning = true;
    udpServerWorker = std::thread(&MotionDetector::udpServerListen, this);
    std::cout << "UDP server started successfully on port " << port << std::endl;
    return 0;
}

int MotionDetector::stopUdpServer()
{
    if (!udpServerRunning) {
        return 0;
    }

    udpServerRunning = false;
    
    // Unblock recvfrom
    shutdown(udpSocket, SHUT_RDWR);

    if (udpServerWorker.joinable()) {
        udpServerWorker.join();
    }

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
    // Check if client already exists
    auto it = std::find(udpClients.begin(), udpClients.end(), std::make_pair(clientIp, clientPort));
    if (it == udpClients.end()) {
        udpClients.push_back(std::make_pair(clientIp, clientPort));
        std::cout << "Added UDP client: " << clientIp << ":" << clientPort << std::endl;
    }
    udpMutex.unlock();
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

    // Calculate total number of I/Q pairs (samples / 2 since each sample is I,Q pair)
    uint32_t totalSamples = 0;
    for (const auto& packet : data) {
        totalSamples += packet.size() / 2; // I/Q pairs are interleaved
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
    
    // Copy CSI data as I/Q pairs
    for (const auto& packet : data) {
        for (size_t i = 0; i < packet.size(); i += 2) {
            CsiSample sample;
            sample.i = packet[i];     // I component
            sample.q = packet[i + 1]; // Q component
            memcpy(ptr, &sample, sizeof(CsiSample));
            ptr += sizeof(CsiSample);
        }
    }

    // Send to each UDP client
    for (const auto& client : clients) {
        struct sockaddr_in clientAddr;
        memset(&clientAddr, 0, sizeof(clientAddr));
        clientAddr.sin_family = AF_INET;
        clientAddr.sin_port = htons(client.second);

        // Convert IP address
        if (inet_pton(AF_INET, client.first.c_str(), &clientAddr.sin_addr) <= 0) {
            continue;
        }

        ssize_t sent = sendto(udpSocket, buffer.data(), buffer.size(), 0,
                             (struct sockaddr*)&clientAddr, sizeof(clientAddr));
        
        if (sent < 0) {
            std::cerr << "Failed to send UDP data to " << client.first << ":" << client.second << ": " << strerror(errno) << std::endl;
        }
    }
}
