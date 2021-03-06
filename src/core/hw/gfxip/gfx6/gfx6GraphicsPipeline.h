/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2014-2019 Advanced Micro Devices, Inc. All Rights Reserved.
 *
 *  Permission is hereby granted, free of charge, to any person obtaining a copy
 *  of this software and associated documentation files (the "Software"), to deal
 *  in the Software without restriction, including without limitation the rights
 *  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 *  copies of the Software, and to permit persons to whom the Software is
 *  furnished to do so, subject to the following conditions:
 *
 *  The above copyright notice and this permission notice shall be included in all
 *  copies or substantial portions of the Software.
 *
 *  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 *  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 *  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 *  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 *  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 *  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 *  SOFTWARE.
 *
 **********************************************************************************************************************/

#pragma once

#include "core/hw/gfxip/graphicsPipeline.h"
#include "core/hw/gfxip/gfx6/gfx6CmdUtil.h"
#include "core/hw/gfxip/gfx6/gfx6PipelineChunkEsGs.h"
#include "core/hw/gfxip/gfx6/gfx6PipelineChunkLsHs.h"
#include "core/hw/gfxip/gfx6/gfx6PipelineChunkVsPs.h"
#include "palPipelineAbiProcessor.h"

namespace Pal
{

class Platform;

namespace Gfx6
{

class ColorBlendState;
class DepthStencilState;
class DepthStencilView;
class GraphicsPipelineUploader;

// Contains information about the pipeline which needs to be passed to the Init methods or between the multiple Init
// phases.
struct GraphicsPipelineLoadInfo
{
    bool    usesOnchipTess;     // Set if the pipeline has a HS and uses on-chip HS.
    bool    usesGs;             // Set if the pipeline has a GS.
    bool    usesOnChipGs;       // Set if the pipeline has a GS and uses on-chip GS.
    uint16  esGsLdsSizeRegGs;   // User-SGPR where the ES/GS ring size in LDS is passed to the GS stage
    uint16  esGsLdsSizeRegVs;   // User-SGPR where the ES/GS ring size in LDS is passed to the VS stage
    uint16  interpolatorCount;  // Number of PS interpolators

    uint32  loadedShRegCount;   // Number of SH registers to load using LOAD_SH_REG_INDEX.  If zero, the LOAD_INDEX
                                // path for pipeline binds is not supported.
    uint32  loadedCtxRegCount;  // Number of constext registers to load using LOAD_CONTEXT_REG_INDEX.  If zero, the
                                // LOAD_INDEX path for pipeline binds is not supported.
};

// Contains graphics stage information calculated at pipeline bind time.
struct DynamicStageInfos
{
    DynamicStageInfo ps;
    DynamicStageInfo vs;
    DynamicStageInfo ls;
    DynamicStageInfo hs;
    DynamicStageInfo es;
    DynamicStageInfo gs;
};

// =====================================================================================================================
// GFX6 graphics pipeline class: implements common GFX6-specific funcionality for the GraphicsPipeline class.  Details
// specific to a particular pipeline configuration (GS-enabled, tessellation-enabled, etc) are offloaded to appropriate
// subclasses.
class GraphicsPipeline : public Pal::GraphicsPipeline
{
public:
    GraphicsPipeline(Device* pDevice, bool isInternal);

    virtual Result GetShaderStats(
        ShaderType   shaderType,
        ShaderStats* pShaderStats,
        bool         getDisassemblySize) const override;

    uint32* Prefetch(uint32* pCmdSpace) const;

    template <bool pm4OptImmediate>
    uint32* WriteDbShaderControl(
        bool       isDepthEnabled,
        bool       usesOverRasterization,
        CmdStream* pCmdStream,
        uint32*    pCmdSpace) const;

    regPA_SC_MODE_CNTL_1 PaScModeCntl1() const { return m_paScModeCntl1; }

    regIA_MULTI_VGT_PARAM IaMultiVgtParam(bool forceWdSwitchOnEop) const
        { return m_iaMultiVgtParam[IaRegIdx(forceWdSwitchOnEop)]; }

