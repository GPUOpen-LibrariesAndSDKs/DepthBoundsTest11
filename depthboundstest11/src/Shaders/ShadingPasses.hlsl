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
// File: ShadingPasses.hlsl
//--------------------------------------------------------------------------------------
#include "Shader_include.hlsl"

//--------------------------------------------------------------------------------------
// External defines
//--------------------------------------------------------------------------------------

#ifndef MAX_NUMBER_OF_LIGHTS
#define MAX_NUMBER_OF_LIGHTS 150
#endif


//--------------------------------------------------------------------------------------
// Textures
//--------------------------------------------------------------------------------------
Texture2D txGBuffer0    : register(t0);
Texture2D txGBuffer1    : register(t1);
Texture2D txDepthBuffer : register(t2);
                        
                  
//--------------------------------------------------------------------------------------
// Constant Buffers
//--------------------------------------------------------------------------------------
struct POINT_LIGHT_STRUCTURE
{
    float4 vColor;                       // Light color
    float4 vWorldSpacePositionAndRange;  // World space position in xyz, range in w
};

cbuffer cbPointLightArray : register( b2 )
{
    POINT_LIGHT_STRUCTURE   g_Light[MAX_NUMBER_OF_LIGHTS];
};

//--------------------------------------------------------------------------------------
// Structures
//--------------------------------------------------------------------------------------
struct VS_FULLSCREEN_QUAD_OUTPUT
{
    float2 vTexCoord : TEXCOORD0;
    float4 vPosition : SV_POSITION;
};

struct PS_FULLSCREEN_QUAD_INPUT
{
    float2 vTexCoord : TEXCOORD0;
    float4 vPosition : SV_POSITION;
};


struct VS_QUAD_INPUT 
{
    float3 vNDCPosition : NDCPOSITION;
    uint uVertexID      : SV_VERTEXID;
};

struct VS_QUAD_OUTPUT
{
    float4 vLightPositionAndRange : LIGHTPOSITIONANDRANGE;
    float3 vLightColor            : LIGHTCOLOR;
    float4 vPosition              : SV_POSITION;
};

struct PS_QUAD_INPUT
{
    nointerpolation float4 vLightPositionAndRange : LIGHTPOSITIONANDRANGE;
    nointerpolation float3 vLightColor            : LIGHTCOLOR;
    float4 vPosition                              : SV_POSITION;    
};


//--------------------------------------------------------------------------------------
// Function:    VS_FullScreenQuad
//
// Description: Vertex shader that generates a fullscreen quad with texcoords.
//              To use draw 3 vertices with primitive type triangle strip
//--------------------------------------------------------------------------------------
VS_FULLSCREEN_QUAD_OUTPUT VS_FullScreenQuad( uint id : SV_VertexID )
{
    VS_FULLSCREEN_QUAD_OUTPUT Out = (VS_FULLSCREEN_QUAD_OUTPUT)0;

    Out.vTexCoord = float2( (id << 1) & 2, id & 2 );
    Out.vPosition = float4( Out.vTexCoord * float2( 2.0f, -2.0f ) + float2( -1.0f, 1.0f), 0.0f, 1.0f );

    return Out;
}


//--------------------------------------------------------------------------------------
// Function:    PS_FullscreenLight
//
// Description: Apply point light contribution using a fullscreen quad
//--------------------------------------------------------------------------------------
float4 PS_FullscreenLight( PS_FULLSCREEN_QUAD_INPUT i ) : SV_TARGET
{
    float4 vDiffuseColor = float4(0, 0, 0, 0);
    float  fSpecularColor = 0;
    float4 vNormal = float4(0, 0, 0, 0);
    float  fDiffuseIntensity = 0.0;
    float  fSpecularIntensity = 0.0;
    
    // Convert screen coordinates to integer
	int3 nScreenCoordinates = int3(i.vPosition.xy, 0);

    //
    // Fetch G-Buffer data
    //
    // Diffuse color and specular component
    float4(vDiffuseColor.xyz, fSpecularColor) = txGBuffer0.Load( nScreenCoordinates );
    
    // Normal
	vNormal  = txGBuffer1.Load( nScreenCoordinates );
	
	// Depth
	float  fDepthBufferDepth   = txDepthBuffer.Load( nScreenCoordinates ).x;

    
    // Convert normal to signed normal
	vNormal.xyz = vNormal.xyz * 2.0 - 1.0;
	
	// Convert Depth to World-space depth
    float4 vWorldSpacePosition = mul(float4(i.vPosition.x, i.vPosition.y, fDepthBufferDepth, 1.0), g_mInvViewProjectionViewport);
    vWorldSpacePosition.xyz = vWorldSpacePosition.xyz / vWorldSpacePosition.w;
	
	//
	// Apply light equation
	//
	
	// Calculate light vector
    float3 fLightVector = g_vLightPosition.xyz - vWorldSpacePosition.xyz;
    
    // Distance falloff
	float fDistanceFallOff = saturate(1.0 - pow(length(fLightVector)/g_vLightPosition.w, 4));
    
    // Normalize light vector
    fLightVector = normalize(fLightVector);
    
	// Diffuse intensity
	fDiffuseIntensity = saturate(dot(vNormal.xyz, fLightVector));

    // Specular intensity	
	if (fDiffuseIntensity>0)
	{
	    // Calculate view vector
	    float3 vViewVector = normalize(vWorldSpacePosition.xyz - g_vEye.xyz);
	    
	    // Calculate reflection vector
	    float3 vReflectionVector = normalize(reflect(fLightVector, vNormal.xyz));
	}

    // Final equation
    float4 vColor;
    vColor.xyz = ( (fDiffuseIntensity * vDiffuseColor.xyz * g_vLightDiffuse.xyz) * fDistanceFallOff ) + g_vLightAmbient.xyz * vDiffuseColor.xyz;
    vColor.w   = 0;
    
    return vColor;
}




