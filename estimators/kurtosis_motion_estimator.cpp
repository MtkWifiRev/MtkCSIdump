#include "kurtosis_motion_estimator.h"

#include <iostream>
#include <cmath>
#include <complex>
#include <array>
#include <cstddef>
#include <algorithm>
#include <math.h>

#define KURTOSIS_OUTLIER 1.3

double DEVIATION_MIN = 0.02;
double DEVIATION_MAX = 0.05;

double KurtosisMotionEstimator::calculate(std::vector<std::vector<double>> data_dump)
{
    std::vector<double> kurtosis;

    for (auto& dump : data_dump)
    {
	double kurt = cv(dump);

	if (kurt < KURTOSIS_OUTLIER)
	{
	    std::cout << "Kurtosis: " << kurt << std::endl;
	    kurtosis.push_back(kurt);
	}
    }

    return calculateMotionFromKurtosis(kurtosis);
}

double KurtosisMotionEstimator::standardDeviation(std::vector<double> & H)
{
     return sqrt(variance(H));
}

double KurtosisMotionEstimator::variance(std::vector<double> & H)
{
     double t;
     int size = H.size();
     double variance = 0.0;

     if (size > 1)
     {
         t = H[0];
         for (int i = 1; i < size; i++)
         {
              t += H[i];
              double diff = ((i + 1.0) * H[i]) - t;
              variance += (diff * diff) / ((i + 1.0) *i);
         }

         return variance / (size - 1.0);
     }
     return 0.0;
}

double KurtosisMotionEstimator::cv(std::vector<double> & H)
{
    return standardDeviation(H) / mean(H);
}

double KurtosisMotionEstimator::mean(std::vector<double> & H)
{
    double sum = 0.0;
    for (auto& i: H)
        sum += i;

    return sum / H.size();
}

double KurtosisMotionEstimator::calculateMotionFromKurtosis(std::vector<double> &kurtosis)
{
    double kurtosis_std = standardDeviation(kurtosis), motion_percentage;

    if (kurtosis_std <= DEVIATION_MIN)
        motion_percentage = 0;
    else if (kurtosis_std >= DEVIATION_MAX + DEVIATION_MIN)
        motion_percentage = 100;
    else
        motion_percentage = round(((kurtosis_std - DEVIATION_MIN) * 100 / DEVIATION_MAX) * 10) / 10;

    std::cout << "kurtosis deviation: " << kurtosis_std << std::endl;

    return motion_percentage;
}
