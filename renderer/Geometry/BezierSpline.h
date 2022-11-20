#pragma once

#include <vector>
#include "Aabb.h"
#include "Math/Util.h"
#include "BezierPath.h"

namespace glacier {

class BezierSpline {
public:
    static constexpr int kNumSegmentPoints = 4;

    enum class ControlMode : uint8_t {
        kAligned,
        kMirrored,
        kFree,
        kAutomatic
    };

    struct Curve {
        Curve(const Vector3& p0, const Vector3& p1, const Vector3& p2, const Vector3& p3);

        void CalcExtremePointTimes();
        AABB CalcBounds();

        Vector3 EvaluatePoint(float t) const;
        Vector3 EvaluateDerivative(float t) const;
        Vector3 EvaluateSecondDerivative(float t) const;
        float EstimateLength();

        Vector3 EvaluateNormal(float t) const;
        std::pair<Curve, Curve> Split(float t);

        Vector3 operator[](int index) const {
            return points[index];
        }

        Vector3 points[kNumSegmentPoints]; // anchor1 control1 control2 anchor2
        float extreme_times[6]; //2 roots x 3 axis
        int num_extreme_point = -1;

    private:
        void StationaryPointTimes(float a, float b, float c);
    };

    BezierSpline(const Vector3& centre, ControlMode mode = ControlMode::kAutomatic, bool is_closed = false);
    //Automatic
    BezierSpline(const std::vector<Vector3>& anchors, bool is_closed = false);
    //Free
    BezierSpline(const Vector3& start_anchor, const Vector3& start_control, const Vector3& end_control, const Vector3& end_anchor);

    BezierPath GenCurveBySpaceEvenly(float spacing = 0.5f, float accuracy = 10.0f) const;
    BezierPath GenCurveByAngleError(float max_angle_error = 0.3f, float min_vert_dist = 0.0f, float accuracy = 10.0f) const;

    Curve GetSegment(size_t segmentIndex) const;
    AABB CalcBounds() const;

    ControlMode mode() const { return mode_; }
    float global_normal_angle() const { return global_normal_angle_; }
    void global_normal_angle(float angle) { global_normal_angle_ = angle; }

    void auto_control_length(float len) { auto_control_length_ = math::Saturate(len); }
    float auto_control_length() const { return auto_control_length_; }

    bool IsClosed() const { return is_closed_; }
    bool IsAnchor(size_t i) const { return i % 3 == 0; }
    size_t LoopIndex(size_t i) const { return (i + points_.size()) % points_.size(); }
    size_t NumAnchorPoints() const { return is_closed_ ? points_.size() / 3 : (points_.size() + 2) / 3; }
    size_t NumSegments() const { return points_.size() / 3; }
    float GetAnchorNormalAngle(int anchor_index) const { return math::Fmod(anchor_normals_angle_[anchor_index], 360.0f); }

    void AddSegmentToEnd(const Vector3& new_anchor);
    void AddSegmentToStart(const Vector3& new_anchor);
    void SplitSegment(const Vector3& anchor, int segment_index, float split_time);
    void DeleteSegment(int anchor_index);
    void MovePoint(int i, Vector3 pos);

    Vector3 operator[](int index) const {
        return points_[index];
    }

private:
    void AutoSetStartAndEndControls();
    void AutoSetAllControlPoints();
    void AutoSetAllAffectedControlPoints(size_t anchor_index);
    void AutoSetAnchorControlPoints(size_t anchor_index);
    void UpdateClosedState();

    bool is_closed_ = false;
    ControlMode mode_ = ControlMode::kAutomatic;
    float auto_control_length_ = 0.3f;
    float global_normal_angle_ = 0.0f;
    std::vector<Vector3> points_;
    std::vector<float> anchor_normals_angle_;
};

}