    regVGT_LS_HS_CONFIG VgtLsHsConfig() const { return m_vgtLsHsConfig; }

    bool CanDrawPrimsOutOfOrder(const DepthStencilView*  pDsView,
                                const DepthStencilState* pDepthStencilState,
                                const ColorBlendState*   pBlendState,
                                uint32                   hasActiveQueries,
                                OutOfOrderPrimMode       gfx7EnableOutOfOrderPrimitives) const;

    bool IsOutOfOrderPrimsEnabled() const
        { return m_paScModeCntl1.bits.OUT_OF_ORDER_PRIMITIVE_ENABLE; }

    regVGT_STRMOUT_BUFFER_CONFIG VgtStrmoutBufferConfig() const { return m_chunkVsPs.VgtStrmoutBufferConfig(); }
    regVGT_STRMOUT_VTX_STRIDE_0 VgtStrmoutVtxStride(uint32 idx) const { return m_chunkVsPs.VgtStrmoutVtxStride(idx); }
    regSPI_VS_OUT_CONFIG SpiVsOutConfig() const { return m_chunkVsPs.SpiVsOutConfig(); }
    regSPI_PS_IN_CONTROL SpiPsInControl() const { return m_chunkVsPs.SpiPsInControl(); }

    regSX_PS_DOWNCONVERT__VI SxPsDownconvert() const { return m_sxPsDownconvert; }
    regSX_BLEND_OPT_EPSILON__VI SxBlendOptEpsilon() const { return m_sxBlendOptEpsilon; }
    regSX_BLEND_OPT_CONTROL__VI SxBlendOptControl() const { return m_sxBlendOptControl; }

    const GraphicsPipelineSignature& Signature() const { return m_signature; }

    bool UsesViewInstancing() const { return (m_signature.viewIdRegAddr[0] != UserDataNotMapped); }

    uint32* WriteShCommands(
        CmdStream*                        pCmdStream,
        uint32*                           pCmdSpace,
        const DynamicGraphicsShaderInfos& graphicsInfo) const;

    uint32* WriteContextCommands(CmdStream* pCmdStream, uint32* pCmdSpace) const;

    uint64 GetContextPm4ImgHash() const { return m_contextRegHash; }

    void OverrideRbPlusRegistersForRpm(
        SwizzledFormat               swizzledFormat,
        uint32                       slot,
        regSX_PS_DOWNCONVERT__VI*    pSxPsDownconvert,
        regSX_BLEND_OPT_EPSILON__VI* pSxBlendOptEpsilon,
        regSX_BLEND_OPT_CONTROL__VI* pSxBlendOptControl) const;

    uint32 GetVsUserDataBaseOffset() const;

protected:
    virtual ~GraphicsPipeline() { }

    virtual Result HwlInit(
        const GraphicsPipelineCreateInfo& createInfo,
        const AbiProcessor&               abiProcessor,
        const CodeObjectMetadata&         metadata,
        Util::MsgPackReader*              pMetadataReader) override;

    virtual const ShaderStageInfo* GetShaderStageInfo(ShaderType shaderType) const override;

private:
    void EarlyInit(
        const CodeObjectMetadata& metadata,
        const RegisterVector&     registers,
        GraphicsPipelineLoadInfo* pInfo);

    uint32 CalcMaxWavesPerSh(uint32 maxWavesPerCu) const;

    void CalcDynamicStageInfo(
        const DynamicGraphicsShaderInfo& shaderInfo,
        DynamicStageInfo*                pStageInfo) const;
    void CalcDynamicStageInfos(
        const DynamicGraphicsShaderInfos& graphicsInfo,
        DynamicStageInfos*                pStageInfos) const;

    void UpdateRingSizes(
        const CodeObjectMetadata& metadata);
    uint32 ComputeScratchMemorySize(
        const CodeObjectMetadata& metadata) const;

    void SetupSignatureFromElf(
        const CodeObjectMetadata& metadata,
        const RegisterVector&     registers,
        uint16*                   pEsGsLdsSizeRegGs,
        uint16*                   pEsGsLdsSizeRegVs);
    void SetupSignatureForStageFromElf(
        const CodeObjectMetadata& metadata,
        const RegisterVector&     registers,
        HwShaderStage             stage,
        uint16*                   pEsGsLdsSizeReg);

