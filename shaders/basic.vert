#version 330 core

layout(location = 0) in vec3 aPosition;

uniform mat4 uMVP;
uniform mat4 uModel;
uniform mat4 uLightMVP;

out vec3 vWorldPosition;
out vec4 vLightPosition;

void main() {
    vec4 worldPosition = uModel * vec4(aPosition, 1.0);
    vWorldPosition = vec3(worldPosition);
    vLightPosition = uLightMVP * vec4(aPosition, 1.0);
    gl_Position = uMVP * vec4(aPosition, 1.0);
}
