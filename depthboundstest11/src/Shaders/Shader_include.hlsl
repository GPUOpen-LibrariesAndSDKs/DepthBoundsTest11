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
// File: shader_include.hlsl
//
// Include file for common shader definitions and functions.
//--------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------
// Defines
//--------------------------------------------------------------------------------------                  
#define ADD_SPECULAR 0
                                          
//--------------------------------------------------------------------------------------
// Textures
//--------------------------------------------------------------------------------------
Texture2D g_txDiffuse      : register( t0 );   // Diffuse color texture
Texture2D g_txNormalHeight : register( t1 );   // Normal map and height map texture pair
Texture2D g_txSpecular     : register( t2 );   // Specular texture
Texture2D g_txDensity      : register( t3 );   // Density texture (only used for debug purposes)

Texture2D g_txHiZ          : register( t4 );   // HiZ texture


//--------------------------------------------------------------------------------------
// Samplers
//--------------------------------------------------------------------------------------
SamplerState g_samLinear      : register( s0 );
SamplerState g_samPoint       : register( s1 );
SamplerState g_samAnisotropic : register( s2 );


//--------------------------------------------------------------------------------------
// Constant Buffers
//--------------------------------------------------------------------------------------
cbuffer cbMain : register( b0 )
{
    // Matrices
	matrix g_mView;                             // View matrix
	matrix g_mProjection;                       // Projection matrix
    matrix g_mViewProjection;                   // VP matrix
    matrix g_mInvView;                          // Inverse of view matrix
    matrix g_mInvViewProjectionViewport;        // Inverse of viewprojectionviewport matrix
	matrix g_mWorld;                            // World matrix
    
    // Camera
    float4 g_vEye;					    	    // Camera's location
	float4 g_vCameraViewVector;                 // Camera's view vector
    
    // Frustum
    float4 g_vScreenResolution;                 // Screen resolution
    
    // Light
    float4 g_vLightPosition;                 	// Light's position in world space, plus light's max radius in .w
	float4 g_vLightDiffuse;                  	// Light's diffuse color
	float4 g_vLightAmbient;                  	// Light's ambient color

	// visualization
	float g_ShowDiscardedPixels;				// visualization to show discarded pixels
};
