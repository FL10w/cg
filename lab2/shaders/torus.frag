#version 450
layout(location=0) in vec3 worldNormal;
layout(location=1) in vec3 worldPosition;
layout(set=0,binding=0,std140) uniform SceneData {
    mat4 viewProjection;
    vec4 cameraPosition;
    vec4 lightDirectionIntensity;
    vec4 lightColor;
    vec4 ambientColorIntensity;
    vec4 albedo;
    vec4 specularShininess;
} scene;
layout(location=0) out vec4 outColor;
vec3 safeNormalize(vec3 v) {
    return v*inversesqrt(max(dot(v,v),1e-8));
}
void main() {
    vec3 N=safeNormalize(worldNormal);
    vec3 L=-safeNormalize(scene.lightDirectionIntensity.xyz);
    vec3 V=safeNormalize(scene.cameraPosition.xyz-worldPosition);
    vec3 H=safeNormalize(L+V);
    float diffuse=max(dot(N,L),0.0);
    // Back-facing light cannot produce a highlight.
    float specular=diffuse>0.0 ? pow(max(dot(N,H),0.0),scene.specularShininess.w) : 0.0;
    vec3 ambient=scene.ambientColorIntensity.rgb*scene.ambientColorIntensity.w*scene.albedo.rgb;
    vec3 direct=scene.lightColor.rgb*scene.lightDirectionIntensity.w*
                (scene.albedo.rgb*diffuse+scene.specularShininess.rgb*specular);
    outColor=vec4(ambient+direct,1.0);
}
