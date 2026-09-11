#version 430 core
in vec2 vUV;
out vec4 FragColor;

uniform sampler2D uBackgroundTex;

uniform float uDimming;  // 0.0 = full bright, 1.0 = black

void main() {
    vec4 col = texture(uBackgroundTex, vUV);
    col.rgb *= (1.0 - uDimming);
    FragColor = col;
}
