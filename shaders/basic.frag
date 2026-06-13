#version 330 core

out vec4 fragmentColor;

uniform vec3 uColor;

void main() {
    fragmentColor = vec4(uColor, 1.0);
}
