#include "parser_mt76.h"

#include "wifi_drv_api/mt76_api.h"

#include <vector>

std::vector<std::vector<double>> ParserMT76::processRawData(void *data, int antIdx)
{
    std::vector<std::vector<std::complex<double>>> csi_per_antenna(ANTENNA_NUM,
        std::vector<std::complex<double>>(CSI_BW160_DATA_COUNT));
    std::vector<double> csi_antenna_real(std::vector<double>(CSI_BW160_DATA_COUNT));
    std::vector<csi_data *> *list = (std::vector<csi_data *>*)data;
    static std::vector<std::vector<double>> tones_per_packet[3];

    for (int it = 0; it < list->size(); it++)
    {
        csi_data *csi = list->at(it);
        if (csi)
        {
            for (size_t i = 0, j = 0; i < csi->data_num; i++)
            {
                csi_per_antenna[csi->rx_idx][j++] = std::complex<double>(
                    csi->data_q[i], csi->data_i[i]);
            }

            if (csi->rx_idx == antIdx)
            {
                for (int i = 0; i < csi_per_antenna[antIdx].size(); i++)
                    csi_antenna_real[i] = std::abs(csi_per_antenna[antIdx][i]);

                if (tones_per_packet[antIdx].size() == 5)
                    tones_per_packet[antIdx].clear();

                std::vector<double> arr(csi_antenna_real.begin(),
                    csi_antenna_real.begin() + csi->data_num);

                tones_per_packet[antIdx].push_back(arr);
            }
        }
    }

    if (tones_per_packet[antIdx].size() == 5)
        return tones_per_packet[antIdx];

    return {};
}
