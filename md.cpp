#include "md.h"
#include "wifi_drv_api/mt76_api.h"
#include "motion_detector.h"
#include <cmath>

extern "C"
{
    int md_get_motion()
    {
        return std::round(MotionDetector::getInstance().getMotion());
    }

    int md_is_monitoring_active()
    {
        return (int)MotionDetector::getInstance().getIsMonitoring();
    }

    int md_start_monitoring(const char *ifname, unsigned interval)
    {
        int ret = MotionDetector::getInstance().startMonitoring(ifname, interval);

        return ret;
    }

    int md_stop_monitoring()
    {
        int ret = MotionDetector::getInstance().stopMonitoring();
        return ret;
    }

    int md_set_antenna_idx(unsigned idx)
    {
        return MotionDetector::getInstance().setAntennaIdx(idx);
    }

    unsigned md_get_antenna_idx()
    {
        return MotionDetector::getInstance().getAntennaIdx();
    }
}
