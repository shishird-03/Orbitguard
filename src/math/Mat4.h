// Own 3D vector and 4x4 homogeneous matrix (column-major, double precision).
// Every transform in ORBITGUARD (orbital-element chain, camera, picking) goes through this.
#pragma once
#include <cmath>

namespace og {

constexpr double PI = 3.14159265358979323846;
constexpr double DEG = PI / 180.0;

struct Vec3 {
    double x = 0, y = 0, z = 0;
    Vec3() = default;
    Vec3(double x_, double y_, double z_) : x(x_), y(y_), z(z_) {}
    Vec3 operator+(const Vec3& o) const { return {x + o.x, y + o.y, z + o.z}; }
    Vec3 operator-(const Vec3& o) const { return {x - o.x, y - o.y, z - o.z}; }
    Vec3 operator*(double s) const { return {x * s, y * s, z * s}; }
    Vec3 operator/(double s) const { return {x / s, y / s, z / s}; }
    Vec3 operator-() const { return {-x, -y, -z}; }
    Vec3& operator+=(const Vec3& o) { x += o.x; y += o.y; z += o.z; return *this; }
    Vec3& operator-=(const Vec3& o) { x -= o.x; y -= o.y; z -= o.z; return *this; }
};
inline double dot(const Vec3& a, const Vec3& b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
inline Vec3 cross(const Vec3& a, const Vec3& b) {
    return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
}
inline double length(const Vec3& a) { return std::sqrt(dot(a, a)); }
inline Vec3 normalize(const Vec3& a) { double l = length(a); return l > 0 ? a / l : a; }
inline Vec3 lerp(const Vec3& a, const Vec3& b, double t) { return a + (b - a) * t; }

// m[col*4 + row], same memory layout OpenGL expects for glLoadMatrixd.
struct Mat4 {
    double m[16];
    Mat4();  // identity
    double& at(int row, int col) { return m[col * 4 + row]; }
    double at(int row, int col) const { return m[col * 4 + row]; }
    Mat4 operator*(const Mat4& o) const;
    Vec3 transformPoint(const Vec3& p) const;  // w = 1, with perspective divide
    Vec3 transformDir(const Vec3& d) const;    // w = 0
    void transform4(const double in[4], double out[4]) const;
};

Mat4 translate(const Vec3& t);
Mat4 scale(const Vec3& s);
Mat4 rotateX(double rad);
Mat4 rotateY(double rad);
Mat4 rotateZ(double rad);
Mat4 transpose(const Mat4& a);
Mat4 inverse(const Mat4& a);  // general 4x4 (cofactor expansion)
Mat4 perspective(double fovyRad, double aspect, double zNear, double zFar);
Mat4 ortho(double l, double r, double b, double t, double n, double f);
Mat4 lookAt(const Vec3& eye, const Vec3& center, const Vec3& up);

}  // namespace og
