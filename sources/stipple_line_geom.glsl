#version 410
layout (lines) in;
layout (line_strip, max_vertices = 2) out;

uniform lowp vec2 viewportSize;
uniform float stippleFactor;

in highp vec4 vColor[];
out highp vec4 gColor;
out highp float gStippleCoord;

void main() {
	vec4 pos0 = gl_in[0].gl_Position;
	vec4 pos1 = gl_in[1].gl_Position;
	vec2 p0 = pos0.xy / pos0.w * viewportSize;
	vec2 p1 = pos1.xy / pos1.w * viewportSize;
	float len = length(p0.xy - p1.xy);
	// Emit first vertex
	gColor = vColor[0];
	gl_Position = pos0;
	gStippleCoord = 0.0;
	EmitVertex();
	// Emit second vertex
	gColor = vColor[1];
	gl_Position = pos1;
	gStippleCoord = len / stippleFactor / 32.0; // Note: not 16, see above
	EmitVertex();
}