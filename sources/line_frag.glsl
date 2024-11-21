#version 410
in highp vec4 gColor;

layout (location = 0) out vec4 fc; 

void main() {
   fc = gColor;
}