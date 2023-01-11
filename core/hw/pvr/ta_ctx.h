#pragma once
#include "types.h"
#include "ta_structs.h"
#include "pvr_regs.h"
#include "helper_classes.h"
#include "stdclass.h"
#include "oslib/oslib.h"

#include <mutex>

class BaseTextureCacheData;
struct N2LightModel;

//Vertex storage types
struct Vertex
{
	float x,y,z;

	u8 col[4];
	u8 spc[4];

	float u,v;

	// Two volumes format
	u8 col1[4];
	u8 spc1[4];

	float u1,v1;

	// Naomi2 normal
	float nx,ny,nz;
};

struct PolyParam
{
	u32 first;		//entry index , holds vertex/pos data
	u32 count;

	BaseTextureCacheData *texture;

	TSP tsp;
	TCW tcw;
	PCW pcw;
	ISP_TSP isp;
	float zvZ;
	u32 tileclip;
	//float zMin,zMax;
	TSP tsp1;
	TCW tcw1;
	BaseTextureCacheData *texture1;

	const float *mvMatrix;
	const float *normalMatrix;
	const float *projMatrix;
	float glossCoef[2];
	const N2LightModel *lightModel;
	bool envMapping[2];
	bool constantColor[2];

	void init()
	{
		first = 0;
		count = 0;
		texture = nullptr;
		tsp.full = 0;
		tcw.full = 0;
		pcw.full = 0;
		isp.full = 0;
		zvZ = 0;
		tileclip = 0;
		tsp1.full = -1;
		tcw1.full = -1;
		texture1 = nullptr;

		mvMatrix = nullptr;
		normalMatrix = nullptr;
		projMatrix = nullptr;
		glossCoef[0] = 0;
		glossCoef[1] = 0;
		lightModel = nullptr;
		envMapping[0] = false;
		envMapping[1] = false;
		constantColor[0] = false;
		constantColor[1] = false;
	}

	bool equivalentIgnoreCullingDirection(const PolyParam& other) const
	{
		return ((pcw.full ^ other.pcw.full) & 0x300CE) == 0
			&& ((isp.full ^ other.isp.full) & 0xF4000000) == 0
			&& tcw.full == other.tcw.full
			&& tsp.full == other.tsp.full
			&& tileclip == other.tileclip
			&& tcw1.full == other.tcw1.full
			&& tsp1.full == other.tsp1.full
			&& mvMatrix == other.mvMatrix
			&& normalMatrix == other.normalMatrix
			&& projMatrix == other.projMatrix
			&& glossCoef[0] == other.glossCoef[0]
			&& glossCoef[1] == other.glossCoef[1]
			&& lightModel == other.lightModel
			&& envMapping[0] == other.envMapping[0]
			&& constantColor[0] == other.constantColor[0]
			&& envMapping[1] == other.envMapping[1]
			&& constantColor[1] == other.constantColor[1];
	}

	bool isNaomi2() const { return projMatrix != nullptr; }
};

struct ModifierVolumeParam
{
	u32 first;
	u32 count;
	ISP_Modvol isp;

	const float *mvMatrix;
	const float *projMatrix;

	void init()
	{
		first = 0;
		count = 0;
		isp.full = 0;
		mvMatrix = nullptr;
		projMatrix = nullptr;
	}

	bool isNaomi2() const { return projMatrix != nullptr; }
};

struct ModTriangle
{
	f32 x0,y0,z0,x1,y1,z1,x2,y2,z2;
};

constexpr size_t MAX_PASSES = 10;

struct  tad_context
{
	u8* thd_data;
	u8* thd_root;
	u8* thd_old_data;

	void Clear()
	{
		thd_old_data = thd_data = thd_root;
	}

	void ClearPartial()
	{
		thd_old_data = thd_data;
		thd_data = thd_root;
	}

	u8* End()
	{
		return thd_data == thd_root ? thd_old_data : thd_data;
	}

	void Reset(u8* ptr)
	{
		thd_root = ptr;
		Clear();
	}
};

