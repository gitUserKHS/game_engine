#version 330 core

out vec4 fragmentColor;

uniform vec3 uColor;
uniform vec3 uLightDirection;
uniform vec3 uLightColor;
uniform float uLightIntensity;
uniform float uAmbient;
uniform bool uLightingEnabled;

in vec3 vWorldPosition;

void main() {
    if (!uLightingEnabled) {
        fragmentColor = vec4(uColor, 1.0);
        return;
    }

    vec3 normal = normalize(cross(dFdx(vWorldPosition), dFdy(vWorldPosition)));
    if (!gl_FrontFacing) {
        normal = -normal;
    }
    vec3 lightToSurface = normalize(uLightDirection);
    float diffuse = max(dot(normal, -lightToSurface), 0.0);
    vec3 litColor = uColor * (uAmbient + uLightColor * diffuse * uLightIntensity);
    fragmentColor = vec4(clamp(litColor, 0.0, 1.0), 1.0);
}
