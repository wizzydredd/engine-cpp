R"(const float PI = 3.14159265358979323846;
const float Epsilon = 0.00001;

float saturate(float x) {
	return max(min(x, 1.0), 0.0);
}

vec2 saturate(vec2 x) {
	return vec2(saturate(x.x), saturate(x.y));
}

vec3 saturate(vec3 x) {
	return vec3(saturate(x.xy), saturate(x.z));
}

vec4 saturate(vec4 x) {
	return vec4(saturate(x.xyz), saturate(x.w));
}

struct TextureSlotOptions {
	bool enabled;
	vec4 uv_transform; // xy = position, zw = scale
};

struct TextureSlot2D {
	TextureSlotOptions opt;
	sampler2D img;
};

struct TextureSlotCube {
	TextureSlotOptions opt;
	samplerCube img;
};

vec2 transformUV(TextureSlotOptions opt, vec2 uv) {
	return opt.uv_transform.xy + uv * opt.uv_transform.zw;
}

struct Material {
	vec3 baseColor;
	float roughness;
	float metallic;
	float emission;

	// POM
	float heightScale;
	bool discardEdges;
};

struct Light {
	vec3 color;
	float intensity;

	float radius;
	float size;
	float nearPlane;

	vec3 position;
	vec3 direction;

	float lightCutoff;
	float spotCutoff;

	int type;
};

#define TexSlot2D(name) uniform TextureSlot2D t##name;
#define TexSlotCube(name) uniform TextureSlotCube t##name;

#define TexSlotEnabled(name) t##name.opt.enabled
#define TexSlotGet(name) t##name

vec3 normalMap(mat3 tbn, vec3 N) {
	N.y = 1.0 - N.y;
	return normalize(tbn * (N * 2.0 - 1.0));
}

vec3 normalMap(mat3 tbn, vec4 N) {
	return normalMap(tbn, N.xyz);
}

float lambert(vec3 n, vec3 l) {
	return saturate(dot(n, l));
}

vec3 worldPosition(mat4 projection, mat4 view, vec2 uv, float z) {
	mat4 vp = projection * view;
	vec4 wp = inverse(vp) * vec4(uv * 2.0 - 1.0, z * 2.0 - 1.0, 1.0);
	return wp.xyz / wp.w;
}

float Sqr(float x) { return x * x; }

#define MAX_LIGHT_RANGE 256.0

float lightAttenuation(Light light, vec3 L, float dist) {
	float r = light.radius;
//	float d = max(dist - r, 0.0);

//	// calculate basic attenuation
//	float denom = d / r + 1.0;
//	float attenuation = 1.0 / Sqr(denom);

//	// scale and bias attenuation such that:
//	//   attenuation == 0 at extent of max influence
//	//   attenuation == 1 when d == 0
//	attenuation = (attenuation - 0.0005) / (1.0 - 0.0005);
//	attenuation = max(attenuation, 0.0);

//	return attenuation;

	return Sqr(clamp(1.0 - Sqr(dist) / Sqr(r), 0.0, 1.0));

//	return smoothstep(light.radius, 0, dist);

//	return max(1.0 / Sqr(dist) - 1.0 / Sqr(light.radius), 0.0);

//	return 1.0f / Sqr((dist / light.radius) + 1.0);

}

vec2 encodeNormals(vec3 n) {
	vec2 enc = normalize(n.xy) * (sqrt(-n.z*0.5+0.5));
	enc = enc*0.5+0.5;
	return enc;
}

vec3 decodeNormals(vec4 enc) {
	vec4 nn = enc*vec4(2,2,0,0) + vec4(-1,-1,1,-1);
	float l = dot(nn.xyz,-nn.xyw);
	nn.z = l;
	nn.xy *= sqrt(l);
	return nn.xyz * 2 + vec3(0,0,-1);
}

)"