    void BuildPm4Headers(
        const GraphicsPipelineUploader& uploader);

    void SetupCommonRegisters(
        const GraphicsPipelineCreateInfo& createInfo,
        const RegisterVector&             registers,
        GraphicsPipelineUploader*         pUploader);
    void SetupNonShaderRegisters(
        const GraphicsPipelineCreateInfo& createInfo,
        const RegisterVector&             registers,
        GraphicsPipelineUploader*         pUploader);

    void SetupIaMultiVgtParam(
        const RegisterVector& registers);
    void FixupIaMultiVgtParamOnGfx7Plus(
        bool                   forceWdSwitchOnEop,
        regIA_MULTI_VGT_PARAM* pIaMultiVgtParam) const;

    void SetupLateAllocVs(
        const RegisterVector&     registers,
        GraphicsPipelineUploader* pUploader);

    void SetupRbPlusRegistersForSlot(
        uint32                       slot,
        uint8                        writeMask,
        SwizzledFormat               swizzledFormat,
        regSX_PS_DOWNCONVERT__VI*    pSxPsDownconvert,
        regSX_BLEND_OPT_EPSILON__VI* pSxBlendOptEpsilon,
        regSX_BLEND_OPT_CONTROL__VI* pSxBlendOptControl) const;

    // Pre-assembled "images" of the PM4 packets used for binding this pipeline to a command buffer.
    struct Pm4Commands
    {
        struct
        {
            PM4CONTEXTREGRMW  dbAlphaToMask;
            PM4CONTEXTREGRMW  dbRenderOverride;
        } common; // Packets which are common to both the SET and LOAD_INDEX paths (such as read-modify-writes).

        struct
        {
            struct
            {
                PM4CMDLOADDATAINDEX  loadShRegIndex;
            } sh;

            struct
            {
                PM4CMDLOADDATAINDEX  loadCtxRegIndex;
            } context;
        } loadIndex; // LOAD_INDEX path, used for GPU's which support the updated microcode.

        PipelinePrefetchPm4 prefetch;

        struct
        {
            struct
            {
                PM4CMDSETDATA                        hdrSpiShaderLateAllocVs;
                regSPI_SHADER_LATE_ALLOC_VS__CI__VI  spiShaderLateAllocVs;
            } sh;

            struct
            {
                PM4CMDSETDATA            hdrVgtShaderStagesEn;
                regVGT_SHADER_STAGES_EN  vgtShaderStagesEn;

                PM4CMDSETDATA   hdrVgtGsMode;
                regVGT_GS_MODE  vgtGsMode;

                PM4CMDSETDATA     hdrVgtReuseOff;
                regVGT_REUSE_OFF  vgtReuseOff;

                PM4CMDSETDATA    hdrVgtTfParam;
                regVGT_TF_PARAM  vgtTfParam;

                PM4CMDSETDATA        hdrCbColorControl;
                regCB_COLOR_CONTROL  cbColorControl;

                PM4CMDSETDATA      hdrCbShaderTargetMask;
                regCB_TARGET_MASK  cbTargetMask;
                regCB_SHADER_MASK  cbShaderMask;

                PM4CMDSETDATA       hdrPaClClipCntl;
                regPA_CL_CLIP_CNTL  paClClipCntl;

                PM4CMDSETDATA      hdrPaSuVtxCntl;
                regPA_SU_VTX_CNTL  paSuVtxCntl;

                PM4CMDSETDATA      hdrPaClVteCntl;
                regPA_CL_VTE_CNTL  paClVteCntl;

                PM4CMDSETDATA       hdrPaScLineCntl;
                regPA_SC_LINE_CNTL  paScLineCntl;

                PM4CMDSETDATA            hdrSpiInterpControl0;
                regSPI_INTERP_CONTROL_0  spiInterpControl0;

                PM4CMDSETDATA                   hdrVgtVertexReuseBlockCntl;
                regVGT_VERTEX_REUSE_BLOCK_CNTL  vgtVertexReuseBlockCntl;

