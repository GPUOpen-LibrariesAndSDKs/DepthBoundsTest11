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
// File: Particle.hlsl
//
// HLSL file containing shader function to render front-facing particles.
//--------------------------------------------------------------------------------------
#include "Shader_include.hlsl"


//--------------------------------------------------------------------------------------
// Structures
//--------------------------------------------------------------------------------------
struct VS_PARTICLE_INPUT
{
    float3 WSPos   : POSITION;
    float  fRadius : RADIUS;
    float4 vColor  : COLOR;
};

struct GS_PARTICLE_INPUT
{
    float3 WSPos   : POSITION;
    float  fRadius : RADIUS;
    float4 vColor  : COLOR;
};

struct PS_PARTICLE_INPUT
{
    float4 vParticlePosition      : PARTICLEPOSITION;
    float2 Tex                    : TEXCOORD0;
    nointerpolation float4 vColor : COLOR;
    float4 Pos                    : SV_POSITION;
};

//--------------------------------------------------------------------------------------
// Vertex Shader to GS
//--------------------------------------------------------------------------------------
GS_PARTICLE_INPUT VSPassThrough( VS_PARTICLE_INPUT input )
{
    GS_PARTICLE_INPUT output = (GS_PARTICLE_INPUT)0;
    
    // Pass world space position to GS
    output.WSPos = input.WSPos;
    
    // Pass radius to GS
    output.fRadius = input.fRadius;
    
    // Pass color to GS
    output.vColor = input.vColor;
    
    return output;
}

//--------------------------------------------------------------------------------------
// Geometry Shader to render point sprites
//--------------------------------------------------------------------------------------
[maxvertexcount(4)]
void GSPointSprite(point GS_PARTICLE_INPUT input[1], inout TriangleStream<PS_PARTICLE_INPUT> SpriteStream)
{
    const float3 g_positions[4] =
    {
        float3( -1.0,  1.0, 0.0 ),
        float3(  1.0,  1.0, 0.0 ),
        float3( -1.0, -1.0, 0.0 ),
        float3(  1.0, -1.0, 0.0 ),
    };
    const float2 g_texcoords[4] = 
    { 
        float2( 0.0, 1.0 ), 
        float2( 1.0, 1.0 ),
        float2( 0.0, 0.0 ),
        float2( 1.0, 0.0 ),
    };
    PS_PARTICLE_INPUT output = (PS_PARTICLE_INPUT)0;
    
    // Emit two new triangles
    [unroll]for( int i=0; i<4; ++i )
    {
        float3 position = g_positions[i] * input[0].fRadius;
        position = mul( position, (float3x3)g_mInvView ) + float4(input[0].WSPos.xyz, 1.0).xyz;
        output.Pos = mul( float4( position, 1.0 ), g_mViewProjection );

        // Pass particle position and radius
        output.vParticlePosition = float4(input[0].WSPos.xyz, input[0].fRadius);
        
        // Pass texture coordinates
        output.Tex = g_texcoords[i];
        
        // Pass color
        output.vColor = input[0].vColor;
        
        // Add vertex
        SpriteStream.Append( output );
    }
    SpriteStream.RestartStrip();
}

//--------------------------------------------------------------------------------------
// Pixel Shader to display constant single color
//--------------------------------------------------------------------------------------
float4 PSConstantColor( PS_PARTICLE_INPUT input ) : SV_TARGET
{
    // Sample particle texture
    float4 vColor = g_txDiffuse.Sample( g_samLinear, input.Tex ).wwww;
    
    // Modulate with color
    vColor = vColor * input.vColor;
    
    // Clip fully transparent pixels
    clip( vColor.a - 1.0/255.0 );
    
    // Return color
    return vColor;
}
