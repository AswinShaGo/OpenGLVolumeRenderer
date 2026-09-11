#version 430 core
layout(location = 0) in vec3 aPosition;

out vec2 vUV;

uniform mat4 uModel;
uniform mat4 uView;
uniform mat4 uProjection;

void main() {
    vUV = aPosition.xy;  // [-1, 1] range
    gl_Position = uProjection * uView * uModel * vec4(aPosition, 1.0);
}
