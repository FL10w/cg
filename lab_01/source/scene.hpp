#pragma once

#include <array>
#include <cmath>
#include <cstdint>
#include <stdexcept>
#include <vector>

// Column-major matrices; column vectors, right-handed world, Vulkan depth [0, 1].
namespace scene {
constexpr float pi = 3.14159265358979323846f;
constexpr float radians(float degrees) { return degrees * pi / 180.0f; }
struct Vec3 { float x{}, y{}, z{}; };
inline Vec3 operator+(Vec3 a, Vec3 b) { return {a.x+b.x,a.y+b.y,a.z+b.z}; }
inline Vec3 operator-(Vec3 a, Vec3 b) { return {a.x-b.x,a.y-b.y,a.z-b.z}; }
inline Vec3 operator*(Vec3 a, float s) { return {a.x*s,a.y*s,a.z*s}; }
inline float dot(Vec3 a, Vec3 b) { return a.x*b.x+a.y*b.y+a.z*b.z; }
inline Vec3 cross(Vec3 a, Vec3 b) { return {a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x}; }
inline Vec3 normalized(Vec3 a) { return a * (1.0f / std::sqrt(dot(a,a))); }
struct alignas(16) Mat4 {
    std::array<float,16> v{};
    float& at(int row,int col) { return v[col*4+row]; }
    float at(int row,int col) const { return v[col*4+row]; }
    static Mat4 identity() { Mat4 m; for(int i=0;i<4;++i) m.at(i,i)=1; return m; }
};
inline Mat4 operator*(const Mat4& a,const Mat4& b) {
    Mat4 c;
    for(int r=0;r<4;++r) for(int k=0;k<4;++k) for(int j=0;j<4;++j)
        c.at(r,k)+=a.at(r,j)*b.at(j,k);
    return c;
}
inline Mat4 lookAt(Vec3 eye,Vec3 target,Vec3 up={0,1,0}) {
    const auto f=normalized(target-eye), s=normalized(cross(f,up)), u=cross(s,f);
    auto m=Mat4::identity();
    m.at(0,0)=s.x; m.at(0,1)=s.y; m.at(0,2)=s.z; m.at(0,3)=-dot(s,eye);
    m.at(1,0)=u.x; m.at(1,1)=u.y; m.at(1,2)=u.z; m.at(1,3)=-dot(u,eye);
    m.at(2,0)=-f.x; m.at(2,1)=-f.y; m.at(2,2)=-f.z; m.at(2,3)=dot(f,eye);
    return m;
}
inline Mat4 perspective(float fov,float aspect,float nearPlane,float farPlane) {
    Mat4 m; const float f=1/std::tan(fov/2);
    m.at(0,0)=f/aspect; m.at(1,1)=-f;
    m.at(2,2)=farPlane/(nearPlane-farPlane);
    m.at(2,3)=farPlane*nearPlane/(nearPlane-farPlane); m.at(3,2)=-1; return m;
}
struct Vertex { Vec3 position, normal; };
struct Mesh { std::vector<Vertex> vertices; std::vector<uint32_t> indices; };
inline Mesh torus(float R=1.4f,float r=.5f,uint32_t major=64,uint32_t minor=32) {
    if (!(R>r && r>0) || major<3 || minor<3) throw std::invalid_argument("Invalid torus dimensions");
    Mesh mesh;
    for(uint32_t i=0;i<major;++i) for(uint32_t j=0;j<minor;++j) {
        const float u=2*pi*i/major, v=2*pi*j/minor;
        Vec3 p{(R+r*std::cos(v))*std::cos(u),r*std::sin(v),(R+r*std::cos(v))*std::sin(u)};
        Vec3 n{std::cos(v)*std::cos(u),std::sin(v),std::cos(v)*std::sin(u)};
        mesh.vertices.push_back({p,n});
        const uint32_t a=i*minor+j, b=((i+1)%major)*minor+j;
        const uint32_t cidx=((i+1)%major)*minor+(j+1)%minor, d=i*minor+(j+1)%minor;
        mesh.indices.insert(mesh.indices.end(),{a,d,b,b,d,cidx});
    }
    return mesh;
}
} // namespace scene