struct RenderPass {
	bool autosort;
	bool z_clear;
	bool mv_op_tr_shared;	// Use opaque modvols geometry for translucent modvols
	u32 op_count;
	u32 mvo_count;
	u32 pt_count;
	u32 tr_count;
	u32 mvo_tr_count;
};

struct N2Matrix
{
	float mat[16];
};

struct N2Light
{
	float color[4];
	float direction[4];	// For parallel/spot
	float position[4];		// For spot/point

	int parallel;
	int routing;
	int dmode;
	int smode;

	int diffuse[2];
	int specular[2];

	float attnDistA;
	float attnDistB;
	float attnAngleA;	// For spot
	float attnAngleB;
	int distAttnMode;	// For spot/point
	int _pad[3];
};

struct N2LightModel
{
	N2Light lights[16];

	float ambientBase[2][4];	// base ambient colors
	float ambientOffset[2][4];	// offset ambient colors
	int ambientMaterialBase[2];	// base ambient light is multiplied by model material/color
	int ambientMaterialOffset[2];// offset ambient light is multiplied by model material/color

	int lightCount;
	int useBaseOver;			// base color overflows into offset color
	int bumpId1;				// Light index for vol0 bump mapping
	int bumpId2;				// Light index for vol1 bump mapping
};

struct rend_context
{
	u8* proc_start;
	u8* proc_end;

	f32 fZ_min;
	f32 fZ_max;

	bool Overrun;
	bool isRTT;
	bool isRenderFramebuffer;
	
	FB_X_CLIP_type fb_X_CLIP;
	FB_Y_CLIP_type fb_Y_CLIP;
	u32 fb_W_LINESTRIDE;
	u32 fb_W_SOF1;
	FB_W_CTRL_type fb_W_CTRL;
	
	RGBAColor fog_clamp_min;
	RGBAColor fog_clamp_max;

	List<Vertex>      verts;
	List<u32>         idx;
	List<ModTriangle> modtrig;
	List<ModifierVolumeParam>  global_param_mvo;
	List<ModifierVolumeParam>  global_param_mvo_tr;

	List<PolyParam>   global_param_op;
	List<PolyParam>   global_param_pt;
	List<PolyParam>   global_param_tr;
	List<RenderPass>  render_passes;

	List<N2Matrix> matrices;
	List<N2LightModel> lightModels;

	void Clear()
	{
		verts.Clear();
		idx.Clear();
		global_param_op.Clear();
		global_param_pt.Clear();
		global_param_tr.Clear();
		modtrig.Clear();
		global_param_mvo.Clear();
		global_param_mvo_tr.Clear();
		render_passes.Clear();

		// Reserve space for background poly
		global_param_op.Append()->init();
		verts.Append(4);

		Overrun = false;
		fZ_min = 1000000.0f;
		fZ_max = 1.0f;
		isRenderFramebuffer = false;
		matrices.Clear();
		lightModels.Clear();
	}

	void newRenderPass();

	u32 getFramebufferWidth() const {
		u32 w = fb_X_CLIP.max + 1;
		if (fb_W_LINESTRIDE != 0)
			// Happens for Flag to Flag, Virtua Tennis?
			w = std::min(fb_W_LINESTRIDE * 4, w);
		return w;
	}
	u32 getFramebufferHeight() const {
		return fb_Y_CLIP.max + 1;
	}
};

#define TA_DATA_SIZE (8 * 1024 * 1024)

//vertex lists
struct TA_context
{
	u32 Address;

	std::mutex rend_inuse;

	tad_context tad;
	rend_context rend;

	TA_context *nextContext = nullptr;
	/*
		Dreamcast games use up to 20k vtx, 30k idx, 1k (in total) parameters.
		at 30 fps, thats 600kvtx (900 stripped)
		at 20 fps thats 1.2M vtx (~ 1.8M stripped)

		allocations allow much more than that !

		some stats:
			recv:   idx: 33528, vtx: 23451, op: 128, pt: 4, tr: 133, mvo: 14, modt: 342
			sc:     idx: 26150, vtx: 17417, op: 162, pt: 12, tr: 244, mvo: 6, modt: 2044
			doa2le: idx: 47178, vtx: 34046, op: 868, pt: 0, tr: 354, mvo: 92, modt: 976 (overruns)
			ika:    idx: 46748, vtx: 33818, op: 984, pt: 9, tr: 234, mvo: 10, modt: 16, ov: 0  
			ct:     idx: 30920, vtx: 21468, op: 752, pt: 0, tr: 360, mvo: 101, modt: 732, ov: 0
			sa2:    idx: 36094, vtx: 24520, op: 1330, pt: 10, tr: 177, mvo: 39, modt: 360, ov: 0
	*/

