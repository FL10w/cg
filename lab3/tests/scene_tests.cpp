#include "scene.hpp"
#include <iostream>
#include <map>
#include <utility>
using namespace scene;
void require(bool condition,const char* message) { if(!condition) throw std::runtime_error(message); }
bool close(float a,float b) { return std::abs(a-b)<1e-4f; }
bool same(Vec3 a,Vec3 b) { return close(a.x,b.x) && close(a.y,b.y) && close(a.z,b.z); }
std::array<float,4> transform(const Mat4& m,std::array<float,4> p) {
    std::array<float,4> out{};
    for(int r=0;r<4;++r) for(int c=0;c<4;++c) out[r]+=m.at(r,c)*p[c];
    return out;
}
int main() {
    try {
        const auto mesh=torus();
        constexpr uint32_t major=64,minor=32;
        require(mesh.vertices.size()==2145 && mesh.indices.size()==12288,"Unexpected torus size");
        std::map<std::pair<uint32_t,uint32_t>,int> edges;
        for(const auto& v:mesh.vertices) {
            const float radial=std::hypot(v.position.x,v.position.z);
            require(close((radial-1.4f)*(radial-1.4f)+v.position.y*v.position.y,.25f),"Vertex is off torus surface");
            require(close(dot(v.normal,v.normal),1),"Normal is not unit length");
            require(v.uv.x>=0 && v.uv.x<=1 && v.uv.y>=0 && v.uv.y<=1,"UV outside [0,1]");
            const float u=2*pi*v.uv.x, angle=2*pi*v.uv.y;
            require(same(v.normal,{std::cos(angle)*std::cos(u),std::sin(angle),std::cos(angle)*std::sin(u)}),
                    "UV coordinates do not follow the torus parameterization");
        }
        for(uint32_t j=0;j<=minor;++j) {
            const auto& a=mesh.vertices[j];
            const auto& b=mesh.vertices[major*(minor+1)+j];
            require(same(a.position,b.position) && same(a.normal,b.normal),"Major seam does not match");
            require(a.uv.x==0 && b.uv.x==1 && a.uv.y==b.uv.y,"Major seam UV endpoints missing");
        }
        for(uint32_t i=0;i<=major;++i) {
            const auto& a=mesh.vertices[i*(minor+1)];
            const auto& b=mesh.vertices[i*(minor+1)+minor];
            require(same(a.position,b.position) && same(a.normal,b.normal),"Minor seam does not match");
            require(a.uv.y==0 && b.uv.y==1 && a.uv.x==b.uv.x,"Minor seam UV endpoints missing");
        }
        // Check the closed surface after identifying the duplicated seam vertices.
        const auto welded=[](uint32_t id) { return ((id/(minor+1))%major)*minor+(id%(minor+1))%minor; };
        for(size_t i=0;i<mesh.indices.size();i+=3) {
            uint32_t ids[]{mesh.indices[i],mesh.indices[i+1],mesh.indices[i+2]};
            for(auto id:ids) require(id<mesh.vertices.size(),"Invalid index");
            const auto& a=mesh.vertices[ids[0]], b=mesh.vertices[ids[1]], c=mesh.vertices[ids[2]];
            require(dot(cross(b.position-a.position,c.position-a.position),a.normal+b.normal+c.normal)>0,"Inward triangle winding");
            require(std::max({a.uv.x,b.uv.x,c.uv.x})-std::min({a.uv.x,b.uv.x,c.uv.x})<=1.0f/major+1e-6f &&
                    std::max({a.uv.y,b.uv.y,c.uv.y})-std::min({a.uv.y,b.uv.y,c.uv.y})<=1.0f/minor+1e-6f,
                    "Triangle interpolates across a UV wrap");
            for(int e=0;e<3;++e) { auto x=welded(ids[e]),y=welded(ids[(e+1)%3]); if(x>y) std::swap(x,y); ++edges[{x,y}]; }
        }
        for(auto [edge,count]:edges) { (void)edge; require(count==2,"Non-manifold edge or open seam"); }
        require(static_cast<int>(major*minor)-static_cast<int>(edges.size())+static_cast<int>(mesh.indices.size()/3)==0,"Wrong Euler characteristic");
        require(uniformStride(1)==144 && uniformStride(64)==192 && uniformStride(256)==256,"Dynamic UBO stride alignment failed");
        for(size_t alignment:{size_t(1),size_t(64),size_t(256)})
            require(uniformStride(alignment)>=sizeof(Uniforms) && uniformStride(alignment)%alignment==0,"Invalid dynamic UBO stride");
        {
            auto m=perspective(radians(45),1.5f,.1f,100);
            const auto n=transform(m,{0,0,-.1f,1}),f=transform(m,{0,0,-100,1});
            require(close(n[2]/n[3],0) && close(f[2]/f[3],1),"Projection depth is not Vulkan [0,1]");
            require(transform(m,{0,1,-2,1})[1]<0,"Vulkan Y flip missing");
        }
        auto eye=transform(lookAt({2,3,4},{}),{2,3,4,1});
        require(close(eye[0],0) && close(eye[1],0) && close(eye[2],0),"Camera origin mismatch");
        Camera camera;
        const auto origin=transform(camera.view(),{camera.position.x,camera.position.y,camera.position.z,1});
        require(close(origin[0],0) && close(origin[1],0) && close(origin[2],0),"Free camera origin mismatch");
        const auto target=transform(camera.view(),{0,0,0,1});
        require(close(target[0],0) && close(target[1],0) && target[2]<0,"Initial camera misses the torus");
        const auto original=camera.position;
        camera.move({0,0,1},1);
        require(close(dot(camera.position-original,camera.position-original),9),"Forward speed mismatch");
        Camera stepped;
        for(int i=0;i<60;++i) stepped.move({0,0,1},1.0f/60);
        require(dot(stepped.position-camera.position,stepped.position-camera.position)<1e-8f,"Movement depends on FPS");
        camera=Camera{}; camera.move({1,0,1},1);
        require(close(dot(camera.position-original,camera.position-original),9),"Diagonal movement is faster");
        camera.rotate(1000,1000);
        require(camera.pitch==89 && std::abs(camera.yaw)<=180,"Camera angle limits failed");
        for(float value:camera.view().v) require(std::isfinite(value),"Camera pole causes a singular view");
        camera=Camera{};
        const float narrow=camera.viewProjection(1).at(0,0);
        camera.fov=75;
        require(std::abs(camera.viewProjection(1).at(0,0))<std::abs(narrow),"FOV has no effect on projection");
        Lighting light; light.direction={};
        const auto data=uniforms(camera,light,Material{},1);
        require(data.lightDirectionIntensity.y==-1 && data.lightDirectionIntensity.w==1,"Zero light direction fallback failed");
        std::cout<<"PASS: torus geometry/winding, UV mapping/seams, dynamic UBO alignment, camera pose/movement/FOV/poles, Vulkan depth, zero light direction\n";
    } catch(const std::exception& e) { std::cerr<<e.what()<<'\n'; return 1; }
}
