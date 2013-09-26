#include <list>
#include <deque>

class ShaderManager;
class LinkedShader;

class GLES_GPU : public GPUCommon
{
public:
	GLES_GPU();
	~GLES_GPU();
	virtual void InitClear();
	virtual void PreExecuteOp(u32 op, u32 diff);
	virtual void ExecuteOp(u32 op, u32 diff);

	virtual void SetDisplayFramebuffer(u32 framebuf, u32 stride, GEBufferFormat format);
	virtual void CopyDisplayToOutput();
	virtual void BeginFrame();
	virtual void UpdateStats();
	virtual void InvalidateCache(u32 addr, int size, GPUInvalidationType type);
	virtual void UpdateMemory(u32 dest, u32 src, int size);
	virtual void ClearCacheNextFrame();
	virtual void DeviceLost();

	virtual void DumpNextFrame();
	virtual void DoState(PointerWrap &p);

	virtual void Resized();
	virtual bool DecodeTexture(u8* dest, GPUgstate state) {
		return textureCache_.DecodeTexture(dest, state);
	}
	virtual bool FramebufferDirty();
	virtual bool FramebufferReallyDirty();

	virtual void GetReportingInfo(std::string &primaryInfo, std::string &fullInfo) {
		primaryInfo = reportingPrimaryInfo_;
		fullInfo = reportingFullInfo_;
	}
	std::vector<FramebufferInfo> GetFramebufferList();

	bool GetCurrentFramebuffer(GPUDebugBuffer &buffer);

protected:
	virtual void FastRunLoop(DisplayList &list);
	virtual void ProcessEvent(GPUEvent ev);

private:
	void Flush() {
		transformDraw_.Flush();
	}
	void DoBlockTransfer();
	void ApplyDrawState(int prim);
	void CheckFlushOp(int cmd, u32 diff);
	void BuildReportingInfo();
	void InitClearInternal();
	void BeginFrameInternal();
	void CopyDisplayToOutputInternal();
	void InvalidateCacheInternal(u32 addr, int size, GPUInvalidationType type);

	FramebufferManager framebufferManager_;
	TextureCache textureCache_;
	TransformDrawEngine transformDraw_;
	ShaderManager *shaderManager_;

	u8 *commandFlags_;

	bool resized_;
	int lastVsync_;

	std::string reportingPrimaryInfo_;
	std::string reportingFullInfo_;
};