	void MarkRend()
	{
		rend.proc_start = tad.thd_root;
		rend.proc_end = tad.End();
	}

	void Alloc()
	{
		tad.Reset((u8*)allocAligned(32, TA_DATA_SIZE));

		rend.verts.Init(320 * 1024, &rend.Overrun, "verts");
		rend.idx.Init(320 * 1024, &rend.Overrun, "idx");
		rend.global_param_op.Init(16384, &rend.Overrun, "global_param_op");
		rend.global_param_pt.Init(5120, &rend.Overrun, "global_param_pt");
		rend.global_param_mvo.Init(4096, &rend.Overrun, "global_param_mvo");
		rend.global_param_tr.Init(10240, &rend.Overrun, "global_param_tr");
		rend.global_param_mvo_tr.Init(4096, &rend.Overrun, "global_param_mvo_tr");

		rend.modtrig.Init(16384, &rend.Overrun, "modtrig");
		
		rend.render_passes.Init(sizeof(RenderPass) * MAX_PASSES, &rend.Overrun, "render_passes");	// 10 render passes
		rend.matrices.Init(2000, &rend.Overrun, "matrices");
		rend.lightModels.Init(150, &rend.Overrun, "lightModels");

		Reset();
	}

	void Reset()
	{
		verify(tad.End() - tad.thd_root <= TA_DATA_SIZE);
		tad.Clear();
		nextContext = nullptr;
		rend_inuse.lock();
		rend.Clear();
		rend.proc_end = rend.proc_start = tad.thd_root;
		rend_inuse.unlock();
	}

	~TA_context()
	{
		verify(tad.End() - tad.thd_root <= TA_DATA_SIZE);
		freeAligned(tad.thd_root);
		rend.verts.Free();
		rend.idx.Free();
		rend.global_param_op.Free();
		rend.global_param_pt.Free();
		rend.global_param_tr.Free();
		rend.modtrig.Free();
		rend.global_param_mvo.Free();
		rend.global_param_mvo_tr.Free();
		rend.render_passes.Free();
		rend.matrices.Free();
		rend.lightModels.Free();
	}
};

extern TA_context* ta_ctx;
extern tad_context ta_tad;

TA_context* tactx_Pop(u32 addr);
void tactx_Term();
TA_context *tactx_Alloc();

/*
	Ta Context

	Rend Context
*/

#define TACTX_NONE (0xFFFFFFFF)

void SetCurrentTARC(u32 addr);
bool QueueRender(TA_context* ctx);
TA_context* DequeueRender();
void FinishRender(TA_context* ctx);

//must be moved to proper header
void FillBGP(TA_context* ctx);
bool rend_framePending();
void SerializeTAContext(Serializer& ser);
void DeserializeTAContext(Deserializer& deser);

void ta_add_poly(const PolyParam& pp);
void ta_add_poly(int listType, const ModifierVolumeParam& mvp);
void ta_add_vertex(const Vertex& vtx);
void ta_add_triangle(const ModTriangle& tri);
float* ta_add_matrix(const float *matrix);
N2LightModel *ta_add_light(const N2LightModel& light);
u32 ta_add_ta_data(u32 *data, u32 size);
int getTAContextAddresses(u32 *addresses);
u32 ta_get_tileclip();
void ta_set_tileclip(u32 tileclip);
u32 ta_get_list_type();
void ta_set_list_type(u32 listType);
void ta_parse_reset();

class TAParserException : public FlycastException
{
public:
	TAParserException() : FlycastException("") {}
};
