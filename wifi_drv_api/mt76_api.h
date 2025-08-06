#ifndef __MT76_VENDOR_H
#define __MT76_VENDOR_H

#include <stdint.h>
#include <vector>

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;
typedef int8_t s8;
typedef int16_t s16;
typedef int32_t s32;
typedef int64_t s64, ktime_t;

#define CSI_BW20_DATA_COUNT 64
#define CSI_BW40_DATA_COUNT 128
#define CSI_BW80_DATA_COUNT 256
#define CSI_BW160_DATA_COUNT 512
#define CSI_BW320_DATA_COUNT 1024
#define ETH_ALEN 6

struct csi_data
{
    u8 ch_bw;
    u16 data_num;
    s16 data_i[CSI_BW320_DATA_COUNT];
    s16 data_q[CSI_BW320_DATA_COUNT];
    u8 band;
    s8 rssi;
    u8 snr;
    u32 ts;
    u8 data_bw;
    u8 pri_ch_idx;
    u8 ta[ETH_ALEN];
    u32 ext_info;
    u8 rx_mode;
    u32 chain_info;
    u16 tx_idx;
    u16 rx_idx;
    u32 segment_num;
    u8 remain_last;
    u16 pkt_sn;
    u8 tr_stream;
    u32 h_idx;
};

class MT76APIPrivate;
class MT76API
{
public:
    MT76API();
    ~MT76API();

    int motion_detection_start(const char *wifi);
    int motion_detection_stop(const char *wifi);
    std::vector<csi_data *> *motion_detection_dump(const char *wifi, int pkt_num);

private:
    MT76APIPrivate *d;
};

#endif
