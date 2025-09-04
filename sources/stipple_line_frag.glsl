#version 430

uniform float stipplePattern[16];
// picking
uniform vec2 mousePos;
uniform int objName;
uniform float devPixRatio;

in highp float gStippleCoord;
in highp vec4 gColor;


layout (location = 0) out vec4 fc; 

layout(std430, binding = 0) coherent buffer PickedNameSSBO
{
  uint nameCount[3];
  uint names[3][32];
} pickedNameSSBO;

bool isNeighbor(int range){
  return abs(mousePos.x - gl_FragCoord.x / devPixRatio) <= range &&
		  abs(mousePos.y - gl_FragCoord.y / devPixRatio) <= range;
}

void main() {
	
	// when pick mode
	if(objName != 0){
	  int ranges[3] = {5, 10 , 20};

	  for(int r = 0; r < 3; r++){
		if(isNeighbor(ranges[r]) && pickedNameSSBO.nameCount[r] < 32){
		  bool found = false;
		  for(int n = 0; n < pickedNameSSBO.nameCount[r]; n++){
			if(pickedNameSSBO.names[r][n] == uint(objName)){
			  found = true;
			  break;
			}
		  }
		  if(!found){
			pickedNameSSBO.names[r][pickedNameSSBO.nameCount[r]] = uint(objName);
			pickedNameSSBO.nameCount[r]++;
		  }
		}
	  }
	}

	float stip = stipplePattern[int(fract(gStippleCoord) * 16.0)];
	if (stip == 0.0)
		discard;
	fc = gColor;
}