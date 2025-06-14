#ifndef MD_H_
#define MD_H_

#ifdef __cplusplus
extern "C"
{
#endif

    int md_get_motion();

    int md_is_monitoring_active();
    int md_start_monitoring(const char *ifname, const char *mac, unsigned interval);
    int md_stop_monitoring();

    int md_set_antenna_idx(unsigned idx);
    unsigned md_get_antenna_idx();

#ifdef __cplusplus
}
#endif

#endif
