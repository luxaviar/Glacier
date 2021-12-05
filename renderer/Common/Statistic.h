#pragma once

namespace glacier {

class Statistic
{
public:
    Statistic();
    void Reset();

    void Sample(double value);

    double samples() const { return num_samples_; }
    double min() const { return min_; }
    double max() const { return max_; }

    double average() {
        if (num_samples_ > 0.0) {
            return total_ / num_samples_;
        }

        return 0.0;
    }

private:
    double total_;
    double num_samples_;
    double min_;
    double max_;
};

}
