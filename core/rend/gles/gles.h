#pragma once
#include "hw/pvr/ta_structs.h"
#include "hw/pvr/ta_ctx.h"
#include "hw/pvr/elan_struct.h"
#include "rend/TexCache.h"
#include "wsi/gl_context.h"
#include "glcache.h"
#include "postprocess.h"
#include "rend/shader_util.h"
#ifndef LIBRETRO
#include "rend/imgui_driver.h"
#endif

#include <unordered_map>
#include <glm/glm.hpp>

#ifndef GL_TEXTURE_MAX_ANISOTROPY
#define GL_TEXTURE_MAX_ANISOTROPY         0x84FE
#endif
#ifndef GL_MAX_TEXTURE_MAX_ANISOTROPY
#define GL_MAX_TEXTURE_MAX_ANISOTROPY     0x84FF
#endif

#define glCheck() do { if (unlikely(config::OpenGlChecks)) { verify(glGetError()==GL_NO_ERROR); } } while(0)

#define VERTEX_POS_ARRAY 0
#define VERTEX_COL_BASE_ARRAY 1
#define VERTEX_COL_OFFS_ARRAY 2
#define VERTEX_UV_ARRAY 3
// OIT only
#define VERTEX_COL_BASE1_ARRAY 4
#define VERTEX_COL_OFFS1_ARRAY 5
#define VERTEX_UV1_ARRAY 6
// Naomi2
#define VERTEX_NORM_ARRAY 7

//vertex types
extern u32 gcflip;

extern glm::mat4 ViewportMatrix;

void DrawStrips();

struct PipelineShader
{
	GLuint program;

	GLint depth_scale;
	GLint pp_ClipTest;
	GLint cp_AlphaTestValue;
	GLint sp_FOG_COL_RAM;
	GLint sp_FOG_COL_VERT;
	GLint sp_FOG_DENSITY;
	GLint trilinear_alpha;
	GLint fog_clamp_min, fog_clamp_max;
	GLint ndcMat;
	GLint palette_index;

	// Naomi2
	GLint mvMat;
	GLint normalMat;
	GLint projMat;
	GLint glossCoef[2];
	GLint envMapping[2];
	GLint bumpMapping;
	GLint constantColor[2];

	GLint lightCount;
	GLint ambientBase[2];
	GLint ambientOffset[2];
	GLint ambientMaterialBase[2];
	GLint ambientMaterialOffset[2];
	GLint useBaseOver;
	GLint bumpId0;
	GLint bumpId1;
	struct {
		GLint color;
		GLint direction;
		GLint position;
		GLint parallel;
		GLint diffuse[2];
		GLint specular[2];
		GLint routing;
		GLint dmode;
		GLint smode;
		GLint distAttnMode;
		GLint attnDistA;
		GLint attnDistB;
		GLint attnAngleA;
		GLint attnAngleB;
	} lights[elan::MAX_LIGHTS];

	const float *lastMvMat;
	const float *lastNormalMat;
	const float *lastProjMat;
	const N2LightModel *lastLightModel;

	//
	bool cp_AlphaTest;
	bool pp_InsideClipping;
	bool pp_Texture;
	bool pp_UseAlpha;
	bool pp_IgnoreTexA;
	u32 pp_ShadInstr;
	bool pp_Offset;
	u32 pp_FogCtrl;
	bool pp_Gouraud;
	bool pp_BumpMap;
	bool fog_clamping;
	bool trilinear;
	bool palette;
	bool naomi2;
	bool divPosZ;
};

class GlBuffer
{
public:
	GlBuffer(GLenum type, GLenum usage =  GL_STREAM_DRAW)
		: type(type), usage(usage), size(0) {
		glGenBuffers(1, &name);
	}

	~GlBuffer() {
		glDeleteBuffers(1, &name);
	}

	void bind() const {
		glBindBuffer(type, name);
	}

	GLuint getName() const {
		return name;
	}

	void update(const void *data, GLsizeiptr size)
	{
		bind();
		if (size > this->size)
		{
			glBufferData(type, size, data, usage);
			this->size = size;
		}
		else
		{
			glBufferSubData(type, 0, size, data);
		}
	}

private:
	GLenum type;
	GLenum usage;
	GLsizeiptr size;
	GLuint name;
};

struct gl_ctx
{
	struct
	{
		GLuint program;

		GLint depth_scale;
		GLint sp_ShaderColor;
		GLint ndcMat;
	} modvol_shader;

	struct
	{
		GLuint program;