//--------------------------------------------------------------------------------------
// Function:    VS_PointLightFromTile
//
// Description: Vertex shader that performs pass-through for quad rendering
//--------------------------------------------------------------------------------------
VS_QUAD_OUTPUT VS_PointLightFromTile( VS_QUAD_INPUT In )
{
    VS_QUAD_OUTPUT Out = (VS_QUAD_OUTPUT)0;

    // Pass-through
    Out.vPosition = float4(In.vNDCPosition, 1.0);
    
    // Pass light properties to PS
    uint uLightIndex = In.uVertexID / 4;  // Four vertices for one quad i.e. one light
    Out.vLightPositionAndRange = g_Light[uLightIndex].vWorldSpacePositionAndRange;
    Out.vLightColor            = g_Light[uLightIndex].vColor.xyz;
    
    return Out;
}


//--------------------------------------------------------------------------------------
// Function:    PS_PointLight
//
// Description: Apply point light contribution using a tile corresponding to point 
//              light extents.
//--------------------------------------------------------------------------------------
float4 PS_PointLight( PS_QUAD_INPUT i ) : SV_TARGET
{

    float4 vDiffuseColor = float4(0, 0, 0, 0);
    float  fSpecularColor = 0;
    float4 vNormal = float4(0, 0, 0, 0);
    float  fDiffuseIntensity = 0.0;
    float  fSpecularIntensity = 0.0;
	float4 discardColor = float4(0.03, 0.00, 0.03, 0);
    
    // Convert screen coordinates to integer
	int3 nScreenCoordinates = int3(i.vPosition.xy, 0);

    //
    // Fetch G-Buffer data
    //
    // Diffuse color and specular component
    float4(vDiffuseColor.xyz, fSpecularColor) = txGBuffer0.Load( nScreenCoordinates );
    
    // Normal
	vNormal = txGBuffer1.Load( nScreenCoordinates );
	
	// Depth
	float  fDepthBufferDepth = txDepthBuffer.Load( nScreenCoordinates ).x;
    
    // Convert normal to signed normal
	vNormal.xyz = vNormal.xyz * 2.0 - 1.0;
	
	// Convert Depth to World-space depth
    float4 vWorldSpacePosition = mul(float4(i.vPosition.x, i.vPosition.y, fDepthBufferDepth, 1.0), g_mInvViewProjectionViewport);
    vWorldSpacePosition.xyz = vWorldSpacePosition.xyz / vWorldSpacePosition.w;
    
    //
    // Retrieve point light properties
    //
    float3 vLightPosition = i.vLightPositionAndRange.xyz;
    float fLightRange = i.vLightPositionAndRange.w;
    float3 vLightColor = i.vLightColor;    
	
	//
	// Apply light equation
	//
	
	// Calculate light vector
    float3 fLightVector = vLightPosition.xyz - vWorldSpacePosition.xyz;
    
    // Distance falloff
	float fDistanceFallOff = saturate(1.0 - pow(length(fLightVector)/fLightRange, 4));

	if (g_ShowDiscardedPixels)
	{
		// shows pixels discarded by shader
		return discardColor;
	}

    // Normalize light vector
    fLightVector = normalize(fLightVector);
    
	// Diffuse intensity
	fDiffuseIntensity = saturate(dot(vNormal.xyz, fLightVector));
	
    // Specular intensity	
	if (fDiffuseIntensity>0)
	{
	    // Calculate view vector
	    float3 vViewVector = normalize(vWorldSpacePosition.xyz - g_vEye.xyz);
	    
	    // Calculate reflection vector
	    float3 vReflectionVector = normalize(reflect(fLightVector, vNormal.xyz));
	    
	    // Specular intensity
		fSpecularIntensity = saturate(dot(vReflectionVector, vViewVector));
		fSpecularIntensity = pow(fSpecularIntensity, 16);		// Hard-coded for now - should ideally come from material (thus from GBuffer)
	}
    
    // Final equation
    float4 vColor;
    vColor.xyz = (fDiffuseIntensity*vDiffuseColor.xyz + fSpecularIntensity*fSpecularColor) * vLightColor.xyz * fDistanceFallOff;
    vColor.w   = 0;

    return vColor;
}


