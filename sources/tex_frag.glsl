#version 410
uniform sampler2D texture;
uniform highp float alpha;
in highp vec2 vUv;

layout (location = 0) out vec4 fc; 

void main() {
   fc = texture2D(texture, vUv)*alpha;
}