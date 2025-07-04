#include "parser_mt76.h"

#include "wifi_drv_api/mt76_api.h"

#include <vector>

#define CSI_MAX_COUNT 256

//TODO rewrite, there is no "data_num"
std::vector<std::vector<double>> ParserMT76::processRawData(void *data, int antIdx)
{
    std::vector<std::vector<std::complex<double>>> csi_per_antenna(ANTENNA_NUM, std::vector<std::complex<double>>(CSI_BW160_DATA_COUNT));
    std::vector<double> csi_antenna_real(std::vector<double>(CSI_BW160_DATA_COUNT));
    std::vector<csi_data *> *list = (std::vector<csi_data *>*)data;
    //static std::vector<std::vector<double>> tones_per_packet[3];
    std::vector<std::vector<double>> tones_per_packet[3];

    for (int it = 0; it < list->size(); it++)
    {
        int recv_i_q_num = 0;

        csi_data *csi = list->at(it);
        if (csi)
        {
            fprintf(stderr, "\tprocessRawData() csi->data_num: %d\n", csi->data_num);
            //for (size_t i = 0, j = 0; i < csi->data_num; i++)
            for (size_t i = 0, j = 0; i < CSI_MAX_COUNT; i++)
            {
                if(csi->data_q[i] != 0 && csi->data_i[i] != 0) {
                    csi_per_antenna[csi->rx_idx][j++] = std::complex<double>(csi->data_q[i], csi->data_i[i]);
                    fprintf(stderr, "\tprocessRawData() csi_data[%d]->i: 0x%x, csi_data[%d]->q: 0x%x\n", i, csi->data_i[i], i, csi->data_q[i]);
                    recv_i_q_num++;
                }
            }

            if (csi->rx_idx == antIdx)
            {
                for (int i = 0; i < csi_per_antenna[antIdx].size(); i++) {
                    csi_antenna_real[i] = std::abs(csi_per_antenna[antIdx][i]);
                }

                //if (tones_per_packet[antIdx].size() == 5)
                //    tones_per_packet[antIdx].clear();

                //std::vector<double> arr(csi_antenna_real.begin(), csi_antenna_real.begin() + csi->data_num);
                std::vector<double> arr(csi_antenna_real.begin(), csi_antenna_real.begin() + recv_i_q_num);

                tones_per_packet[antIdx].push_back(arr);
            }
        }
        fprintf(stderr, "\tprocessRawData() got %d non-zero I/Q\n", recv_i_q_num);
    }


    //if (tones_per_packet[antIdx].size() == 5)
    return tones_per_packet[antIdx];

    //return {};
}
