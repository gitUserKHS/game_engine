#version 330 core

out vec4 fragmentColor;

uniform vec3 uColor;
uniform vec3 uLightDirection;
uniform vec3 uLightColor;
uniform float uLightIntensity;
uniform float uAmbient;
uniform bool uLightingEnabled;
uniform bool uShadowEnabled;
uniform sampler2D uShadowMap;

in vec3 vWorldPosition;
in vec4 vLightPosition;

float shadowFactor() {
    if (!uShadowEnabled) {
        return 1.0;
    }

    vec3 projection = vLightPosition.xyz / vLightPosition.w;
    projection = projection * 0.5 + 0.5;
    if (projection.x < 0.0 || projection.x > 1.0 ||
        projection.y < 0.0 || projection.y > 1.0 ||
        projection.z > 1.0) {
        return 1.0;
    }

    float closestDepth = texture(uShadowMap, projection.xy).r;
    float currentDepth = projection.z;
    float bias = 0.006;
    return currentDepth - bias > closestDepth ? 0.45 : 1.0;
}

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
    float shadow = shadowFactor();
    vec3 litColor = uColor * (uAmbient + uLightColor * diffuse * uLightIntensity * shadow);
    fragmentColor = vec4(clamp(litColor, 0.0, 1.0), 1.0);
}
