#pragma once

#include <vector>
#include "Aabb.h"
#include "Math/Util.h"

namespace glacier {

class BezierSpline;

class BezierPath {
public:
    struct Point {
        Point() {}
        Point(const Vector3& pos, const Vector3& tan, float len) :
            position(pos),
            tangent(tan),
            cumulative_length(len) 
        {
        }

        Vector3 position;
        Vector3 tangent;
        Vector3 normal;
        float time = 0;
        float cumulative_length = 0;
    };

    struct TimeOnCurve {
        int pre_index;
        int next_index;
        float time;
    };

    // split by angle error & min dist
    BezierPath(const BezierSpline& spline, float max_angle_error, float min_vert_dist, float accuracy);
    // split evenly
    BezierPath(const BezierSpline& spline, float spacing, float accuracy);
    BezierPath(BezierPath&& other) = default;

    bool IsClosed() const { return is_closed_; }
    const std::vector<Point>& GetPoints() const { return points_; }
    float Length() const { return points_.back().cumulative_length; }
    const Vector3& up() const { return Vector3::up; }

    Vector3 GetPosition(int index) const { return points_[index].position; }
    Vector3 GetNormal(int index) const { return points_[index].normal; }
    Vector3 GetTangent(int index) const { return points_[index].tangent; }
    float GetTime(int index) const { return points_[index].time; }

    Vector3 GetNormal(float t, bool loop = false) const;
    Vector3 GetTangent(float t, bool loop = false) const;
    Vector3 GetPoint(float t, bool loop = false) const;

private:
    void CalcNormal(const BezierSpline& spline);
    TimeOnCurve CalcTimeOnCurve(float time, bool loop) const;

    bool is_closed_;
    Vector3 up_;
    std::vector<Point> points_;
    std::vector<int> anchor_index_map_;
    AABB bounds_;
};

}
