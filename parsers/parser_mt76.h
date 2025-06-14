#pragma once

#include "parser.h"
#include <cstdint>

#define ANTENNA_NUM 3

class ParserMT76 final : public Parser
{
private:
    std::vector<int> cleanupDump(const std::string &data);
public:
    virtual std::vector<std::vector<double>> processRawData(void *data, int antIdx) override;
};
