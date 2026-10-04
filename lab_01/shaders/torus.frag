#version 450
layout(location=0) in vec3 worldNormal;
layout(location=0) out vec4 outColor;
void main() {
    float diffuse=max(dot(normalize(worldNormal),normalize(vec3(-.4,.8,.6))),0.0);
    outColor=vec4(vec3(.16,.55,.85)*(.3+.7*diffuse),1.0);
}
