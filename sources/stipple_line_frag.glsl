#version 410

uniform float stipplePattern[16];

in highp float gStippleCoord;
in highp vec4 gColor;

layout (location = 0) out vec4 fc; 

void main() {
	float stip = stipplePattern[int(fract(gStippleCoord) * 16.0)];
	if (stip == 0.0)
		discard;
	fc = gColor;
}