#version 450
layout(location=0) in vec3 position;
layout(location=1) in vec3 normal;
layout(set=0,binding=0,std140) uniform CameraData {
    mat4 viewProjection;
} camera;
layout(location=0) out vec3 worldNormal;
void main() {
    gl_Position=camera.viewProjection*vec4(position,1.0);
    worldNormal=normal;
}
