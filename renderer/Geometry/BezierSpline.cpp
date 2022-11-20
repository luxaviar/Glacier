#include <limits>
#include <math.h>
#include "BezierSpline.h"

namespace glacier {

BezierSpline::Curve::Curve(const Vector3& p0, const Vector3& p1, const Vector3& p2, const Vector3& p3) :
    points{p0, p1, p2, p3}
{
    
}

/// Times of stationary points on curve (points where derivative is zero on any axis)
void BezierSpline::Curve::CalcExtremePointTimes() {
    if (num_extreme_point >= 0) return;

    // coefficients of derivative function
    Vector3 a = 3.0f * (-points[0] + 3.0f * points[1] - 3.0f * points[2] + points[3]);
    Vector3 b = 6.0f * (points[0] - 2.0f * points[1] + points[2]);
    Vector3 c = 3.0f * (points[1] - points[0]);

    StationaryPointTimes(a.x, b.x, c.x);
    StationaryPointTimes(a.y, b.y, c.y);
    StationaryPointTimes(a.z, b.z, c.z);
}

// Finds times of stationary points on curve defined by ax^2 + bx + c.
// Only times between 0 and 1 are considered as Bezier only uses values in that range
void BezierSpline::Curve::StationaryPointTimes(float a, float b, float c) {
    // from quadratic equation: y = [-b +- sqrt(b^2 - 4ac)]/2a
    if (a == 0) return;

    float discriminant = b * b - 4 * a * c;
    if (discriminant >= 0) {
        float s = math::Sqrt(discriminant);
        float t1 = (-b + s) / (2 * a);
        if (t1 >= 0 && t1 <= 1) {
            extreme_times[num_extreme_point++] = t1;
        }

        if (discriminant != 0) {
            float t2 = (-b - s) / (2 * a);
            if (t2 >= 0 && t2 <= 1) {
                extreme_times[num_extreme_point++] = t2;
            }
        }
    }
}

Vector3 BezierSpline::Curve::EvaluatePoint(float t) const {
    t = math::Saturate(t);
    return (1 - t) * (1 - t) * (1 - t) * points[0] + 3 * (1 - t) * (1 - t) * t * points[1]+ 3 * (1 - t) * t * t * points[2]+ t * t * t * points[3];
}

Vector3 BezierSpline::Curve::EvaluateDerivative(float t) const {
    t = math::Saturate(t);
    return 3 * (1 - t) * (1 - t) * (points[1] - points[0]) + 6 * (1 - t) * t * (points[2] - points[1]) + 3 * t * t * (points[3] - points[2]);
}

Vector3 BezierSpline::Curve::EvaluateSecondDerivative(float t) const {
    t = math::Saturate(t);
    return 6 * (1 - t) * (points[2] - 2.0f * points[1] + points[0]) + 6 * t * (points[3] - 2.0f * points[2] + points[1]);
}

Vector3 BezierSpline::Curve::EvaluateNormal(float t) const {
    Vector3 tangent = EvaluateDerivative(t);
    Vector3 next_tangent = EvaluateSecondDerivative(t);
    Vector3 c = next_tangent.Cross(tangent);
    return c.Cross(tangent).Normalized();
}

AABB BezierSpline::Curve::CalcBounds() {
    AABB bounds;
    bounds.AddPoint(points[0]);
    bounds.AddPoint(points[3]);

    CalcExtremePointTimes();

    for (int i = 0; i < num_extreme_point; ++i) {
        auto p = EvaluatePoint(extreme_times[i]);
        bounds.AddPoint(p);
    }

    return bounds;
}

float BezierSpline::Curve::EstimateLength() {
    float control_length = (points[0] - points[1]).Magnitude() + (points[1] - points[2]).Magnitude() + (points[2] - points[3]).Magnitude();
    float estimated_length = (points[0]- points[3]).Magnitude() + control_length / 2.0f;
    return estimated_length;
}

std::pair<BezierSpline::Curve, BezierSpline::Curve> BezierSpline::Curve::Split(float t) {
    Vector3 a1 = Vector3::Lerp(points[0], points[1], t);
    Vector3 a2 = Vector3::Lerp(points[1], points[2], t);
    Vector3 a3 = Vector3::Lerp(points[2], points[3], t);
    Vector3 b1 = Vector3::Lerp(a1, a2, t);
    Vector3 b2 = Vector3::Lerp(a2, a3, t);
    Vector3 split_point = Vector3::Lerp(b1, b2, t);

    return { {points[0], a1, b1, split_point }, { split_point, b2, a3, points[3] } };
}

BezierSpline::BezierSpline(const Vector3& centre, ControlMode mode, bool is_closed) :
    is_closed_(is_closed),
    mode_(mode)
{
    Vector3 dir = Vector3::forward;
    constexpr float width = 2;
    constexpr float controlHeight = 0.5f;
    constexpr float controlWidth = 1.0f;

    points_.push_back(centre + Vector3::left * width);
    points_.push_back(centre + Vector3::left * controlWidth + dir * controlHeight);
    points_.push_back(centre + Vector3::right * controlWidth - dir * controlHeight);
    points_.push_back(centre + Vector3::right * width);

    anchor_normals_angle_.push_back(0);
    anchor_normals_angle_.push_back(0);

    if (is_closed_) {
        UpdateClosedState();
    }
}

BezierSpline::BezierSpline(const std::vector<Vector3>& anchors, bool is_closed) :
    mode_(ControlMode::kAutomatic),
    points_({anchors[0], Vector3::zero, Vector3::zero, anchors[1]}),
    anchor_normals_angle_({0, 0})
{
    assert(anchors.size() >= 2);

    for (int i = 2; i < anchors.size(); i++) {
        AddSegmentToEnd(anchors[i]);
        anchor_normals_angle_.push_back(0);
    }

    is_closed_ = is_closed;

    if (is_closed_) {
        UpdateClosedState();
    }
    else if (anchors.size() == 2) {
        AutoSetStartAndEndControls();
    }
}

BezierSpline::BezierSpline(const Vector3& start_anchor, const Vector3& start_control,
    const Vector3& end_control, const Vector3& end_anchor) :
    mode_(ControlMode::kFree),
    is_closed_(false),
    points_({ start_anchor, start_control, end_control, end_anchor }),
    anchor_normals_angle_({ 0, 0 })
{
    
}

BezierPath BezierSpline::GenCurveBySpaceEvenly(float spacing, float accuracy) const {
    return BezierPath(*this, spacing, accuracy);
}

BezierPath BezierSpline::GenCurveByAngleError(float max_angle_error, float min_vert_dist, float accuracy) const {
    return BezierPath(*this, max_angle_error, min_vert_dist, accuracy);
}

BezierSpline::Curve BezierSpline::GetSegment(size_t segment_index) const {
    segment_index = math::Clamp<size_t>(segment_index, 0, NumSegments() - 1);
    return Curve{ points_[segment_index * 3], points_[segment_index * 3 + 1], points_[segment_index * 3 + 2], points_[LoopIndex(segment_index * 3 + 3)] };
}

AABB BezierSpline::CalcBounds() const {
    // Loop through all segments and keep track of the minmax points of all their bounding boxes
    AABB bounds;

    for (int i = 0; i < NumSegments(); i++) {
        auto segment = GetSegment(i);

        bounds.AddPoint(segment.points[0]);
        bounds.AddPoint(segment.points[3]);

        segment.CalcExtremePointTimes();

        for (int i = 0; i < segment.num_extreme_point; ++i) {
            auto p = segment.EvaluatePoint(segment.extreme_times[i]);
            bounds.AddPoint(p);
        }
    }

    return bounds;
}

void BezierSpline::AddSegmentToEnd(const Vector3& new_anchor) {
    if (is_closed_) {
        return;
    }

    auto last_anchor = points_.back();
    auto last_control = points_[points_.size() - 2];
    // Set position for new control to be mirror of its counterpart
    Vector3 second_control_offset = (last_anchor - last_control);
    if (mode_ != ControlMode::kMirrored && mode_ != ControlMode::kAutomatic) {
        // Set position for new control to be aligned with its counterpart, but with a length of half the distance from prev to new anchor
        float anchor_dist = (last_anchor - new_anchor).Magnitude();
        second_control_offset = (last_anchor - last_control).Normalized() * anchor_dist * .5f;
    }
    Vector3 second_control = last_anchor + second_control_offset;
    Vector3 new_control = (new_anchor + second_control) * 0.5f;

    points_.insert(points_.end(), { second_control, new_control, new_anchor });
    anchor_normals_angle_.push_back(anchor_normals_angle_.back());

    if (mode_ == ControlMode::kAutomatic) {
        AutoSetAllAffectedControlPoints(points_.size() - 1);
    }
}

/// Add new anchor point to start of the path
void BezierSpline::AddSegmentToStart(const Vector3& new_anchor) {
    if (is_closed_) {
        return;
    }

    auto first_anchor = points_.front();
    // Set position for new control to be mirror of its counterpart
    Vector3 second_control_offset = (first_anchor - points_[1]);
    if (mode_ != ControlMode::kMirrored && mode_ != ControlMode::kAutomatic) {
        // Set position for new control to be aligned with its counterpart, but with a length of half the distance from prev to new anchor
        float anchor_dist = (first_anchor - new_anchor).Magnitude();
        second_control_offset = second_control_offset.Normalized() * anchor_dist * .5f;
    }
    Vector3 second_control = first_anchor + second_control_offset;
    Vector3 new_control = (new_anchor + second_control) * .5f;
    points_.insert(points_.begin(), { new_anchor, new_control, second_control });
    anchor_normals_angle_.insert(anchor_normals_angle_.begin(), anchor_normals_angle_[0]);

    if (mode_ == ControlMode::kAutomatic) {
        AutoSetAllAffectedControlPoints(0);
    }
}

/// Insert new anchor point at given position. Automatically place control points around it so as to keep shape of curve the same
void BezierSpline::SplitSegment(const Vector3& new_anchor, int segment_index, float split_time) {
    split_time = math::Saturate(split_time);

    if (mode_ == ControlMode::kAutomatic) {
        points_.insert(points_.begin() + segment_index * 3 + 2, { Vector3::zero, new_anchor, Vector3::zero });
        AutoSetAllAffectedControlPoints(segment_index * 3 + 3);
    } else {
        // Split the curve to find where control points can be inserted to least affect shape of curve
        // Curve will probably be deformed slightly since splitTime is only an estimate (for performance reasons, and so doesn't correspond exactly with anchorPos)
        auto segment = GetSegment(segment_index);
        auto split_segment_pair = segment.Split(split_time);
        points_.insert(points_.begin() + segment_index * 3 + 2, { split_segment_pair.first[2], split_segment_pair.second[0], split_segment_pair.second[1] });
        int new_anchor_index = segment_index * 3 + 3;
        MovePoint(new_anchor_index - 2, split_segment_pair.first[1]);
        MovePoint(new_anchor_index + 2, split_segment_pair.second[2]);
        MovePoint(new_anchor_index, new_anchor);

        if (mode_ == ControlMode::kMirrored) {
            float avgDst = ((split_segment_pair.first[2] - new_anchor).Magnitude() + (split_segment_pair.second[1] - new_anchor).Magnitude()) / 2;
            MovePoint(new_anchor_index + 1, new_anchor + (split_segment_pair.second[1] - new_anchor).Normalized() * avgDst);
        }
    }
    
    // Insert angle for new anchor (value should be set inbetween neighbour anchor angles)
    int new_anchor_angle_index = (segment_index + 1) % anchor_normals_angle_.size();
    float prev_angle = anchor_normals_angle_[segment_index];
    float next_angle = anchor_normals_angle_[new_anchor_angle_index];
    float split_angle = math::LerpAngle(prev_angle, next_angle, split_time);
    anchor_normals_angle_.insert(anchor_normals_angle_.begin() + new_anchor_angle_index, split_angle);
}

/// Delete the anchor point at given index, as well as its associated control points
void BezierSpline::DeleteSegment(int anchor_index) {
    // Don't delete segment if its the last one remaining (or if only two segments in a closed path)
    if (NumSegments() > 2 || (!is_closed_ && NumSegments() > 1)) {
        if (anchor_index == 0) {
            if (is_closed_) {
                points_[points_.size() - 1] = points_[2]; //connect last anchor to the second anchor
            }
            //erase the first anchor and tow control points right after it
            points_.erase(points_.begin(), points_.begin() + 3);
        }
        else if (anchor_index == points_.size() - 1 && !is_closed_) {
            //erase the last anchor and tow control points right before it
            points_.erase(points_.begin() + anchor_index - 2, points_.begin() + anchor_index - 2 + 3);
        }
        else {
            points_.erase(points_.begin() + anchor_index - 1, points_.begin() + anchor_index - 1 + 3);
        }

        anchor_normals_angle_.erase(anchor_normals_angle_.begin() + anchor_index / 3);

        if (mode_ == ControlMode::kAutomatic) {
            AutoSetAllControlPoints();
        }
    }
}

/// Determines good positions (for a smooth path) for the control points affected by a moved/inserted anchor point
void BezierSpline::AutoSetAllAffectedControlPoints(size_t anchor_index) {
    for (int i = (int)anchor_index - 3; i <= (int)anchor_index + 3; i += 3) {
        if ((i >= 0 && i < points_.size()) || is_closed_) {
            AutoSetAnchorControlPoints(LoopIndex(i));
        }
    }

    AutoSetStartAndEndControls();
}

/// Calculates good positions (to result in smooth path) for the controls around specified anchor
void BezierSpline::AutoSetAnchorControlPoints(size_t anchor_index) {
    // Calculate a vector that is perpendicular to the vector bisecting the angle between this anchor and its two immediate neighbours
    // The control points will be placed along that vector
    Vector3 anchor_pos = points_[anchor_index];
    Vector3 dir = Vector3::zero;
    float neighbour_dist[2];

    if (anchor_index - 3 >= 0 || is_closed_) {
        Vector3 offset = points_[LoopIndex(anchor_index - 3)] - anchor_pos;
        dir += offset.Normalized();
        neighbour_dist[0] = offset.Magnitude();
    }
    if (anchor_index + 3 >= 0 || is_closed_) {
        Vector3 offset = points_[LoopIndex(anchor_index + 3)] - anchor_pos;
        dir -= offset.Normalized();
        neighbour_dist[1] = -offset.Magnitude();
    }

    dir.Normalize();

    // Set the control points along the calculated direction, with a distance proportional to the distance to the neighbouring control point
    for (int i = 0; i < 2; i++) {
        int control_index = (int)anchor_index + i * 2 - 1;
        if ((control_index >= 0 && control_index < points_.size()) || is_closed_) {
            points_[LoopIndex(control_index)] = anchor_pos + dir * neighbour_dist[i] * auto_control_length_;
        }
    }
}

/// Determines good positions (for a smooth path) for the control points at the start and end of a path
void BezierSpline::AutoSetStartAndEndControls() {
    if (is_closed_) {
        // Handle case with only 2 anchor points separately, as will otherwise result in straight line ()
        if (NumAnchorPoints() == 2) {
            Vector3 anchor_dir = (points_[3] - points_[0]).Normalized();
            float anchor_dist = (points_[0] - points_[3]).Magnitude();
            Vector3 perp = anchor_dir.Cross(Vector3::up);
            points_[1] = points_[0] + perp * anchor_dist / 2.0f;
            points_[5] = points_[0] - perp * anchor_dist / 2.0f;
            points_[2] = points_[3] + perp * anchor_dist / 2.0f;
            points_[4] = points_[3] - perp * anchor_dist / 2.0f;

        }
        else {
            AutoSetAnchorControlPoints(0);
            AutoSetAnchorControlPoints(points_.size() - 3);
        }
    }
    else {
        // Handle case with 2 anchor points separately, as otherwise minor adjustments cause path to constantly flip
        if (NumAnchorPoints() == 2) {
            points_[1] = points_[0] + (points_[3] - points_[0]) * .25f;
            points_[2] = points_[3] + (points_[0] - points_[3]) * .25f;
        }
        else {
            points_[1] = (points_[0] + points_[2]) * .5f;
            points_[points_.size() - 2] = (points_[points_.size() - 1] + points_[points_.size() - 3]) * .5f;
        }
    }
}

/// Determines good positions (for a smooth path) for all control points
void BezierSpline::AutoSetAllControlPoints() {
    if (NumAnchorPoints() > 2) {
        for (int i = 0; i < points_.size(); i += 3) {
            AutoSetAnchorControlPoints(i);
        }
    }

    AutoSetStartAndEndControls();
}

void BezierSpline::UpdateClosedState() {
    if (IsClosed()) {
        auto last_anchor = points_.back();
        auto first_anchor = points_.front();
        // Set positions for new controls to mirror their counterparts
        Vector3 last_anchor_second_control = last_anchor * 2 - points_[points_.size() - 2];
        Vector3 first_anchor_second_control = first_anchor * 2 - points_[1];
        if (mode_ != ControlMode::kMirrored && mode_ != ControlMode::kAutomatic) {
            // Set positions for new controls to be aligned with their counterparts, but with a length of half the distance between start/end anchor
            float anchor_dist = (last_anchor - first_anchor).Magnitude();
            last_anchor_second_control = last_anchor + (last_anchor - points_[points_.size() - 2]).Normalized() * anchor_dist * .5f;
            first_anchor_second_control = first_anchor + (first_anchor - points_[1]).Normalized() * anchor_dist * .5f;
        }
        points_.push_back(last_anchor_second_control);
        points_.push_back(first_anchor_second_control);
    }
    else {
        points_.pop_back();
        points_.pop_back();
    }

    if (mode_ == ControlMode::kAutomatic) {
        AutoSetStartAndEndControls();
    }
}

void BezierSpline::MovePoint(int i, Vector3 new_pos) {
    bool is_anchor = IsAnchor(i);

    // Don't process control point if control mode is set to automatic
    if (!is_anchor && mode_ == ControlMode::kAutomatic) return;

    Vector3 delta_move = new_pos - points_[i];
    points_[i] = new_pos;

    if (mode_ == ControlMode::kAutomatic) {
        AutoSetAllAffectedControlPoints(i);
        return;
    }
    
    // Move control points with anchor point
    if (is_anchor) {
        if (i + 1 < points_.size() || IsClosed()) {
            points_[LoopIndex(i + 1)] += delta_move;
        }
        if (i - 1 >= 0 || IsClosed()) {
            points_[LoopIndex(i - 1)] += delta_move;
        }
    }
    // If not in free control mode, then move attached control point to be aligned/mirrored (depending on mode)
    else if (mode_ != ControlMode::kFree) {
        bool next_is_anchor = IsAnchor(i + 1);
        int neighbour_control = (next_is_anchor) ? i + 2 : i - 2;
        int anchor_index = (next_is_anchor) ? i + 1 : i - 1;

        if ((neighbour_control >= 0 && neighbour_control < points_.size()) || IsClosed()) {
            float movement = 0;
            // If in aligned mode, then attached control's current distance from anchor point should be maintained
            if (mode_ == ControlMode::kAligned) {
                movement = (points_[LoopIndex(anchor_index)] - points_[LoopIndex(neighbour_control)]).Magnitude();
            }
            // If in mirrored mode, then both control points should have the same distance from the anchor point
            else if (mode_ == ControlMode::kMirrored) {
                movement = (points_[LoopIndex(anchor_index)] - points_[i]).Magnitude();
            }
            Vector3 dir = (points_[LoopIndex(anchor_index)] - new_pos).Normalized();
            points_[LoopIndex(neighbour_control)] = points_[LoopIndex(anchor_index)] + dir * movement;
        }
    }
}

}

