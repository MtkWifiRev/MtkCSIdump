#pragma once

#include "motion_estimator.h"

class KurtosisMotionEstimator : public MotionEstimator
{
private:
    double standardDeviation(std::vector<double> & H);
    double variance(std::vector<double> & H);
    double mean(std::vector<double> & H);
    double cv(std::vector<double> & H);
    double calculateMotionFromKurtosis(std::vector<double> &kurtosis);

public:
    double calculate(std::vector<std::vector<double>> data_dump) override;
};
