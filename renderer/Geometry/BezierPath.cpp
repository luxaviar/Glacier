#include <limits>
#include <math.h>
#include "BezierPath.h"
#include "BezierSpline.h"

namespace glacier {

BezierPath::BezierPath(const BezierSpline& spline, float max_angle_error, float min_vert_dist, float accuracy) {
    is_closed_ = spline.IsClosed();
    BezierSpline::Curve segment = spline.GetSegment(0);

    points_.emplace_back(spline[0], segment.EvaluateDerivative(0).Normalized(), 0);

    anchor_index_map_.push_back(0);
    bounds_.AddPoint(spline[0]);

    Vector3 pre_test_point = spline[0];
    Vector3 last_pass_point = spline[0];

    float path_length = 0;
    float vert_dist = 0;

    // Go through all segments and split up into vertices
    for (int segment_index = 0; segment_index < spline.NumSegments(); segment_index++)
    {
        BezierSpline::Curve segment = spline.GetSegment(segment_index);
        float estimated_length = segment.EstimateLength();
        int divisions = math::Ceil(estimated_length * accuracy);
        float increment = 1.0f / divisions;

        for (float t = increment; t <= 1; t += increment)
        {
            bool is_last_point = (t + increment > 1 && segment_index == spline.NumSegments() - 1);
            if (is_last_point)
            {
                t = 1;
            }
            Vector3 cur_test_point = segment.EvaluatePoint(t);
            Vector3 next_test_point = segment.EvaluatePoint(t + increment);

            // angle at current point on path
            float local_angle = 180.0f - Vector3::MinAngle(pre_test_point, cur_test_point, next_test_point);
            // angle between the last added vertex, the current point on the path, and the next point on the path
            float pass_angle = 180 - Vector3::MinAngle(last_pass_point, cur_test_point, next_test_point);
            float angle_error = math::Max(local_angle, pass_angle);

            if ((angle_error > max_angle_error && vert_dist >= min_vert_dist) || is_last_point)
            {
                path_length += (last_pass_point - cur_test_point).Magnitude();
                points_.emplace_back(cur_test_point, segment.EvaluateDerivative(t).Normalized(), path_length);
                bounds_.AddPoint(cur_test_point);
                vert_dist = 0;
                last_pass_point = cur_test_point;
            }
            else
            {
                vert_dist += (cur_test_point - pre_test_point).Magnitude();
            }
            pre_test_point = cur_test_point;
        }

        anchor_index_map_.push_back(points_.size() - 1);
    }

    CalcNormal(spline);
}

BezierPath::BezierPath(const BezierSpline& spline, float spacing, float accuracy) {
    is_closed_ = spline.IsClosed();
    auto segment = spline.GetSegment(0);

    points_.emplace_back(spline[0], segment.EvaluateDerivative(0).Normalized(), 0);
    anchor_index_map_.push_back(0);
    bounds_.AddPoint(spline[0]);

    Vector3 pre_test_point = spline[0];
    Vector3 last_pass_point = spline[0];

    float path_length = 0;
    float vert_dist = 0;

    // Go through all segments and split up into vertices
    for (int segment_index = 0; segment_index < spline.NumSegments(); segment_index++)
    {
        auto segment = spline.GetSegment(segment_index);

        float estimated_length = segment.EstimateLength();
        int divisions = math::Ceil(estimated_length * accuracy);
        float increment = 1.0f / divisions;

        for (float t = increment; t <= 1; t += increment)
        {
            bool is_last_point = (t + increment > 1 && segment_index == spline.NumSegments() - 1);
            if (is_last_point)
            {
                t = 1; //break at next loop
            }
            Vector3 cur_test_point = segment.EvaluatePoint(t);
            vert_dist += (cur_test_point - pre_test_point).Magnitude();

            // If vertices are now too far apart, go back by amount we overshot by
            if (vert_dist > spacing) {
                float over_dist = vert_dist - spacing;
                cur_test_point += (pre_test_point - cur_test_point).Normalized() * over_dist;
                t -= increment;
            }

            if (vert_dist >= spacing || is_last_point)
            {
                path_length += (last_pass_point - cur_test_point).Magnitude();
                points_.emplace_back(cur_test_point, segment.EvaluateDerivative(t).Normalized(), path_length);
                bounds_.AddPoint(cur_test_point);
                vert_dist = 0;
                last_pass_point = cur_test_point;
            }
            pre_test_point = cur_test_point;
        }
        anchor_index_map_.push_back(points_.size() - 1);
    }

    CalcNormal(spline);
}

void BezierPath::CalcNormal(const BezierSpline& spline) {
    up_ = (bounds_.Size().z > bounds_.Size().y) ? Vector3::up : -Vector3::forward;
    Vector3 last_rotation_axis = up_;
    float length = points_.back().cumulative_length;

    points_[0].time = points_[0].cumulative_length / length;
    points_[0].normal = last_rotation_axis.Cross(points_[0].tangent).Normalized();

    // Loop through the data and assign to arrays.
    for (int i = 1; i < points_.size(); i++) {
        auto& point = points_[i];
        point.time = point.cumulative_length / length;

        // First reflection
        Vector3 offset = (point.position - points_[i - 1].position).Normalized();
        Vector3 r = Vector3::Reflect(last_rotation_axis, offset);
        Vector3 t = Vector3::Reflect(points_[i - 1].tangent, offset);

        // Second reflection
        Vector3 v2 = (point.tangent - t).Normalized();

        last_rotation_axis = Vector3::Reflect(r, v2);
        Vector3 n = last_rotation_axis.Cross(point.tangent).Normalized();
        point.normal = n;
    }

    if (is_closed_) {
        auto& last = points_.back();
        auto& first = points_.front();
        float join_normal_angle_error = Vector3::SignedAngle(last.normal, first.normal, first.tangent);
        // Gradually rotate the normals along the path to ensure start and end normals line up correctly
        if (math::Abs(join_normal_angle_error) > 0.1f) // don't bother correcting if very nearly correct
        {
            for (int i = 1; i < points_.size(); i++) {
                float t = (i / (points_.size() - 1.0f));
                float angle = join_normal_angle_error * t;

                auto& point = points_[i];
                Quaternion rot = Quaternion::AngleAxis(angle, point.tangent);
                point.normal = rot * point.normal;// *(FlipNormals) ? -1 : 1);
            }
        }
    }

    // Rotate normals to match up with user-defined anchor angles
    //if (spline.global_normal_angle() == 0) return;
    for (int anchor_index = 0; anchor_index < anchor_index_map_.size() - 1; anchor_index++) {
        int next_anchor_index = (is_closed_) ? (anchor_index + 1) % spline.NumSegments() : anchor_index + 1;

        float begin_angle = spline.GetAnchorNormalAngle(anchor_index) + spline.global_normal_angle();
        float end_angle = spline.GetAnchorNormalAngle(next_anchor_index) + spline.global_normal_angle();
        float delta_angle = math::DeltaAngle(begin_angle, end_angle);

        int begin_vert_index = anchor_index_map_[anchor_index];
        int end_vert_index = anchor_index_map_[anchor_index + 1];

        int num = end_vert_index - begin_vert_index;
        if (anchor_index == anchor_index_map_.size() - 2) {
            num += 1;
        }

        for (int i = 0; i < num; i++) {
            int vertIndex = begin_vert_index + i;
            auto& point = points_[vertIndex];
            float t = num == 1 ? 1.0f : i / (num - 1.0f);
            float angle = begin_angle + delta_angle * t;
            Quaternion rot = Quaternion::AngleAxis(angle, point.tangent);
            point.normal = (rot * point.normal);// *((FlipNormals) ? -1 : 1);
        }
    }
}

BezierPath::TimeOnCurve BezierPath::CalcTimeOnCurve(float time, bool loop) const {
    if (loop) {
        if (time < 0) {
            time += math::Ceil(math::Abs(time));
        }
        time = math::Fmod(time, 1.0f);
    }
    else {
        time = math::Saturate(time);
    }

    int prev_index = 0;
    int next_index = points_.size() - 1;
    int i = math::Round(time * (points_.size() - 1)); // starting guess

    // Starts by looking at middle vertex and determines if t lies to the left or to the right of that vertex.
    // Continues dividing in half until closest surrounding vertices have been found.
    while (true) {
        // t lies to left
        if (time <= points_[i].time) {
            next_index = i;
        }
        // t lies to right
        else {
            prev_index = i;
        }
        i = (next_index + prev_index) / 2;

        if (next_index - prev_index <= 1) {
            break;
        }
    }

    float time_in_segment = math::InverseLerp(points_[prev_index].time, points_[next_index].time, time);
    return { prev_index, next_index, time_in_segment };
}

Vector3 BezierPath::GetNormal(float t, bool loop) const {
    auto info = CalcTimeOnCurve(t, loop);
    return Vector3::Lerp(points_[info.pre_index].normal, points_[info.next_index].normal, info.time);
}

Vector3 BezierPath::GetTangent(float t, bool loop) const {
    auto info = CalcTimeOnCurve(t, loop);
    return Vector3::Lerp(points_[info.pre_index].tangent, points_[info.next_index].tangent, info.time);
}

Vector3 BezierPath::GetPoint(float t, bool loop) const {
    auto info = CalcTimeOnCurve(t, loop);
    return Vector3::Lerp(points_[info.pre_index].position, points_[info.next_index].position, info.time);
}

}

