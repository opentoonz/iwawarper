#version 410
layout(location = 0) in vec3 position;
out highp vec4 vColor;

uniform highp mat4 matrix;
uniform highp vec4 color;

void main() {
   vColor = color;
   gl_Position = matrix * vec4(position,1.0);
}