                // This packet must be last because not all HW will write it.
                PM4CMDSETDATA         hdrDbShaderControl;
                regDB_SHADER_CONTROL  dbShaderControl;

                // Command space needed, in DWORDs.  This field must always be last in the structure to not interfere
                // w/ the actual commands contained above.
                size_t  spaceNeeded;
            } context;
        } set; // SET path, used for GPU's which are stuck with the legacy microcode.
    };

    Device*const  m_pDevice;
    uint64        m_contextRegHash;

    // We need two copies of IA_MULTI_VGT_PARAM to cover all possible register combinations depending on whether or not
    // WD_SWITCH_ON_EOP is required.
    static constexpr uint32  NumIaMultiVgtParam = 2;
    regIA_MULTI_VGT_PARAM  m_iaMultiVgtParam[NumIaMultiVgtParam];

    regSX_PS_DOWNCONVERT__VI     m_sxPsDownconvert;
    regSX_BLEND_OPT_EPSILON__VI  m_sxBlendOptEpsilon;
    regSX_BLEND_OPT_CONTROL__VI  m_sxBlendOptControl;
    regVGT_LS_HS_CONFIG          m_vgtLsHsConfig;
    regPA_SC_MODE_CNTL_1         m_paScModeCntl1;

    PipelineChunkLsHs  m_chunkLsHs;
    PipelineChunkEsGs  m_chunkEsGs;
    PipelineChunkVsPs  m_chunkVsPs;

    GraphicsPipelineSignature  m_signature;
    Pm4Commands                m_commands;

    // Used to index into the IA_MULTI_VGT_PARAM array based on dynamic state. This just constructs a flat index
    // directly from the integer representations of the bool inputs (1/0).
    static PAL_INLINE uint32 IaRegIdx(bool forceSwitchOnEop) { return static_cast<uint32>(forceSwitchOnEop); }

    // Returns the target mask of the specified CB target.
    uint8 GetTargetMask(uint32 target) const
    {
        PAL_ASSERT(target < MaxColorTargets);
        return ((m_commands.set.context.cbTargetMask.u32All >> (target * 4)) & 0xF);
    }

    PAL_DISALLOW_DEFAULT_CTOR(GraphicsPipeline);
    PAL_DISALLOW_COPY_AND_ASSIGN(GraphicsPipeline);
};

// =====================================================================================================================
// Extension of the PipelineUploader helper class for Gfx9+ graphics pipelines.
class GraphicsPipelineUploader : public Pal::PipelineUploader
{
public:
    explicit GraphicsPipelineUploader(
        Device* pDevice,
        uint32  ctxRegisterCount,
        uint32  shRegisterCount)
        :
        PipelineUploader(pDevice->Parent(), ctxRegisterCount, shRegisterCount)
    { }
    virtual ~GraphicsPipelineUploader() { }

    // Add a context register to GPU memory for use with LOAD_CONTEXT_REG_INDEX.
    PAL_INLINE void AddCtxReg(uint16 address, uint32 value)
        { Pal::PipelineUploader::AddCtxRegister(address - CONTEXT_SPACE_START, value); }
    template <typename Register_t>
    PAL_INLINE void AddCtxReg(uint16 address, Register_t reg)
        { Pal::PipelineUploader::AddCtxRegister(address - CONTEXT_SPACE_START, reg.u32All); }

    // Add a SH register to GPU memory for use with LOAD_SH_REG_INDEX.
    PAL_INLINE void AddShReg(uint16 address, uint32 value)
        { Pal::PipelineUploader::AddShRegister(address - PERSISTENT_SPACE_START, value); }
    template <typename Register_t>
    PAL_INLINE void AddShReg(uint16 address, Register_t reg)
        { Pal::PipelineUploader::AddShRegister(address - PERSISTENT_SPACE_START, reg.u32All); }

private:
    PAL_DISALLOW_DEFAULT_CTOR(GraphicsPipelineUploader);
    PAL_DISALLOW_COPY_AND_ASSIGN(GraphicsPipelineUploader);
};

} // Gfx6
} // Pal
