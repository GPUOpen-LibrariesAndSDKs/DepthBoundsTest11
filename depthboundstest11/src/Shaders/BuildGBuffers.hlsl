//
// Copyright (c) 2016 Advanced Micro Devices, Inc. All rights reserved.
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.
//

//--------------------------------------------------------------------------------------
// File: BuildingPass.hlsl
//
// HLSL file containing shader functions for rendering data into G-Buffers
//--------------------------------------------------------------------------------------
#include "Shader_include.hlsl"

//--------------------------------------------------------------------------------------
// Structures
//--------------------------------------------------------------------------------------
struct VS_INPUT
{
    float3 inPositionOS   : POSITION;
    float3 vInNormalOS    : NORMAL;
    float2 inTexCoord     : TEXCOORD0;
};


struct PS_INPUT
{
    float2 vTexCoord  : TEXCOORD0;
    float3 vTNormal   : NORMAL;    
    float4 vPosition  : SV_POSITION;
};

struct PS_OUTPUT
{
    float4 vRT0 : SV_TARGET0;  // Diffuse
    float4 vRT1 : SV_TARGET1;  // Normal
};

    
//--------------------------------------------------------------------------------------
// Vertex shader
//--------------------------------------------------------------------------------------
PS_INPUT VS_FillGBuffers( VS_INPUT i )
{
    PS_INPUT Out;

	matrix mWorldViewProjection = mul ( g_mViewProjection, g_mWorld );

    // Compute position in clipping space
    Out.vPosition = mul( float4( i.inPositionOS.xyz, 1.0 ), mWorldViewProjection );
    
    // Transform normals (which are defined locally to the model) into world space
	Out.vTNormal   = mul(i.vInNormalOS,   (float3x3)g_mWorld);
	
    // Normalize them
	Out.vTNormal   = normalize(Out.vTNormal);
    
    // Propagate texture coordinate through
    Out.vTexCoord = i.inTexCoord;
    
    return Out;
}   

//--------------------------------------------------------------------------------------
// Pixel shader: Fill G-Buffers
//--------------------------------------------------------------------------------------
PS_OUTPUT PS_FillGBuffers( PS_INPUT i )
{ 
    PS_OUTPUT Out = (PS_OUTPUT)0;
    
    // Sample diffuse texture	
    float4 diffuse = g_txDiffuse.Sample( g_samAnisotropic, i.vTexCoord );
    
    // Sample specular texture (monochromatic specular used)
    //float specular = CalculateLuminance( g_txSpecular.Sample( g_samAnisotropic, i.vTexCoord ).xyz );
    float specular = g_txSpecular.Sample( g_samAnisotropic, i.vTexCoord ).x;

    // Store diffuse color and specular component
    Out.vRT0 = float4(diffuse.xyz, specular);
    										   
	// Store normal as signed value										   
	Out.vRT1 = float4(i.vTNormal.xyz*0.5 + 0.5, 0);
	
    // Return color
    return Out;
}   





