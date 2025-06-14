#include <cstring>
#include <iostream>
#include <thread>
#include <future>
#include <signal.h>
#include <sys/socket.h>
#include <fstream>
#include <unistd.h>

#include "estimators/kurtosis_motion_estimator.h"
#include "motion_detector.h"

extern double DEVIATION_MIN;
extern double DEVIATION_MAX;

MotionDetector& md = MotionDetector::getInstance();
int stop = 0;

void signalHandler(int)
{
    md.stopMonitoring();
    stop = 1;
}

int main(int argc, char **argv)
{
    struct sigaction sig = {};
    std::memset(&sig, 0, sizeof(sig));
    sig.sa_handler = signalHandler;

    if (sigaction(SIGINT, &sig, NULL) || sigaction(SIGTERM, &sig, NULL))
        std::cerr << "sigaction error" << std::endl;

    if (argc != 6)
    {
        std::cout << "Need 4 arguments: DEVIATION_MIN DEVIATION_MAX wifi_interface mac_addr interval" << std::endl;
    }

    DEVIATION_MIN = atof(argv[1]);
    DEVIATION_MAX = atof(argv[2]);

    md.startMonitoring(argv[3], argv[4], std::stoul(argv[5]));

    while (!stop)
    {
        std::cout << "Main Function Executing..." << std::endl;
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    return 0;
}
