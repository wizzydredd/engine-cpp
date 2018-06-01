#include "imm.h"
#include "../math/consts.h"

#include "../core/logging/log.h"

#include <iterator>

NS_BEGIN

Vector<ImmDrawable> Imm::g_drawables;
Vector<ImmBatch> Imm::g_batches;
Vector<ImmVertex> Imm::g_vertices;
Vector<u32> Imm::g_indices;
PrimitiveType Imm::g_beginPrimitive = PrimitiveType::Triangles;
float Imm::g_lineWidth = 1.0f;

VertexArray Imm::g_vao;
VertexBuffer Imm::g_vbo;
VertexBuffer Imm::g_ibo;
ShaderProgram Imm::g_shader;

Mat4 Imm::g_modelMatrix = Mat4(1.0f);
Mat4 Imm::g_viewMatrix = Mat4(1.0f);
bool Imm::g_noDepth = false;

static const String ImmVS = R"(#version 330 core
layout (location = 0) in vec3 vPosition;
layout (location = 1) in vec4 vColor;

uniform mat4 mProjection;
uniform mat4 mView;

out vec4 oColor;

void main() {
	gl_Position = mProjection * mView * vec4(vPosition, 1.0);
	oColor = vColor;
}
)";

static const String ImmFS = R"(#version 330 core
out vec4 fragColor;

in vec4 oColor;

void main() {
	fragColor = oColor;
}
)";

void Imm::initialize() {
	g_vao = Builder<VertexArray>::build();
	g_vbo = Builder<VertexBuffer>::build();
	g_ibo = Builder<VertexBuffer>::build();

	g_vao.bind();
	g_vbo.bind(BufferType::ArrayBuffer);

	g_vbo.addVertexAttrib(0, 3, DataType::Float, false, sizeof(ImmVertex),  0);
	g_vbo.addVertexAttrib(1, 4, DataType::Float,  true, sizeof(ImmVertex), 12);

	g_ibo.bind(BufferType::IndexBuffer);

	g_vao.unbind();

	g_shader = Builder<ShaderProgram>::build();
	g_shader.add(ImmVS, ShaderType::VertexShader);
	g_shader.add(ImmFS, ShaderType::FragmentShader);
	g_shader.link();

	glEnable(GL_PRIMITIVE_RESTART);
	glPrimitiveRestartIndex(UINT_MAX);
}

void Imm::render(const Mat4& view, const Mat4& projection) {
	g_viewMatrix = view;

	generateBatches();

	g_vao.bind();

	g_shader.bind();
	g_shader.get("mProjection").set(projection);
	g_shader.get("mView").set(view);

	for (ImmBatch b : g_batches) {
		if (b.noDepth) glDisable(GL_DEPTH_TEST);
		glLineWidth(b.lineWidth);
		glDrawElements(b.primitiveType, b.indexCount, GL_UNSIGNED_INT, (void*)(4 * b.offset));
		if (b.noDepth) glEnable(GL_DEPTH_TEST);
	}

	g_shader.unbind();

	g_vao.unbind();

	g_drawables.clear();
	g_batches.clear();
}

void Imm::lineWidth(float value) {
	g_lineWidth = value;
}

void Imm::setModel(const Mat4& m) {
	g_modelMatrix = m;
}

void Imm::disableDepth() {
	g_noDepth = true;
}

void Imm::begin(PrimitiveType primitive) {
	assert(g_vertices.empty());
	g_beginPrimitive = primitive;
}

void Imm::end() {
	ImmDrawable dw;
	for (ImmVertex v : g_vertices) {
		ImmVertex nv;
		nv.color = v.color;
		nv.position = Vec3(g_modelMatrix * Vec4(v.position, 1.0f));
		dw.vertices.push_back(nv);
	}
	dw.indices.insert(dw.indices.end(), g_indices.begin(), g_indices.end());
	dw.primitiveType = g_beginPrimitive;
	dw.noDepth = g_noDepth;
	dw.lineWidth = g_lineWidth;

	g_vertices.clear();
	g_indices.clear();

	g_drawables.push_back(dw);

	g_modelMatrix = Mat4(1.0f);
	g_noDepth = false;
	g_lineWidth = 1.0f;
}

void Imm::vertex(const Vec3 &pos, const Vec4 &col, bool index) {
	ImmVertex vert;
	vert.position = pos;
	vert.color = col;

	if (index) g_indices.push_back(g_vertices.size());
	g_vertices.push_back(vert);
}

void Imm::vertex(const Vec3& pos, bool index) {
	vertex(pos, Vec4(1.0f), index);
}

void Imm::addIndex(u32 index) {
	g_indices.push_back(g_vertices.size() + index);
}

void Imm::addIndices(const Vector<u32>& indices) {
	for (u32 i : indices) addIndex(i);
}

void Imm::line(const Vec3& a, const Vec3& b, const Vec4 color) {
	vertex(a, color);
	vertex(b, color);
}

