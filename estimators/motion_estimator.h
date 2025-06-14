#pragma once

#include <vector>

class MotionEstimator
{
public:
    virtual double calculate(std::vector<std::vector<double>> data_dump) = 0;
};
