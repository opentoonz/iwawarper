#version 410
layout (triangles) in;
layout (line_strip, max_vertices = 4) out;

in highp vec4 vColor[];
out highp vec4 gColor;

void main() {
	for(int v = 0; v < 3; v++) {
	   gColor = vColor[v];
	   gl_Position = gl_in[v].gl_Position;
	   EmitVertex();
    }
	gColor = vColor[0];
	gl_Position = gl_in[0].gl_Position;
	EmitVertex();
}