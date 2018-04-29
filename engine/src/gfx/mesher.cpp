#include "mesher.h"

#include "../core/logging/log.h"

NS_BEGIN

void VertexFormat::put(const String& name, AttributeType type, bool normalized, i32 location) {
	VertexAttribute attr;
	attr.size = type;
	attr.normalized = normalized;
	attr.location = location;
	attr.name = name;
	m_attributes.push_back(attr);

	m_stride += u32(type) * 4;
}

void VertexFormat::bind(ShaderProgram* shader) {
	u32 off = 0;
	bool shaderValid = shader != nullptr;
	for (auto attr : m_attributes) {
		i32 loc = attr.location;
		if (shaderValid && attr.location == -1) {
			loc = shader->getAttributeLocation(attr.name);
		}
		if (loc != -1) {
			glEnableVertexAttribArray(loc);
			glVertexAttribPointer(loc, attr.size, GL_FLOAT, attr.normalized, m_stride, ((void*) off));
		}
		off += attr.size * 4;
	}
}

void VertexFormat::unbind(ShaderProgram* shader) {
	if (shader == nullptr) return;
	for (auto attr : m_attributes) {
		i32 loc = attr.location;
		if (attr.location == -1) {
			loc = shader->getAttributeLocation(attr.name);
		}
		if (loc != -1) {
			glDisableVertexAttribArray(loc);
		}
	}
}

Mesh::~Mesh() {
	if (m_vbo) GLBuffer::destroy(m_vbo);
	if (m_ibo && m_indexed) GLBuffer::destroy(m_ibo);
	if (m_vao && m_useVertexArrays) GLVertexArray::destroy(m_vao);
}

Mesh::Mesh(bool indexed, bool vao)
	: m_indexed(indexed), m_useVertexArrays(vao)
{
	m_format = uptr<VertexFormat>(new VertexFormat());
	m_vbo = GLBuffer::create();
	if (indexed) m_ibo = GLBuffer::create();
	if (vao) m_vao = GLVertexArray::create();

	m_format->put("vPosition", AttributeType::AttrVector3, false, 0);
	m_format->put("vNormal", AttributeType::AttrVector3, false, 1);
	m_format->put("vTangent", AttributeType::AttrVector3, false, 2);
	m_format->put("vTexCoord", AttributeType::AttrVector2, false, 3);
	m_format->put("vColor", AttributeType::AttrVector4, false, 4);
}

void Mesh::bind(ShaderProgram* shader) {
	if (m_useVertexArrays) {
		glBindVertexArray(m_vao);
	} else {
		glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
		m_format->bind(shader);
		if (m_indexed) glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_ibo);
	}
}

void Mesh::unbind(ShaderProgram* shader) {
	if (m_useVertexArrays) {
		glBindVertexArray(0);
	} else {
		glBindBuffer(GL_ARRAY_BUFFER, 0);
		m_format->unbind(shader);
		if (m_indexed) glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
	}
}

u8* Mesh::map() {
	glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
	return (u8*) glMapBuffer(GL_ARRAY_BUFFER, GL_READ_WRITE);
}

void Mesh::unmap() {
	glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
	glUnmapBuffer(GL_ARRAY_BUFFER);
}

MeshFactory& MeshFactory::addVertex(const Vertex& vert) {
	m_vertexData.push_back(vert);
	return *this;
}

MeshFactory& MeshFactory::addIndex(i32 index) {
	m_indexData.push_back(index);
	return *this;
}

MeshFactory& MeshFactory::addTriangle(i32 i0, i32 i1, i32 i2) {
	m_indexData.push_back(i0);
	m_indexData.push_back(i1);
	m_indexData.push_back(i2);
	return *this;
}

MeshFactory& MeshFactory::addData(const Vector<Vertex>& vertices, const Vector<i32>& indices) {
	i32 off = m_vertexData.size();
	m_vertexData.insert(m_vertexData.end(), vertices.begin(), vertices.end());
	for (auto i : indices) {
		m_indexData.push_back(off + i);
	}
	return *this;
}

MeshFactory& MeshFactory::addFromFile(const String& file) {
	Assimp::Importer imp;
	const aiScene* scene = imp.ReadFile(
		file.c_str(),
		aiPostProcessSteps::aiProcess_Triangulate |
		aiPostProcessSteps::aiProcess_FlipUVs |
		aiPostProcessSteps::aiProcess_CalcTangentSpace |
		aiPostProcessSteps::aiProcess_FlipWindingOrder
	);
	if (scene == nullptr || scene->mFlags == AI_SCENE_FLAGS_INCOMPLETE) {
		LogError(imp.GetErrorString());
		return *this;
	}
	addAIScene(scene);

	return *this;
}

