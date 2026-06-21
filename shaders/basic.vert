#version 330 core

layout(location = 0) in vec3 aPosition;

uniform mat4 uMVP;
uniform mat4 uModel;

out vec3 vWorldPosition;

void main() {
    vWorldPosition = vec3(uModel * vec4(aPosition, 1.0));
    gl_Position = uMVP * vec4(aPosition, 1.0);
}
