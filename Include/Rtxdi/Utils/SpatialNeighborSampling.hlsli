/*
 * SPDX-FileCopyrightText: Copyright (c) 2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
 * SPDX-License-Identifier: LicenseRef-NvidiaProprietary
 *
 * NVIDIA CORPORATION, its affiliates and licensors retain all intellectual
 * property and proprietary rights in and to this material, related
 * documentation and any modifications thereto. Any use, reproduction,
 * disclosure or distribution of this material and related documentation
 * without an express license agreement from NVIDIA CORPORATION or
 * its affiliates is strictly prohibited.
 */

#ifndef RTXDI_SPATIAL_NEIGHBOR_SAMPLING_HLSLI
#define RTXDI_SPATIAL_NEIGHBOR_SAMPLING_HLSLI

#include "Rtxdi/DI/ReSTIRDIParameters.h"

// Stratified sequences start at a random group of four offsets
// Non-stratified sequences start at a random individual offset.
uint RTXDI_InitializeSpatialNeighborSequence(
    float randomValue,
    RTXDI_RuntimeParameters rParams,
    ReSTIRDI_SpatialSamplingMode neighborOffsetSamplingMode)
{
    if (neighborOffsetSamplingMode == RTXDI_DI_NEIGHBOR_OFFSET_SAMPLING_MODE_STRATIFIED_LOW_DISCREPANCY)
    {
        const uint groupCount = (rParams.neighborOffsetMask + 1u) >> 2u;
        return min(uint(randomValue * float(groupCount)), groupCount - 1u);
    }
    else
    {
        return uint(randomValue * float(rParams.neighborOffsetMask));
    }
}

uint RTXDI_CalculateSpatialNeighborOffsetIndex(
    uint neighborSequence,
    uint spatialNeighborIndex,
    uint numSpatialSamples,
    RTXDI_RuntimeParameters rParams,
    ReSTIRDI_SpatialSamplingMode neighborOffsetSamplingMode)
{
    if (neighborOffsetSamplingMode == RTXDI_DI_NEIGHBOR_OFFSET_SAMPLING_MODE_STRATIFIED_LOW_DISCREPANCY)
    {
        // Fixed order for each group of four: 0, 2, 1, 3.
        uint quadrant = ((spatialNeighborIndex & 1u) << 1u) | ((spatialNeighborIndex & 2u) >> 1u);

        // Only the trailing pair of a 4m+2 spatial budget depends on the frame:
        // Sample quadrants 0,2 on even frames and change to 1,3 on odd frames
        if ((numSpatialSamples & 3u) == 2u && spatialNeighborIndex >= (numSpatialSamples & ~3u))
        {
            quadrant |= rParams.frameIndex & 1u;
        }

        const uint groupMask = rParams.neighborOffsetMask >> 2u;
        const uint group = (neighborSequence + (spatialNeighborIndex >> 2u)) & groupMask;
        return (group << 2u) | quadrant;
    }
    else
    {
        return (neighborSequence + spatialNeighborIndex) & rParams.neighborOffsetMask;
    }
}

#endif // RTXDI_SPATIAL_NEIGHBOR_SAMPLING_HLSLI
