// #define _GNU_SOURCE
#include "mt76_api.h"

#include <errno.h>
#include <linux/nl80211.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <vector>

#ifdef __cplusplus
extern "C"
{
#endif
#include <netlink/attr.h>
#include <unl.h>
#ifdef __cplusplus
}
#endif

#include <net/if.h>
#include <unistd.h>

#define MTK_NL80211_VENDOR_ID 0x0ce7
#define CSI_DUMP_PER_NUM 3
#define CSI_MAX_COUNT 256

static struct unl unl;
enum mtk_nl80211_vendor_subcmds
{
    MTK_NL80211_VENDOR_SUBCMD_AMNT_CTRL = 0xae,
    MTK_NL80211_VENDOR_SUBCMD_CSI_CTRL = 0xc2,
    MTK_NL80211_VENDOR_SUBCMD_RFEATURE_CTRL = 0xc3,
    MTK_NL80211_VENDOR_SUBCMD_WIRELESS_CTRL = 0xc4,
    MTK_NL80211_VENDOR_SUBCMD_MU_CTRL = 0xc5,
    MTK_NL80211_VENDOR_SUBCMD_PHY_CAPA_CTRL = 0xc6,
};

enum mtk_vendor_attr_csi_ctrl
{
    MTK_VENDOR_ATTR_CSI_CTRL_UNSPEC,

    MTK_VENDOR_ATTR_CSI_CTRL_CFG,
    MTK_VENDOR_ATTR_CSI_CTRL_CFG_MODE,
    MTK_VENDOR_ATTR_CSI_CTRL_CFG_TYPE,
    MTK_VENDOR_ATTR_CSI_CTRL_CFG_VAL1,
    MTK_VENDOR_ATTR_CSI_CTRL_CFG_VAL2,
    MTK_VENDOR_ATTR_CSI_CTRL_MAC_ADDR,
    MTK_VENDOR_ATTR_CSI_CTRL_INTERVAL,

    MTK_VENDOR_ATTR_CSI_CTRL_DUMP_NUM,

    MTK_VENDOR_ATTR_CSI_CTRL_DATA,

    /* keep last */
    NUM_MTK_VENDOR_ATTRS_CSI_CTRL,
    MTK_VENDOR_ATTR_CSI_CTRL_MAX =
        NUM_MTK_VENDOR_ATTRS_CSI_CTRL - 1
};

enum mtk_vendor_attr_csi_data
{
    MTK_VENDOR_ATTR_CSI_DATA_UNSPEC,
    MTK_VENDOR_ATTR_CSI_DATA_PAD,

    MTK_VENDOR_ATTR_CSI_DATA_VER,
    MTK_VENDOR_ATTR_CSI_DATA_TS,
    MTK_VENDOR_ATTR_CSI_DATA_RSSI,
    MTK_VENDOR_ATTR_CSI_DATA_SNR,
    MTK_VENDOR_ATTR_CSI_DATA_BW,
    MTK_VENDOR_ATTR_CSI_DATA_CH_IDX,
    MTK_VENDOR_ATTR_CSI_DATA_TA,
    MTK_VENDOR_ATTR_CSI_DATA_I,
    MTK_VENDOR_ATTR_CSI_DATA_Q,
    MTK_VENDOR_ATTR_CSI_DATA_INFO,
    MTK_VENDOR_ATTR_CSI_DATA_RSVD1,
    MTK_VENDOR_ATTR_CSI_DATA_RSVD2,
    MTK_VENDOR_ATTR_CSI_DATA_RSVD3,
    MTK_VENDOR_ATTR_CSI_DATA_RSVD4,
    MTK_VENDOR_ATTR_CSI_DATA_TX_ANT,
    MTK_VENDOR_ATTR_CSI_DATA_RX_ANT,
    MTK_VENDOR_ATTR_CSI_DATA_MODE,
    MTK_VENDOR_ATTR_CSI_DATA_H_IDX,

    /* keep last */
    NUM_MTK_VENDOR_ATTRS_CSI_DATA,
    MTK_VENDOR_ATTR_CSI_DATA_MAX =
            NUM_MTK_VENDOR_ATTRS_CSI_DATA - 1
};


