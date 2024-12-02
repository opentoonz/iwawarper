#version 410
layout(location = 0) in vec3 position;
layout(location = 1) in vec2 uv;
out highp vec2 vUv;
out highp vec2 vMatteUv;

uniform highp mat4 matrix;
uniform lowp vec2 matteImgSize;
uniform lowp vec2 workAreaSize;
uniform bool matte_sw;

void main() {
   vUv = uv;
   gl_Position = matrix * vec4(position,1.0);
   if(matte_sw)
		vMatteUv = vec2((position.x-workAreaSize.x/2.)/matteImgSize.x + 0.5,
						(position.y-workAreaSize.y/2.)/matteImgSize.y + 0.5);
}