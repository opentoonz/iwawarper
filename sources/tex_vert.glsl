#version 410
layout(location = 0) in vec3 position;
layout(location = 1) in vec2 uv;
out highp vec2 vUv;

uniform highp mat4 matrix;

void main() {
   vUv = uv;
   gl_Position = matrix * vec4(position,1.0);
}