MeshFactory& MeshFactory::addFromFile(VirtualFile* file) {
	Assimp::Importer imp;
	const aiScene* scene = imp.ReadFileFromMemory(
		file->readAll(), file->size,
		aiPostProcessSteps::aiProcess_Triangulate |
		aiPostProcessSteps::aiProcess_FlipUVs |
		aiPostProcessSteps::aiProcess_CalcTangentSpace |
		aiPostProcessSteps::aiProcess_FlipWindingOrder
	);
	if (scene == nullptr) {
		LogError(imp.GetErrorString());
		return *this;
	}
	addAIScene(scene);

	return *this;
}

MeshFactory& MeshFactory::calculateNormals(PrimitiveType primitive) {
	switch (primitive) {
		case PrimitiveType::Points:
		case PrimitiveType::Lines:
		case PrimitiveType::LineLoop:
		case PrimitiveType::LineStrip:
			break;
		case PrimitiveType::Triangles:
		{
			for (u32 i = 0; i < m_indexData.size(); i += 3) {
				i32 i0 = index(i + 0);
				i32 i1 = index(i + 1);
				i32 i2 = index(i + 2);
				triNormal(i0, i1, i2);
			}
		} break;
		case PrimitiveType::TriangleFan:
		{
			for (u32 i = 0; i < m_indexData.size(); i += 2) {
				i32 i0 = index(0);
				i32 i1 = index(i);
				i32 i2 = index(i + 1);
				triNormal(i0, i1, i2);
			}
		} break;
		case PrimitiveType::TriangleStrip:
		{
			for (u32 i = 0; i < m_indexData.size(); i += 2) {
				i32 i0, i1, i2;
				if (i % 2 == 0) {
					i0 = index(i + 0);
					i1 = index(i + 1);
					i2 = index(i + 2);
				} else {
					i0 = index(i + 2);
					i1 = index(i + 1);
					i2 = index(i + 0);
				}
				triNormal(i0, i1, i2);
			}
		} break;
	}

	for (auto& v : m_vertexData) {
		v.normal = v.normal.normalized();
	}

	return *this;
}

MeshFactory& MeshFactory::calculateTangents(PrimitiveType primitive) {
	switch (primitive) {
		case PrimitiveType::Points:
		case PrimitiveType::Lines:
		case PrimitiveType::LineLoop:
		case PrimitiveType::LineStrip:
			break;
		case PrimitiveType::Triangles:
		{
			for (u32 i = 0; i < m_indexData.size(); i += 3) {
				i32 i0 = index(i + 0);
				i32 i1 = index(i + 1);
				i32 i2 = index(i + 2);
				triTangent(i0, i1, i2);
			}
		} break;
		case PrimitiveType::TriangleFan:
		{
			for (u32 i = 0; i < m_indexData.size(); i += 2) {
				i32 i0 = index(0);
				i32 i1 = index(i);
				i32 i2 = index(i + 1);
				triTangent(i0, i1, i2);
			}
		} break;
		case PrimitiveType::TriangleStrip:
		{
			for (u32 i = 0; i < m_indexData.size(); i += 2) {
				i32 i0, i1, i2;
				if (i % 2 == 0) {
					i0 = index(i + 0);
					i1 = index(i + 1);
					i2 = index(i + 2);
				} else {
					i0 = index(i + 2);
					i1 = index(i + 1);
					i2 = index(i + 0);
				}
				triTangent(i0, i1, i2);
			}
		} break;
	}

	for (auto& v : m_vertexData) {
		v.tangent = v.tangent.normalized();
	}

	return *this;
}

MeshFactory& MeshFactory::transformTexCoords(const Mat4& t) {
	for (auto& v : m_vertexData) {
		v.texCoord = (t * Vec4(v.texCoord.x, v.texCoord.y, 0.0f, 1.0f)).xy;
	}
	return *this;
}

