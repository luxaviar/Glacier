#include "rect.h"

namespace glacier {

Rect::Rect(const Vec2f& a_, const Vec2f& b_, const Vec2f& c_) :
    a(a_),
    b(b_),
    c(c_)
{
}

Rect::Rect(const Vec2f& center, const Vec2f& forward, float half_width, float half_height) {
    Vec2f left(-forward.y, forward.x);
    //Vec2f right(forward.y, -forward.x);

    a = center + left * half_width + forward * half_height;
    b = center + left * half_width - forward * half_height;
    c = center - left * half_width - forward * half_height;
}

/*
0 <= dot(AB,AM) <= dot(AB,AB) &&
0 <= dot(AD,AM) <= dot(AD,AD)
*/
bool Rect::Contains(const Vec2f& point) const {
    Vec2f ba = a - b;
    Vec2f bc = c - b;

    Vec2f bm = point - b;

    float dot_mb = bm.Dot(ba);
    float dot_ab = ba.Dot(ba);
    float dot_mc = bm.Dot(bc);
    float dot_bc = bc.Dot(bc);

    return 0 <= dot_mb && dot_mb <= dot_ab && 0 <= dot_mc && dot_mc <= dot_bc;
}

bool Rect::Contains(const Vec3f& point) const {
    Vec2f p(point.x, point.z);
    return Contains(p);

}


}