static struct nla_policy csi_ctrl_policy[NUM_MTK_VENDOR_ATTRS_CSI_CTRL];
static struct nla_policy csi_data_policy[NUM_MTK_VENDOR_ATTRS_CSI_DATA];
static std::vector<csi_data *> csi_list;
class MT76APIPrivate
{
public:
    MT76APIPrivate()
    {
//        csi_ctrl_policy[MTK_VENDOR_ATTR_CSI_CTRL_BAND_IDX].type = NLA_U8;
        csi_ctrl_policy[MTK_VENDOR_ATTR_CSI_CTRL_CFG].type = NLA_NESTED;
        csi_ctrl_policy[MTK_VENDOR_ATTR_CSI_CTRL_CFG_MODE].type = NLA_U8;
        csi_ctrl_policy[MTK_VENDOR_ATTR_CSI_CTRL_CFG_TYPE].type = NLA_U8;
        csi_ctrl_policy[MTK_VENDOR_ATTR_CSI_CTRL_CFG_VAL1].type = NLA_U8;
        csi_ctrl_policy[MTK_VENDOR_ATTR_CSI_CTRL_CFG_VAL2].type = NLA_U8;
        csi_ctrl_policy[MTK_VENDOR_ATTR_CSI_CTRL_MAC_ADDR].type = NLA_NESTED;
//        csi_ctrl_policy[MTK_VENDOR_ATTR_CSI_CTRL_INTERVAL].type = NLA_U32;
//        csi_ctrl_policy[MTK_VENDOR_ATTR_CSI_CTRL_STA_INTERVAL].type = NLA_U32;
        csi_ctrl_policy[MTK_VENDOR_ATTR_CSI_CTRL_DUMP_NUM].type = NLA_U16;
        csi_ctrl_policy[MTK_VENDOR_ATTR_CSI_CTRL_DATA].type = NLA_NESTED;

        csi_data_policy[MTK_VENDOR_ATTR_CSI_DATA_VER].type = NLA_U8;
        csi_data_policy[MTK_VENDOR_ATTR_CSI_DATA_TS].type = NLA_U32;
        csi_data_policy[MTK_VENDOR_ATTR_CSI_DATA_RSSI].type = NLA_U8;
        csi_data_policy[MTK_VENDOR_ATTR_CSI_DATA_SNR].type = NLA_U8;
        csi_data_policy[MTK_VENDOR_ATTR_CSI_DATA_BW].type = NLA_U8;
        csi_data_policy[MTK_VENDOR_ATTR_CSI_DATA_CH_IDX].type = NLA_U8;
        csi_data_policy[MTK_VENDOR_ATTR_CSI_DATA_TA].type = NLA_NESTED;
        csi_data_policy[MTK_VENDOR_ATTR_CSI_DATA_I].type = NLA_NESTED;
        csi_data_policy[MTK_VENDOR_ATTR_CSI_DATA_Q].type = NLA_NESTED;
        csi_data_policy[MTK_VENDOR_ATTR_CSI_DATA_INFO].type = NLA_U32;

        csi_data_policy[MTK_VENDOR_ATTR_CSI_DATA_TX_ANT].type = NLA_U8;
        csi_data_policy[MTK_VENDOR_ATTR_CSI_DATA_RX_ANT].type = NLA_U8;
        csi_data_policy[MTK_VENDOR_ATTR_CSI_DATA_MODE].type = NLA_U8;
        csi_data_policy[MTK_VENDOR_ATTR_CSI_DATA_H_IDX].type = NLA_U32;
    }
    ~MT76APIPrivate()
    {
        if (!csi_list.empty())
        {
            for (auto it : csi_list)
                delete it;
            csi_list.clear();
        }
    }