uptr<Mesh> MeshFactory::build(bool indexed, bool vao) {
	if (m_vertexData.empty()) return nullptr;

	uptr<Mesh> mesh(new Mesh(indexed, vao));

	if (vao) {
		glBindVertexArray(mesh->m_vao);
		glBindBuffer(GL_ARRAY_BUFFER, mesh->m_vbo);

		mesh->m_format->bind();

		if (indexed) glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mesh->m_ibo);
		glBindVertexArray(0);
	}

	i32 vsize = mesh->m_format->stride() * m_vertexData.size();

	glBindBuffer(GL_ARRAY_BUFFER, mesh->m_vbo);
	glBufferData(GL_ARRAY_BUFFER, vsize, m_vertexData.data(), GL_STATIC_DRAW);

	if (indexed) {
		i32 isize = 4 * m_indexData.size();
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mesh->m_ibo);
		glBufferData(GL_ELEMENT_ARRAY_BUFFER, isize, m_indexData.data(), GL_STATIC_DRAW);
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
	}
	glBindBuffer(GL_ARRAY_BUFFER, 0);

	mesh->m_vertexCount = m_vertexData.size();
	mesh->m_indexCount = m_indexData.size();

	m_vertexData.clear();
	m_indexData.clear();

	return mesh;
}

void MeshFactory::triNormal(i32 i0, i32 i1, i32 i2) {
	Vec3 v0 = m_vertexData[i0].position;
	Vec3 v1 = m_vertexData[i1].position;
	Vec3 v2 = m_vertexData[i2].position;

	Vec3 e0 = v1 - v0;
	Vec3 e1 = v2 - v0;
	Vec3 n = e0.cross(e1);

	m_vertexData[i0].normal += n;
	m_vertexData[i1].normal += n;
	m_vertexData[i2].normal += n;
}

void MeshFactory::triTangent(i32 i0, i32 i1, i32 i2) {
	Vec3 v0 = m_vertexData[i0].position;
	Vec3 v1 = m_vertexData[i1].position;
	Vec3 v2 = m_vertexData[i2].position;

	Vec2 t0 = m_vertexData[i0].texCoord;
	Vec2 t1 = m_vertexData[i1].texCoord;
	Vec2 t2 = m_vertexData[i2].texCoord;

	Vec3 e0 = v1 - v0;
	Vec3 e1 = v2 - v0;
	
	Vec2 dt1 = t1 - t0;
	Vec2 dt2 = t2 - t0;

	float dividend = dt1.perpDot(dt2);
	float f = dividend == 0.0f ? 0.0f : 1.0f / dividend;

	Vec3 t = Vec3();

	t.x = (f * (dt2.y * e0.x - dt1.y * e1.x));
	t.y = (f * (dt2.y * e0.y - dt1.y * e1.y));
	t.z = (f * (dt2.y * e0.z - dt1.y * e1.z));

	m_vertexData[i0].tangent += t;
	m_vertexData[i1].tangent += t;
	m_vertexData[i2].tangent += t;
}

void MeshFactory::addAIScene(const aiScene* scene) {
	i32 off = m_vertexData.size();

	const aiVector3D aiZeroVector(0.0f, 0.0f, 0.0f);
	const aiColor4D aiOneVector4(1.0f, 1.0f, 1.0f, 1.0f);

	for (u32 m = 0; m < scene->mNumMeshes; m++) {
		aiMesh* mesh = scene->mMeshes[m];
		bool hasPositions = mesh->HasPositions();
		bool hasNormals = mesh->HasNormals();
		bool hasUVs = mesh->HasTextureCoords(0);
		bool hasTangents = mesh->HasTangentsAndBitangents();
		bool hasColors = mesh->HasVertexColors(0);

		for (u32 i = 0; i < mesh->mNumVertices; i++) {
			Vertex v;
			const aiVector3D pos = hasPositions ? mesh->mVertices[i] : aiZeroVector;
			const aiVector3D normal = hasNormals ? mesh->mNormals[i] : aiZeroVector;
			const aiVector3D texCoord = hasUVs ? mesh->mTextureCoords[0][i] : aiZeroVector;
			const aiVector3D tangent = hasTangents ? mesh->mTangents[i] : aiZeroVector;
			const aiColor4D color = hasColors ? mesh->mColors[0][i] : aiOneVector4;

			v.position = Vec3(pos.x, pos.y, pos.z);
			v.normal = Vec3(normal.x, normal.y, normal.z);
			v.texCoord = Vec2(texCoord.x, texCoord.y);
			v.tangent = Vec3(tangent.x, tangent.y, tangent.z);
			v.color = Vec4(color.r, color.g, color.b, color.a);

			addVertex(v);
		}

		for (u32 i = 0; i < mesh->mNumFaces; i++) {
			aiFace face = mesh->mFaces[i];
			for (u32 j = 0; j < face.mNumIndices; j++) {
				addIndex(off + face.mIndices[j]);
			}
		}

		off += m_vertexData.size();
	}
}

NS_END