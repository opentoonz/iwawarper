#version 410
in highp vec4 vColor;

layout (location = 0) out vec4 fc; 

void main() {
   fc = vColor;
}