    static int md_csi_dump_cb(struct nl_msg *msg, void *arg)
    {
        nlattr *tb[NUM_MTK_VENDOR_ATTRS_CSI_CTRL];
        nlattr *tb_data[NUM_MTK_VENDOR_ATTRS_CSI_DATA];
        nlattr *attr;
        nlattr *cur;
        size_t idx;
        int rem;
        csi_data *c;

        attr = unl_find_attr(&unl, msg, NL80211_ATTR_VENDOR_DATA);
        if (!attr)
        {
            fprintf(stderr, "Testdata attribute not found\n");
            return NL_SKIP;
        }

        nla_parse_nested(tb, MTK_VENDOR_ATTR_CSI_CTRL_MAX,
                 attr, csi_ctrl_policy);

        if (!tb[MTK_VENDOR_ATTR_CSI_CTRL_DATA])
            return NL_SKIP;

        nla_parse_nested(tb_data, MTK_VENDOR_ATTR_CSI_DATA_MAX,
                 tb[MTK_VENDOR_ATTR_CSI_CTRL_DATA], csi_data_policy);

        if (!(tb_data[MTK_VENDOR_ATTR_CSI_DATA_VER] &&
              tb_data[MTK_VENDOR_ATTR_CSI_DATA_TS] &&
              tb_data[MTK_VENDOR_ATTR_CSI_DATA_RSSI] &&
              tb_data[MTK_VENDOR_ATTR_CSI_DATA_SNR] &&
              tb_data[MTK_VENDOR_ATTR_CSI_DATA_BW] &&
              tb_data[MTK_VENDOR_ATTR_CSI_DATA_CH_IDX] &&
              tb_data[MTK_VENDOR_ATTR_CSI_DATA_TA] &&
              tb_data[MTK_VENDOR_ATTR_CSI_DATA_I] &&
              tb_data[MTK_VENDOR_ATTR_CSI_DATA_Q] &&
              tb_data[MTK_VENDOR_ATTR_CSI_DATA_INFO] &&
              tb_data[MTK_VENDOR_ATTR_CSI_DATA_MODE] &&
              tb_data[MTK_VENDOR_ATTR_CSI_DATA_H_IDX]))
        {
            fprintf(stderr, "Attributes error for CSI data\n");
            return NL_SKIP;
        }

        c = new csi_data();
        if (!c)
            return -ENOMEM;

        c->rssi = nla_get_u8(tb_data[MTK_VENDOR_ATTR_CSI_DATA_RSSI]);
        c->snr = nla_get_u8(tb_data[MTK_VENDOR_ATTR_CSI_DATA_SNR]);
        c->data_bw = nla_get_u8(tb_data[MTK_VENDOR_ATTR_CSI_DATA_BW]);
        c->pri_ch_idx = nla_get_u8(tb_data[MTK_VENDOR_ATTR_CSI_DATA_CH_IDX]);
        c->rx_mode = nla_get_u8(tb_data[MTK_VENDOR_ATTR_CSI_DATA_MODE]);

        c->tx_idx = nla_get_u16(tb_data[MTK_VENDOR_ATTR_CSI_DATA_TX_ANT]);
        c->rx_idx = nla_get_u16(tb_data[MTK_VENDOR_ATTR_CSI_DATA_RX_ANT]);

        c->ext_info = nla_get_u32(tb_data[MTK_VENDOR_ATTR_CSI_DATA_INFO]);
        c->h_idx = nla_get_u32(tb_data[MTK_VENDOR_ATTR_CSI_DATA_H_IDX]);

        c->ts = nla_get_u32(tb_data[MTK_VENDOR_ATTR_CSI_DATA_TS]);

        idx = 0;
        nla_for_each_nested(cur, tb_data[MTK_VENDOR_ATTR_CSI_DATA_TA], rem)
        {
            if (idx < ETH_ALEN)
                c->ta[idx++] = nla_get_u8(cur);
        }

        idx = 0;
        nla_for_each_nested(cur, tb_data[MTK_VENDOR_ATTR_CSI_DATA_I], rem)
        {
            if (idx < CSI_MAX_COUNT)
                c->data_i[idx++] = nla_get_u16(cur);
        }

        idx = 0;
        nla_for_each_nested(cur, tb_data[MTK_VENDOR_ATTR_CSI_DATA_Q], rem)
        {
            if (idx < CSI_MAX_COUNT)
                c->data_q[idx++] = nla_get_u16(cur);
        }

        csi_list.push_back(c);

        return NL_SKIP;
    }

    std::vector<csi_data *> *motion_detection_dump(const char *wifi, int pkt_num)
    {
        int ret = 0, i;
        struct nl_msg *msg;
        nlattr *data;
        int band, if_idx;

        if_idx = if_nametoindex(wifi);
        if (!if_idx)
        {
            fprintf(stderr, "%s\n", strerror(errno));
            return NULL;
        }

        band = strtoul(wifi + (strlen(wifi) - 1), NULL, 0);

        if (!csi_list.empty())
        {
            for (auto it : csi_list)
                delete it;
            csi_list.clear();
        }

        for (i = 0; i < pkt_num / CSI_DUMP_PER_NUM; i++)
        {
            if (unl_genl_init(&unl, "nl80211") < 0)
            {
                fprintf(stderr, "Failed to connect to nl80211\n");
                return NULL;
            }

            msg = unl_genl_msg(&unl, NL80211_CMD_VENDOR, true);

            if (nla_put_u32(msg, NL80211_ATTR_IFINDEX, if_idx) ||
                nla_put_u32(msg, NL80211_ATTR_VENDOR_ID, MTK_NL80211_VENDOR_ID) ||
                nla_put_u32(msg, NL80211_ATTR_VENDOR_SUBCMD, MTK_NL80211_VENDOR_SUBCMD_CSI_CTRL))
                return NULL;

            data = nla_nest_start(msg, NL80211_ATTR_VENDOR_DATA | NLA_F_NESTED);
            if (!data)
                return NULL;

            if (nla_put_u16(msg, MTK_VENDOR_ATTR_CSI_CTRL_DUMP_NUM, CSI_DUMP_PER_NUM))
                return NULL;

            //nla_put_u8(msg, MTK_VENDOR_ATTR_CSI_CTRL_BAND_IDX, band);

            nla_nest_end(msg, data);

            if (unl_genl_request(&unl, msg, md_csi_dump_cb, NULL))
                fprintf(stderr, "nl80211 call failed: %s\n", strerror(-ret));

            unl_free(&unl);
        }

        return &csi_list;
    }