void Imm::cube(const Vec3& halfExtents, const Vec4 color) {
	const u32 indices[] = {
		// front
		0, 1, 2,
		2, 3, 0,
		// right
		1, 5, 6,
		6, 2, 1,
		// back
		7, 6, 5,
		5, 4, 7,
		// left
		4, 0, 3,
		3, 7, 4,
		// bottom
		4, 5, 1,
		1, 0, 4,
		// top
		3, 2, 6,
		6, 7, 3
	};

	Vector<u32> indices_v(std::begin(indices), std::end(indices));
	addIndices(indices_v);

	float x = halfExtents.x;
	float y = halfExtents.y;
	float z = halfExtents.z;

	vertex(Vec3(-x, -y, z), color, false);
	vertex(Vec3(x, -y, z), color, false);
	vertex(Vec3(x, y, z), color, false);
	vertex(Vec3(-x, y, z), color, false);
	vertex(Vec3(-x, -y, -z), color, false);
	vertex(Vec3(x, -y, -z), color, false);
	vertex(Vec3(x, y, -z), color, false);
	vertex(Vec3(-x, y, -z), color, false);
}

void Imm::sphere(const Vec3 &pos, float radius, const Vec4 &color, u32 stacks, u32 slices) {
	// Calc The Index Positions
	for (int i = 0; i < slices * stacks + slices; ++i){
		addIndex(i);
		addIndex(i + slices);
		addIndex(i + slices + 1);

		addIndex(i + slices + 1);
		addIndex(i);
		addIndex(i + 1);
	}

	// Calc The Vertices
	for (u32 i = 0; i <= stacks; ++i) {
		float V   = float(i) / float(stacks);
		float phi = V * Pi;

		// Loop Through Slices
		for (int j = 0; j <= slices; ++j) {
			float U = float(j) / float(slices);
			float theta = U * (Pi * 2.0f);

			// Calc The Vertex Positions
			float x = std::cos(theta) * std::sin(phi);
			float y = std::cos(phi);
			float z = std::sin(theta) * std::sin(phi);

			// Push Back Vertex Data
			vertex(pos + Vec3(x, y, z) * radius, color, false);
		}
	}
}

void Imm::cone(const Vec3& pos, const Vec3& dir, float base, float height, const Vec4& color) {
	const i32 slices = 24;

	u32 i = 0;
	for (i = 1; i < slices; i++) {
		addIndex(0); addIndex(i); addIndex(i+1);
	}
	addIndex(0); addIndex(i); addIndex(1);

	vertex(pos + dir * height, color, false);

	for (u32 i = 0; i < slices; i++) {
		float V   = float(i) / float(slices);
		float phi = V * TwoPi;
		float x = std::cos(phi);
		float z = std::sin(phi);
		Vec3 ax = Vec3(x, 0, z);
		vertex(pos + ax * base, color, false);
	}
}

void Imm::arrow(const Vec3& pos, const Vec3& dir, float len, const Vec4& color, float thickness) {

}

void Imm::generateBatches() {
	if (g_drawables.empty()) return;

	Vector<ImmVertex> vertices;
	Vector<u32> indices;

	ImmDrawable first = g_drawables[0];
	vertices.insert(vertices.end(), first.vertices.begin(), first.vertices.end());
	indices.insert(indices.end(), first.indices.begin(), first.indices.end());

	ImmBatch fb;
	fb.primitiveType = first.primitiveType;
	fb.noDepth = first.noDepth;
	fb.indexCount = first.indices.size();
	fb.offset = 0;
	g_batches.push_back(fb);

	u32 ioff = first.vertices.size();
	u32 off = 0;
	for (u32 i = 1; i < g_drawables.size(); i++) {
		ImmDrawable curr = g_drawables[i];
		ImmDrawable prev = g_drawables[i - 1];
		if (curr.primitiveType != prev.primitiveType ||
			curr.noDepth != prev.noDepth ||
			curr.lineWidth != prev.lineWidth) {
			off += g_batches.back().indexCount;
			ImmBatch b;
			b.primitiveType = curr.primitiveType;
			b.noDepth = curr.noDepth;
			b.indexCount = curr.indices.size();
			b.offset = off;
			b.lineWidth = curr.lineWidth;
			g_batches.push_back(b);
		} else {
			g_batches.back().indexCount += curr.indices.size();
		}

		vertices.insert(vertices.end(), curr.vertices.begin(), curr.vertices.end());
		for (u32 i : curr.indices) indices.push_back(i + ioff);

		ioff += curr.vertices.size();
	}

	g_vbo.bind();
	g_vbo.setData<ImmVertex>(vertices.size(), vertices.data(), BufferUsage::Dynamic);
	g_vbo.unbind();

	g_ibo.bind();
	g_ibo.setData<u32>(indices.size(), indices.data(), BufferUsage::Dynamic);
	g_ibo.unbind();
}

NS_END
