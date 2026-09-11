#version 430 core

layout(location = 0) in vec3 aPosition;

uniform mat4 uModel;
uniform mat4 uView;
uniform mat4 uProjection;

out vec3 vLocalPos;

void main()
{
    vLocalPos = aPosition;  // [-0.5, 0.5]^3
    gl_Position = uProjection * uView * uModel * vec4(aPosition, 1.0);
}
