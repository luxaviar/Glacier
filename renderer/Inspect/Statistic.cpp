#include "statistic.h"

namespace glacier {

Statistic::Statistic() :
    total_(0.0),
    num_samples_(0.0),
    min_(std::numeric_limits<double>::max()),
    max_(std::numeric_limits<double>::lowest())
{
}

void Statistic::Reset() {
    total_ = 0.0;
    num_samples_ = 0.0;
    min_ = std::numeric_limits<double>::max();
    max_ = std::numeric_limits<double>::lowest();
}

void Statistic::Sample(double value) {
    total_ += value;
    num_samples_++;
    if (value > max_) {
        max_ = value;
    }

    if (value < min_) {
        min_ = value;
    }
}

}
