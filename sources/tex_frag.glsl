#version 410
uniform sampler2D texture;
uniform sampler2D matteTexture;

uniform highp float alpha;
uniform bool matte_sw;
uniform highp vec4 matteColors[10];
uniform highp float matteTolerance;
uniform lowp vec2 matteImgSize;
uniform int matteDilate;

in highp vec2 vUv;
in highp vec2 vMatteUv;

layout (location = 0) out vec4 fc; 

bool checkMatte(vec4 mc, int mcIndex, float matteTolerance){
	return abs(mc.x - matteColors[mcIndex].x) <= matteTolerance
			&& abs(mc.y - matteColors[mcIndex].y) <= matteTolerance
			&& abs(mc.z - matteColors[mcIndex].z) <= matteTolerance;
}


void main() {
	fc = texture2D(texture, vUv)*alpha;
   
	if(matte_sw){
		vec2 uvStep = vec2(1./matteImgSize.x, 1./matteImgSize.y);
		bool isInMatte = false;

		for(int dy = -matteDilate; dy <= matteDilate; dy++){
			float vPos = vMatteUv.y + uvStep.y * dy;
			for(int dx = -matteDilate; dx <= matteDilate; dx++){
				float uPos = vMatteUv.x + uvStep.x * dx;		
				// matteTexture‚ÌF‚ðŽæ“¾
				vec4 mc = texture2D(matteTexture, vec2(uPos, vPos));

				for(int i = 0; i < 10; i++){
					// MatteF‚ª‚à‚¤‚È‚¢ê‡
					if(matteColors[i] == vec4(0.))
						break;
					// Matte‚ÉŠÜ‚Ü‚ê‚éðŒ
					if( checkMatte(mc, i, matteTolerance) ){
						isInMatte = true;
						break;
					}
				}
				if(isInMatte)
					break;
			}
			if(isInMatte)
				break;
		}
		if(!isInMatte)
			fc = vec4(0.);
	}
}