#version 430 core
in vec2 vUV;
out vec4 FragColor;

uniform vec4  uPlaneColor;  // interior color + alpha
uniform vec4  uRimColor;    // edge color + alpha
uniform float uRimWidth;    // edge width in UV space

void main() {
    // Distance from nearest edge of the [-1,1] quad
    float dx = 1.0 - abs(vUV.x);
    float dy = 1.0 - abs(vUV.y);
    float edgeDist = min(dx, dy);

    // Smooth rim: 1.0 at edge, 0.0 in interior
    float rim = 1.0 - smoothstep(0.0, uRimWidth, edgeDist);

    FragColor = mix(uPlaneColor, uRimColor, rim);
}
