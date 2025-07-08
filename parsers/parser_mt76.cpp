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
        int num_subcarriers = CSI_BW20_DATA_COUNT; // Default value
        
        csi_data *csi = list->at(it);
        if (csi)
        {
            //fprintf(stderr, "\tprocessRawData() csi->data_num: %d\n", csi->data_num);
            
            // Determine number of subcarriers based on bandwidth
            switch (csi->ch_bw) {
                case 0: num_subcarriers = CSI_BW20_DATA_COUNT; break;   // 20MHz
                case 1: num_subcarriers = CSI_BW40_DATA_COUNT; break;   // 40MHz
                case 2: num_subcarriers = CSI_BW80_DATA_COUNT; break;   // 80MHz
                case 3: num_subcarriers = CSI_BW160_DATA_COUNT; break;  // 160MHz
                default: num_subcarriers = CSI_BW20_DATA_COUNT; break;  // Default to 20MHz
            }
            
            //fprintf(stderr, "\tprocessRawData() ch_bw: %d, num_subcarriers: %d\n", csi->ch_bw, num_subcarriers);
            
            // Process all subcarriers for this bandwidth
            for (size_t i = 0; i < num_subcarriers; i++)
            {
                std::complex<double> csi_complex(csi->data_i[i], csi->data_q[i]);
                csi_per_antenna[csi->rx_idx][i] = csi_complex;
                //fprintf(stderr, "\tprocessRawData() csi_data[%d]->i: 0x%x, csi_data[%d]->q: 0x%x\n", i, csi->data_i[i], i, csi->data_q[i]);
            }

            if (csi->rx_idx == antIdx)
            {
                for (int i = 0; i < num_subcarriers; i++) {
                    csi_antenna_real[i] = std::abs(csi_per_antenna[antIdx][i]);
                }

                std::vector<double> arr(csi_antenna_real.begin(), csi_antenna_real.begin() + num_subcarriers);

                tones_per_packet[antIdx].push_back(arr);
            }
            
            //fprintf(stderr, "\tprocessRawData() processed %d subcarriers\n", num_subcarriers);
        }
    }


    return tones_per_packet[antIdx];
}