    int md_csi_set_attr(int band, struct nl_msg *msg, u8 mode, u8 type, u8 v1, u32 v2)
    {
        nlattr *data;
        u8 a[ETH_ALEN];
        int matches, i;

        data = nla_nest_start(msg, MTK_VENDOR_ATTR_CSI_CTRL_CFG | NLA_F_NESTED);
        if (!data)
            return -ENOMEM;

        nla_put_u8(msg, MTK_VENDOR_ATTR_CSI_CTRL_CFG_MODE, mode);
        nla_put_u8(msg, MTK_VENDOR_ATTR_CSI_CTRL_CFG_TYPE, type);
        nla_put_u8(msg, MTK_VENDOR_ATTR_CSI_CTRL_CFG_VAL1, v1);
        nla_put_u32(msg, MTK_VENDOR_ATTR_CSI_CTRL_CFG_VAL2, v2);

        nla_nest_end(msg, data);

        return 0;
    }

    int md_csi_set(int band, int idx, u8 mode, u8 type, u8 v1, u32 v2)
    {
        struct nl_msg *msg;
        nlattr *data;
        int ret;

        fprintf(stderr, "enter md_csi_set()\n");

        if (unl_genl_init(&unl, "nl80211") < 0)
        {
            fprintf(stderr, "Failed to connect to nl80211\n");
            return 2;
        }

        msg = unl_genl_msg(&unl, NL80211_CMD_VENDOR, false);

        if (nla_put_u32(msg, NL80211_ATTR_IFINDEX, idx) ||
            nla_put_u32(msg, NL80211_ATTR_VENDOR_ID, MTK_NL80211_VENDOR_ID) ||
            nla_put_u32(msg, NL80211_ATTR_VENDOR_SUBCMD, MTK_NL80211_VENDOR_SUBCMD_CSI_CTRL))
            return false;

        data = nla_nest_start(msg, NL80211_ATTR_VENDOR_DATA | NLA_F_NESTED);
        if (!data)
            return -ENOMEM;

        md_csi_set_attr(band, msg, mode, type, v1, v2);

        nla_nest_end(msg, data);

        ret = unl_genl_request(&unl, msg, NULL, NULL);
        if (ret)  {
            fprintf(stderr, "nl80211 call failed: %s\n", strerror(-ret));
        }

        unl_free(&unl);

        return ret;
    }

    int motion_detection_start(const char *wifi, u32 interval)
    {
        int band, if_idx;
        int ret = 0;

        if_idx = if_nametoindex(wifi);
        if (!if_idx)
        {
            fprintf(stderr, "%s\n", strerror(errno));
            return 2;
        }

        band = strtoul(wifi + (strlen(wifi) - 1), NULL, 0);

        ret = md_csi_set(band, if_idx, 2, 3, 0, 34);
        if (ret)
        {
            fprintf(stderr, "md start: md_csi_set (QoS data) failed: %s\n", strerror(-ret));
            return ret;
        }
        ret = md_csi_set(band, if_idx, 2, 9, 1, 0);
        if (ret)
        {
            fprintf(stderr, "md start: md_csi_set (data output by event) failed: %s\n", strerror(-ret));
            return ret;
        }

        ret = md_csi_set(band, if_idx, 1, 0, 0, 0);
        if (ret)
        {
            fprintf(stderr, "md start: md_csi_set (csi start) failed: %s\n", strerror(-ret));
            return ret;
        }

        return ret;
    }

    int motion_detection_stop(const char *wifi)
    {
        int band, if_idx;
        int ret = 0;

        if_idx = if_nametoindex(wifi);
        if (!if_idx)
        {
            fprintf(stderr, "%s\n", strerror(errno));
            return 2;
        }

        band = strtoul(wifi + (strlen(wifi) - 1), NULL, 0);

        ret = md_csi_set(band, if_idx, 0, 0, 0, 0);
        if (ret)
        {
            fprintf(stderr, "md stop: md_csi_set (csi stop) failed: %s\n", strerror(-ret));
            return ret;
        }

        return ret;
    }
};

MT76API::MT76API()
{
    d = new MT76APIPrivate();
}

MT76API::~MT76API()
{
    delete d;
}

std::vector<csi_data *> *MT76API::motion_detection_dump(const char *wifi, int pkt_num)
{
    return d->motion_detection_dump(wifi, pkt_num);
}

int MT76API::motion_detection_start(const char *wifi, u32 interval)
{
    return d->motion_detection_start(wifi, interval);
}

int MT76API::motion_detection_stop(const char *wifi)
{
    return d->motion_detection_stop(wifi);
}