		GLint depth_scale;
		GLint sp_ShaderColor;
		GLint ndcMat;

		GLint mvMat;
		GLint projMat;
	} n2ModVolShader;

	std::unordered_map<u32, PipelineShader> shaders;

	struct
	{
		GLuint program;
		GLint scale;
		GLuint vao;
		GLuint geometry;
		GLuint osd_tex;
	} OSD_SHADER;

	struct
	{
		GLuint mainVAO;
		GLuint modvolVAO;
		std::unique_ptr<GlBuffer> geometry;
		std::unique_ptr<GlBuffer> modvols;
		std::unique_ptr<GlBuffer> idxs;
		std::unique_ptr<GlBuffer> idxs2;
	} vbo;

	struct
	{
		u32 texAddress = ~0;
		GLuint depthb;
		GLuint tex;
		GLuint fbo;
		GLuint pbo;
		u32 pboSize;
		bool directXfer;
		u32 width;
		u32 height;
		FB_W_CTRL_type fb_w_ctrl;
		u32 linestride;
	} rtt;

	struct
	{
		GLuint depthb;
		GLuint colorb;
		GLuint tex;
		GLuint fbo;
		int width;
		int height;
		GLuint origFbo;
	} ofbo;

	const char *gl_version;
	const char *glsl_version_header;
	int gl_major;
	int gl_minor;
	bool is_gles;
	GLuint single_channel_format;
	GLenum index_type;
	bool GL_OES_packed_depth_stencil_supported;
	bool GL_OES_depth24_supported;
	bool highp_float_supported;
	float max_anisotropy;
	bool mesa_nouveau;
	bool border_clamp_supported;

	size_t get_index_size() { return index_type == GL_UNSIGNED_INT ? sizeof(u32) : sizeof(u16); }
};

extern gl_ctx gl;
extern GLuint fbTextureId;

BaseTextureCacheData *gl_GetTexture(TSP tsp, TCW tcw);

enum ModifierVolumeMode { Xor, Or, Inclusion, Exclusion, ModeCount };

void gl_load_osd_resources();
void gl_free_osd_resources();
bool ProcessFrame(TA_context* ctx);
void UpdateFogTexture(u8 *fog_table, GLenum texture_slot, GLint fog_image_format);
void UpdatePaletteTexture(GLenum texture_slot);
void termGLCommon();
void findGLVersion();
void GetFramebufferScaling(float& scale_x, float& scale_y, float& scissoring_scale_x, float& scissoring_scale_y);
void GetFramebufferSize(float& dc_width, float& dc_height);
void SetupMatrices(float dc_width, float dc_height,
				   float scale_x, float scale_y, float scissoring_scale_x, float scissoring_scale_y,
				   float &ds2s_offs_x, glm::mat4& ndcMat, glm::mat4& scissor_mat);

void SetCull(u32 CullMode);
s32 SetTileClip(u32 val, GLint uniform);
void SetMVS_Mode(ModifierVolumeMode mv_mode, ISP_Modvol ispc);

GLuint BindRTT(bool withDepthBuffer = true);
void ReadRTTBuffer();
void RenderFramebuffer();
void DrawFramebuffer();
GLuint init_output_framebuffer(int width, int height);
bool render_output_framebuffer();
void free_output_framebuffer();

void OSD_DRAW(bool clear_screen);
PipelineShader *GetProgram(bool cp_AlphaTest, bool pp_InsideClipping,
		bool pp_Texture, bool pp_UseAlpha, bool pp_IgnoreTexA, u32 pp_ShadInstr, bool pp_Offset,
		u32 pp_FogCtrl, bool pp_Gouraud, bool pp_BumpMap, bool fog_clamping, bool trilinear,
		bool palette, bool naomi2);

GLuint gl_CompileShader(const char* shader, GLuint type);
GLuint gl_CompileAndLink(const char *vertexShader, const char *fragmentShader);
bool CompilePipelineShader(PipelineShader* s);
extern const char* GouraudSource;

