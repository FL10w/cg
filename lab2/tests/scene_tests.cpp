#include "scene.hpp"
#include <iostream>
#include <map>
#include <utility>
using namespace scene;
void require(bool condition,const char* message) { if(!condition) throw std::runtime_error(message); }
bool close(float a,float b) { return std::abs(a-b)<1e-4f; }
std::array<float,4> transform(const Mat4& m,std::array<float,4> p) {
    std::array<float,4> out{};
    for(int r=0;r<4;++r) for(int c=0;c<4;++c) out[r]+=m.at(r,c)*p[c];
    return out;
}
int main() {
    try {
        const auto mesh=torus();
        require(mesh.vertices.size()==2048 && mesh.indices.size()==12288,"Unexpected torus size");
        std::map<std::pair<uint32_t,uint32_t>,int> edges;
        for(const auto& v:mesh.vertices) {
            const float radial=std::hypot(v.position.x,v.position.z);
            require(close((radial-1.4f)*(radial-1.4f)+v.position.y*v.position.y,.25f),"Vertex is off torus surface");
            require(close(dot(v.normal,v.normal),1),"Normal is not unit length");
        }
        for(size_t i=0;i<mesh.indices.size();i+=3) {
            uint32_t ids[]{mesh.indices[i],mesh.indices[i+1],mesh.indices[i+2]};
            for(auto id:ids) require(id<mesh.vertices.size(),"Invalid index");
            const auto& a=mesh.vertices[ids[0]], b=mesh.vertices[ids[1]], c=mesh.vertices[ids[2]];
            require(dot(cross(b.position-a.position,c.position-a.position),a.normal+b.normal+c.normal)>0,"Inward triangle winding");
            for(int e=0;e<3;++e) { auto x=ids[e],y=ids[(e+1)%3]; if(x>y) std::swap(x,y); ++edges[{x,y}]; }
        }
        for(auto [edge,count]:edges) { (void)edge; require(count==2,"Non-manifold edge or open seam"); }
        require(static_cast<int>(mesh.vertices.size())-static_cast<int>(edges.size())+static_cast<int>(mesh.indices.size()/3)==0,"Wrong Euler characteristic");
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
        std::cout<<"PASS: closed torus, winding, procedural normals, camera pose/movement/FOV/poles, Vulkan depth, zero light direction\n";
    } catch(const std::exception& e) { std::cerr<<e.what()<<'\n'; return 1; }
}
