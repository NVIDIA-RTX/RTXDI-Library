/*
 * SPDX-FileCopyrightText: Copyright (c) 2020-2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
 * SPDX-License-Identifier: LicenseRef-NvidiaProprietary
 *
 * NVIDIA CORPORATION, its affiliates and licensors retain all intellectual
 * property and proprietary rights in and to this material, related
 * documentation and any modifications thereto. Any use, reproduction,
 * disclosure or distribution of this material and related documentation
 * without an express license agreement from NVIDIA CORPORATION or
 * its affiliates is strictly prohibited.
 */

#include <Rtxdi/DI/ReSTIRDI.h>

#include <cassert>
#include <vector>
#include <memory>
#include <numeric>
#include <math.h>

using namespace rtxdi;

namespace rtxdi
{

RTXDI_DIBufferIndices GetDefaultReSTIRDIBufferIndices()
{
    RTXDI_DIBufferIndices bufferIndices = {};
    bufferIndices.initialSamplingOutputBufferIndex = 0;
    bufferIndices.temporalResamplingInputBufferIndex = 0;
    bufferIndices.temporalResamplingOutputBufferIndex = 0;
    bufferIndices.spatialResamplingInputBufferIndex = 0;
    bufferIndices.spatialResamplingOutputBufferIndex = 0;
    bufferIndices.shadingInputBufferIndex = 0;
    return bufferIndices;
}

RTXDI_DIInitialSamplingParameters GetDefaultReSTIRDIInitialSamplingParams()
{
    RTXDI_DIInitialSamplingParameters params = {};
    params.brdfCutoff = 0.0001f;
    params.brdfRayMinT = 0.001f;
    params.enableInitialVisibility = true;
    params.environmentMapImportanceSampling = 1;
    params.localLightSamplingMode = ReSTIRDI_LocalLightSamplingMode::Uniform;
    params.numBrdfSamples = 1;
    params.numEnvironmentSamples = 1;
    params.numInfiniteLightSamples = 1;
    params.numLocalLightSamples = 8;
    return params;
}

RTXDI_DITemporalResamplingParameters GetDefaultReSTIRDITemporalResamplingParams()
{
    RTXDI_DITemporalResamplingParameters params = {};
    params.maxHistoryLength = 20;
    params.biasCorrectionMode = ReSTIRDI_TemporalBiasCorrectionMode::Basic;
    params.depthThreshold = 0.1f;
    params.normalThreshold = 0.5f;
    params.enableVisibilityShortcut = false;
    params.enablePermutationSampling = true;
    params.uniformRandomNumber = 0;
    return params;
}

RTXDI_BoilingFilterParameters GetDefaultReSTIRDIBoilingFilterParams()
{
    RTXDI_BoilingFilterParameters params = {};
    params.enableBoilingFilter = static_cast<uint32_t>(true);
    params.boilingFilterStrength = 0.2f;
    return params;
}

RTXDI_DISpatialResamplingParameters GetDefaultReSTIRDISpatialResamplingParams()
{
    RTXDI_DISpatialResamplingParameters params = {};
    params.numDisocclusionBoostSamples = 8;
    params.numSamples = 1;
    params.biasCorrectionMode = ReSTIRDI_SpatialBiasCorrectionMode::Basic;
    params.depthThreshold = 0.1f;
    params.normalThreshold = 0.5f;
    params.samplingRadius = 32.0f;
    params.enableMaterialSimilarityTest = true;
    params.discountNaiveSamples = true;
    params.targetHistoryLength = 0;
    return params;
}

RTXDI_DISpatioTemporalResamplingParameters GetDefaultReSTIRDISpatioTemporalResamplingParams()
{
    RTXDI_DISpatioTemporalResamplingParameters params;
    params.depthThreshold = 0.1f;
    params.normalThreshold = 0.5f;
    params.biasCorrectionMode = ReSTIRDI_SpatioTemporalBiasCorrectionMode::Basic;
    params.maxHistoryLength = 20;

    params.enablePermutationSampling = true;
    params.uniformRandomNumber = 0;
    params.enableVisibilityShortcut = false;
    params.numSamples = 1;

    params.numDisocclusionBoostSamples = 8;
    params.samplingRadius = 32.0f;
    params.enableMaterialSimilarityTest = true;
    params.discountNaiveSamples = true;
    return params;
}

RTXDI_ShadingParameters GetDefaultReSTIRDIShadingParams()
{
    RTXDI_ShadingParameters params = {};
    params.enableDenoiserInputPacking = false;
    params.enableFinalVisibility = true;
    params.finalVisibilityMaxAge = 4;
    params.finalVisibilityMaxDistance = 16.f;
    params.reuseFinalVisibility = true;
    return params;
}

void debugCheckParameters(const ReSTIRDIStaticParameters& params)
{
    assert(params.RenderWidth > 0);
    assert(params.RenderHeight > 0);
}

ReSTIRDIContext::ReSTIRDIContext(const ReSTIRDIStaticParameters& params) :
    m_lastFrameOutputReservoir(0),
    m_currentFrameOutputReservoir(0),
    m_staticParams(params),
    m_resamplingMode(ReSTIRDI_ResamplingMode::TemporalAndSpatial),
    m_reservoirBufferParams(CalculateReservoirBufferParameters(params.RenderWidth, params.RenderHeight, params.CheckerboardSamplingMode)),
    m_bufferIndices(GetDefaultReSTIRDIBufferIndices()),
    m_initialSamplingParams(GetDefaultReSTIRDIInitialSamplingParams()),
    m_temporalResamplingParams(GetDefaultReSTIRDITemporalResamplingParams()),
    m_boilingFilterParams(GetDefaultReSTIRDIBoilingFilterParams()),
    m_spatialResamplingParams(GetDefaultReSTIRDISpatialResamplingParams()),
    m_spatioTemporalResamplingParams(GetDefaultReSTIRDISpatioTemporalResamplingParams()),
    m_shadingParams(GetDefaultReSTIRDIShadingParams())
{
    debugCheckParameters(params);
    UpdateCheckerboardField();
    m_runtimeParams.neighborOffsetMask = m_staticParams.NeighborOffsetCount - 1;
    UpdateBufferIndices();
}

ReSTIRDI_ResamplingMode ReSTIRDIContext::GetResamplingMode() const
{
    return m_resamplingMode;
}

RTXDI_RuntimeParameters ReSTIRDIContext::GetRuntimeParams() const
{
    return m_runtimeParams;
}

RTXDI_ReservoirBufferParameters ReSTIRDIContext::GetReservoirBufferParameters() const
{
    return m_reservoirBufferParams;
}

RTXDI_DIBufferIndices ReSTIRDIContext::GetBufferIndices() const
{
    return m_bufferIndices;
}

RTXDI_DIInitialSamplingParameters ReSTIRDIContext::GetInitialSamplingParameters() const
{
    return m_initialSamplingParams;
}

RTXDI_DITemporalResamplingParameters ReSTIRDIContext::GetTemporalResamplingParameters() const
{
    return m_temporalResamplingParams;
}

RTXDI_BoilingFilterParameters ReSTIRDIContext::GetBoilingFilterParameters() const
{
    return m_boilingFilterParams;
}

RTXDI_DISpatialResamplingParameters ReSTIRDIContext::GetSpatialResamplingParameters() const
{
    return m_spatialResamplingParams;
}

RTXDI_DISpatioTemporalResamplingParameters ReSTIRDIContext::GetSpatioTemporalResamplingParameters() const
{
    return m_spatioTemporalResamplingParams;
}

RTXDI_ShadingParameters ReSTIRDIContext::GetShadingParameters() const
{
    return m_shadingParams;
}

const ReSTIRDIStaticParameters& ReSTIRDIContext::GetStaticParameters() const
{
    return m_staticParams;
}

void ReSTIRDIContext::SetFrameIndex(uint32_t frameIndex)
{
    m_runtimeParams.frameIndex = frameIndex;
    m_temporalResamplingParams.uniformRandomNumber = JenkinsHash(m_runtimeParams.frameIndex);
    m_lastFrameOutputReservoir = m_currentFrameOutputReservoir;
    UpdateBufferIndices();
    UpdateCheckerboardField();
}

uint32_t ReSTIRDIContext::GetFrameIndex() const
{
    return m_runtimeParams.frameIndex;
}

void ReSTIRDIContext::SetResamplingMode(ReSTIRDI_ResamplingMode resamplingMode)
{
    m_resamplingMode = resamplingMode;
    UpdateBufferIndices();
}

void ReSTIRDIContext::SetInitialSamplingParameters(const RTXDI_DIInitialSamplingParameters& initialSamplingParams)
{
    m_initialSamplingParams = initialSamplingParams;
}

void ReSTIRDIContext::SetTemporalResamplingParameters(const RTXDI_DITemporalResamplingParameters& temporalResamplingParams)
{
    m_temporalResamplingParams = temporalResamplingParams;
    m_temporalResamplingParams.uniformRandomNumber = JenkinsHash(m_runtimeParams.frameIndex);
}

void ReSTIRDIContext::SetBoilingFilterParameters(const RTXDI_BoilingFilterParameters& boilingFilterParams)
{
    m_boilingFilterParams = boilingFilterParams;
}

void ReSTIRDIContext::SetSpatialResamplingParameters(const RTXDI_DISpatialResamplingParameters& spatialResamplingParams)
{
    m_spatialResamplingParams = spatialResamplingParams;
}

void ReSTIRDIContext::SetSpatioTemporalResamplingParameters(const RTXDI_DISpatioTemporalResamplingParameters& spatioTemporalResamplingParams)
{
    m_spatioTemporalResamplingParams = spatioTemporalResamplingParams;
    m_spatioTemporalResamplingParams.uniformRandomNumber = JenkinsHash(m_runtimeParams.frameIndex);
}

void ReSTIRDIContext::SetShadingParameters(const RTXDI_ShadingParameters& shadingParams)
{
    m_shadingParams = shadingParams;
}

void ReSTIRDIContext::UpdateBufferIndices()
{
    const bool useTemporalResampling =
        m_resamplingMode == ReSTIRDI_ResamplingMode::Temporal ||
        m_resamplingMode == ReSTIRDI_ResamplingMode::TemporalAndSpatial ||
        m_resamplingMode == ReSTIRDI_ResamplingMode::FusedSpatiotemporal;

    const bool useSpatialResampling =
        m_resamplingMode == ReSTIRDI_ResamplingMode::Spatial ||
        m_resamplingMode == ReSTIRDI_ResamplingMode::TemporalAndSpatial ||
        m_resamplingMode == ReSTIRDI_ResamplingMode::FusedSpatiotemporal;


    if (m_resamplingMode == ReSTIRDI_ResamplingMode::FusedSpatiotemporal)
    {
        m_bufferIndices.initialSamplingOutputBufferIndex = (m_lastFrameOutputReservoir + 1) % c_NumReSTIRDIReservoirBuffers;
        m_bufferIndices.temporalResamplingInputBufferIndex = m_lastFrameOutputReservoir;
        m_bufferIndices.shadingInputBufferIndex = m_bufferIndices.initialSamplingOutputBufferIndex;
    }
    else
    {
        m_bufferIndices.initialSamplingOutputBufferIndex = (m_lastFrameOutputReservoir + 1) % c_NumReSTIRDIReservoirBuffers;
        m_bufferIndices.temporalResamplingInputBufferIndex = m_lastFrameOutputReservoir;
        m_bufferIndices.temporalResamplingOutputBufferIndex = (m_bufferIndices.temporalResamplingInputBufferIndex + 1) % c_NumReSTIRDIReservoirBuffers;
        m_bufferIndices.spatialResamplingInputBufferIndex = useTemporalResampling
            ? m_bufferIndices.temporalResamplingOutputBufferIndex
            : m_bufferIndices.initialSamplingOutputBufferIndex;
        m_bufferIndices.spatialResamplingOutputBufferIndex = (m_bufferIndices.spatialResamplingInputBufferIndex + 1) % c_NumReSTIRDIReservoirBuffers;
        m_bufferIndices.shadingInputBufferIndex = useSpatialResampling
            ? m_bufferIndices.spatialResamplingOutputBufferIndex
            : m_bufferIndices.temporalResamplingOutputBufferIndex;
    }
    m_currentFrameOutputReservoir = m_bufferIndices.shadingInputBufferIndex;
}

void ReSTIRDIContext::UpdateCheckerboardField()
{
    switch (m_staticParams.CheckerboardSamplingMode)
    {
    case CheckerboardMode::Black:
        m_runtimeParams.activeCheckerboardField = (m_runtimeParams.frameIndex & 1u) ? 1u : 2u;
        break;
    case CheckerboardMode::White:
        m_runtimeParams.activeCheckerboardField = (m_runtimeParams.frameIndex & 1u) ? 2u : 1u;
        break;
    case CheckerboardMode::Off:
    default:
        m_runtimeParams.activeCheckerboardField = 0;
    }
}

}
