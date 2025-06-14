#pragma once

#include <string>
#include <vector>
#include <algorithm>
#include <math.h>
#include <complex.h>

class Parser
{
protected:
    size_t packets_to_read = 0;

public:
    virtual std::vector<std::vector<double>> processRawData(void *data, int antIdx) = 0;
};