extern struct ShaderUniforms_t
{
	float PT_ALPHA;
	float depth_coefs[4];
	float fog_den_float;
	float ps_FOG_COL_RAM[3];
	float ps_FOG_COL_VERT[3];
	float trilinear_alpha;
	float fog_clamp_min[4];
	float fog_clamp_max[4];
	glm::mat4 ndcMat;
	struct {
		bool enabled;
		int x;
		int y;
		int width;
		int height;
	} base_clipping;
	int palette_index;

	void Set(const PipelineShader* s)
	{
		if (s->cp_AlphaTestValue!=-1)
			glUniform1f(s->cp_AlphaTestValue,PT_ALPHA);

		if (s->depth_scale!=-1)
			glUniform4fv( s->depth_scale, 1, depth_coefs);

		if (s->sp_FOG_DENSITY!=-1)
			glUniform1f( s->sp_FOG_DENSITY,fog_den_float);

		if (s->sp_FOG_COL_RAM!=-1)
			glUniform3fv( s->sp_FOG_COL_RAM, 1, ps_FOG_COL_RAM);

		if (s->sp_FOG_COL_VERT!=-1)
			glUniform3fv( s->sp_FOG_COL_VERT, 1, ps_FOG_COL_VERT);

		if (s->fog_clamp_min != -1)
			glUniform4fv(s->fog_clamp_min, 1, fog_clamp_min);
		if (s->fog_clamp_max != -1)
			glUniform4fv(s->fog_clamp_max, 1, fog_clamp_max);

		if (s->ndcMat != -1)
			glUniformMatrix4fv(s->ndcMat, 1, GL_FALSE, &ndcMat[0][0]);

		if (s->palette_index != -1)
			glUniform1i(s->palette_index, palette_index);
	}

} ShaderUniforms;

class TextureCacheData final : public BaseTextureCacheData
{
public:
	TextureCacheData(TSP tsp, TCW tcw) : BaseTextureCacheData(tsp, tcw), texID(glcache.GenTexture()) {
	}
	TextureCacheData(TextureCacheData&& other) : BaseTextureCacheData(std::move(other)) {
		std::swap(texID, other.texID);
	}

	GLuint texID;   //gl texture
	std::string GetId() override { return std::to_string(texID); }
	void UploadToGPU(int width, int height, const u8 *temp_tex_buffer, bool mipmapped, bool mipmapsIncluded = false) override;
	bool Delete() override;
};

class GlTextureCache final : public BaseTextureCache<TextureCacheData>
{
public:
	void Cleanup()
	{
		if (!texturesToDelete.empty())
		{
			glcache.DeleteTextures((GLsizei)texturesToDelete.size(), &texturesToDelete[0]);
			texturesToDelete.clear();
		}
		CollectCleanup();
	}
	void DeleteLater(GLuint texId) { texturesToDelete.push_back(texId); }

private:
	std::vector<GLuint> texturesToDelete;
};
extern GlTextureCache TexCache;

extern const u32 Zfunction[8];
extern const u32 SrcBlendGL[], DstBlendGL[];

struct OpenGLRenderer : Renderer
{
	bool Init() override;
	void Resize(int w, int h) override { width = w; height = h; }
	void Term() override;

	bool Process(TA_context* ctx) override;

	bool Render() override;

	bool RenderLastFrame() override;

	void DrawOSD(bool clear_screen) override { OSD_DRAW(clear_screen); }

	BaseTextureCacheData *GetTexture(TSP tsp, TCW tcw) override
	{
		return gl_GetTexture(tsp, tcw);
	}

	bool Present() override
	{
		if (!frameRendered)
			return false;
#ifndef LIBRETRO
		imguiDriver->setFrameRendered();
#endif
		frameRendered = false;
		return true;
	}

	virtual GLenum getFogTextureSlot() const {
		return GL_TEXTURE1;
	}
	virtual GLenum getPaletteTextureSlot() const {
		return GL_TEXTURE2;
	}

	bool frameRendered = false;
	int width;
	int height;
};

void initQuad();
void termQuad();
void drawQuad(GLuint texId, bool rotate = false, bool swapY = false);

extern const char* ShaderCompatSource;
extern const char *VertexCompatShader;
extern const char *PixelCompatShader;

class OpenGlSource : public ShaderSource
{
public:
	OpenGlSource() : ShaderSource(gl.glsl_version_header) {
		addConstant("TARGET_GL", gl.gl_version);
		addSource(ShaderCompatSource);
	}
};

#ifdef LIBRETRO
extern "C" struct retro_hw_render_callback hw_render;
void termVmuLightgun();
#endif

inline static void bindVertexArray(GLuint vao)
{
#ifndef GLES2
	if (gl.gl_major >= 3)
		glBindVertexArray(vao);
#endif
}

inline static void deleteVertexArray(GLuint vao)
{
#ifndef GLES2
	if (gl.gl_major >= 3)
		glDeleteVertexArrays(1, &vao);
#endif
}
