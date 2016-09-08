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
// File: DepthBoundsTest11.cpp
//
// Sample showing how to use driver extensions, with the Depth Bounds Test as an example. 
//--------------------------------------------------------------------------------------

// DXUT now sits one directory up
#include "..\\..\\DXUT\\Core\\DXUT.h"
#include "..\\..\\DXUT\\Core\\DXUTmisc.h"
#include "..\\..\\DXUT\\Core\\DDSTextureLoader.h"
#include "..\\..\\DXUT\\Optional\\DXUTgui.h"
#include "..\\..\\DXUT\\Optional\\DXUTCamera.h"
#include "..\\..\\DXUT\\Optional\\DXUTSettingsDlg.h"
#include "..\\..\\DXUT\\Optional\\SDKmisc.h"
#include "..\\..\\DXUT\\Optional\\SDKmesh.h"

// AMD SDK also sits one directory up
#include "..\\..\\AMD_SDK\\inc\\AMD_SDK.h"

#include "amd_ags.h"

// Project includes
#include "resource.h"

#pragma comment ( lib, "amd_ags_x64.lib" )

#pragma warning( disable : 4100 ) // disable unreference formal parameter warnings for /W4 builds

using namespace DirectX;

//--------------------------------------------------------------------------------------
// Defines
//--------------------------------------------------------------------------------------
#define DEFAULT_DIFFUSE_TEXTURE_FILENAME            (L"..\\media\\Default_Diffuse.dds")
#define DEFAULT_SPECULAR_TEXTURE_FILENAME           (L"..\\media\\Default_Specular.dds")
#define BACKGROUND_MESH_SCALE                       1.0f
#define FRONT_CLIP_PLANE                            1.0f
#define FAR_CLIP_PLANE                              10000.0f
#define MAX_NUMBER_OF_LIGHTS                        150
#define POINT_LIGHT_MAX_RANGE                       40.0f
#define POINT_LIGHT_MAX_INTENSITY					0.25f
//--------------------------------------------------------------------------------------
// Macros
//--------------------------------------------------------------------------------------
#define FLOAT_POSITIVE_RANDOM(x)    ( ((x)*rand()) / RAND_MAX )
#define FLOAT_RANDOM(x)             ((((2.0f*rand())/RAND_MAX) - 1.0f)*(x))
#define CLIP(x)						( ( (x) > 1 || (x) < 0 ) ? 0 : (x) )
#define MAX(x,y)					( ( (x) > (y) ) ? (x) : (y) )
#define MIN(x,y)					( ( (x) < (y) ) ? (x) : (y) )


// Constant buffers

struct MAIN_CB_STRUCT
{
    // Matrices
    XMMATRIX    g_mView;                          // View matrix
    XMMATRIX    g_mProjection;                    // Projection matrix
    XMMATRIX    g_mViewProjection;                // VP matrix
    XMMATRIX    g_mInvView;                       // Inverse of view matrix
    XMMATRIX    g_mInvViewProjectionViewport;     // Inverse of viewprojectionviewport matrix
    XMMATRIX    g_mWorld;                         // World matrix
   
    // Camera    
    XMFLOAT4      g_vEye;                           // Camera's location
    XMFLOAT4      g_vCameraViewVector;              // Camera's view vector

    // Frustum
    XMFLOAT4      g_vScreenResolution;              // Screen resolution

    // Light
    XMFLOAT4      g_vLightPosition;                 // Light's position in world space, plus light's max radius in .w
    XMFLOAT4      g_vLightDiffuse;                  // Light's diffuse color
    XMFLOAT4      g_vLightAmbient;                  // Light's ambient color

	// visualization
	float			fShowDiscardedPixels;
	float			fPadding[3];
};     

struct PARTICLE_DESCRIPTOR
{
    XMFLOAT3		WSPos;								// World space position
    float			fRadius;							// Particle radius
    XMFLOAT4		vColor;								// Particle color
};

struct QUAD_DESCRIPTOR
{
    XMFLOAT3 NDCPosition;    // NDC position
};

struct LIGHT_DESCRIPTOR
{
    // Point Light properties
    XMVECTOR     vColor;

    // Pre-transformed data
    XMFLOAT3     vWorldSpacePosition;
    float        fRange;

    // Post-transformed data
    XMFLOAT3     vViewSpacePosition;
    XMFLOAT3     vNDCTile2DCoordinatesMin;
    XMFLOAT3     vNDCTile2DCoordinatesMax;
};

struct POINT_LIGHT_STRUCTURE
{
    XMFLOAT4     vColor;                         // Light color
    XMFLOAT4     vWorldSpacePositionAndRange;    // World space position in xyz, range in w
};

struct POINT_LIGHT_ARRAY_CB_STRUCT
{
    POINT_LIGHT_STRUCTURE PointLight[MAX_NUMBER_OF_LIGHTS];
};

//--------------------------------------------------------------------------------------
// AMD helper classes defined here
//--------------------------------------------------------------------------------------
static AMD::ShaderCache     g_ShaderCache; 
static AMD::HUD             g_HUD;
static AMD::Slider*			g_NumPointLightsSlider = 0;	

// Global boolean for HUD rendering
bool                        g_bRenderHUD = true;

//--------------------------------------------------------------------------------------
// Global variables
//--------------------------------------------------------------------------------------
CFirstPersonCamera					g_Camera;					// A model viewing camera
CDXUTDialogResourceManager			g_DialogResourceManager;	// manager for shared resources of dialogs
CD3DSettingsDlg						g_SettingsDlg;				// Device settings dialog
CDXUTTextHelper*					g_pTxtHelper = NULL;

// Viewport-sized resources				
ID3D11Texture2D*					g_pMainDepthStencil = NULL;
ID3D11ShaderResourceView*			g_pMainDepthStencilSRV = NULL;
ID3D11DepthStencilView*				g_pMainDSV = NULL;
ID3D11DepthStencilView*				g_pMainReadOnlyDSV = NULL;
ID3D11Texture2D*					g_pGBuffer[2] = { NULL, NULL };
ID3D11RenderTargetView*				g_pGBufferRTV[2] = { NULL, NULL };
ID3D11ShaderResourceView*			g_pGBufferSRV[2] = { NULL, NULL };

// Shaders
ID3D11VertexShader*                 g_pBuildingPass_StoreVS = NULL;
ID3D11PixelShader*                  g_pBuildingPass_StorePS = NULL;
ID3D11VertexShader*                 g_pShadingPass_FullscreenQuadVS = NULL;
ID3D11PixelShader*                  g_pShadingPass_FullscreenLightPS = NULL;
ID3D11VertexShader*                 g_pShadingPass_PointLightFromTileVS = NULL;
ID3D11PixelShader*                  g_pShadingPass_PointLightFromTilePS = NULL;
ID3D11VertexShader*                 g_pParticleVS = NULL;
ID3D11GeometryShader*               g_pParticleGS = NULL;
ID3D11PixelShader*                  g_pParticlePS = NULL;

// Textures
ID3D11ShaderResourceView*           g_pLightTextureRV = NULL;
ID3D11ShaderResourceView*           g_pDefaultDiffuseTextureRV = NULL;
ID3D11ShaderResourceView*           g_pDefaultSpecularTextureRV = NULL;

// The mesh
CDXUTSDKMesh						g_SceneMesh;
XMFLOAT3							g_vMeshCentre(0.0f, 0.0f, 0.0f);
ID3D11Buffer*                       g_pMainCB = NULL;
ID3D11Buffer*                       g_pMeshCB = NULL;
ID3D11Buffer*                       g_pPointLightArrayCB = NULL;
ID3D11InputLayout*                  g_pMeshLayout = NULL;
ID3D11InputLayout*                  g_pFSQuadVertexLayout = NULL;
ID3D11InputLayout*                  g_pQuadVertexLayout = NULL;
ID3D11InputLayout*                  g_pParticleVertexLayout = NULL;
ID3D11Buffer*                       g_pQuadVB = NULL;
ID3D11Buffer*                       g_pQuadIB = NULL;
UINT                                g_uRandomSeed = 1;
ID3D11Buffer*                       g_pParticleVB = NULL;

// States
ID3D11RasterizerState*              g_pRasterizerStateSolid_BFCOff = NULL;
ID3D11RasterizerState*              g_pRasterizerStateSolid_BFCOn = NULL;
ID3D11SamplerState*                 g_pSamplerStatePoint = NULL;
ID3D11SamplerState*                 g_pSamplerStateLinear = NULL;
ID3D11SamplerState*                 g_pSamplerStateAnisotropic = NULL;
ID3D11BlendState*                   g_pNoBlendBS = NULL;
ID3D11BlendState*                   g_pAdditiveBS = NULL;
ID3D11DepthStencilState*            g_pLessEqualDSS = NULL;
ID3D11DepthStencilState*            g_pGreaterDSS = NULL;
ID3D11DepthStencilState*            g_pLessEqualNoDepthWritesDSS = NULL;
ID3D11DepthStencilState*            g_pAlwaysDSS = NULL;

// Camera and light parameters
XMVECTOR							g_vecEye;
XMVECTOR							g_vecAt;
XMVECTOR							g_LightPosition;
float                               g_fLightMaxRadius = 500.0f;
XMFLOAT4							g_pWorldSpaceFrustumPlaneEquation[6];

// Point Lights
UINT                                g_uNumberOfLights = MAX_NUMBER_OF_LIGHTS/2;
LIGHT_DESCRIPTOR                    g_pLightArray[MAX_NUMBER_OF_LIGHTS];

// Render settings
UINT                                g_uRenderWidth;
UINT                                g_uRenderHeight;
XMMATRIX							g_mView;
XMMATRIX							g_mProjection;
XMMATRIX							g_mWorld;
XMVECTOR							g_vCameraFrom;				// Camera "from" point
XMVECTOR							g_vCameraTo;				// Camera "to" point
bool								g_bDepthBoundsTest = true;
bool								g_bShowLights = true;
bool								g_bShowDiscardedPixels = false;
bool								g_bRenderText = true;


// AGS - AMD's helper library
AGSContext*                         g_pAGSContext = nullptr;
unsigned int                        g_ExtensionsSupported = 0;

//#define MEM_DEBUG 1
// for finding D3D memory leaks
#ifdef MEM_DEBUG
ID3D11Device*						g_pMemDebugDevice;
#endif


//--------------------------------------------------------------------------------------
// UI control IDs
//--------------------------------------------------------------------------------------
enum 
{
	IDC_TOGGLEFULLSCREEN = 1,
	IDC_TOGGLEREF,
	IDC_CHANGEDEVICE,
	IDC_DEPTHBOUNDS,
	IDC_SHOWLIGHTS,
	IDC_SHOWDISCARDEDPIXELS,
	IDC_LIGHTCOUNTSLIDER,
};


//--------------------------------------------------------------------------------------
// Forward declarations 
//--------------------------------------------------------------------------------------
LRESULT CALLBACK MsgProc( HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, bool* pbNoFurtherProcessing,
                          void* pUserContext );
void CALLBACK OnKeyboard( UINT nChar, bool bKeyDown, bool bAltDown, void* pUserContext );
void CALLBACK OnGUIEvent( UINT nEvent, int nControlID, CDXUTControl* pControl, void* pUserContext );
void CALLBACK OnFrameMove( double fTime, float fElapsedTime, void* pUserContext );
bool CALLBACK ModifyDeviceSettings( DXUTDeviceSettings* pDeviceSettings, void* pUserContext );

bool CALLBACK IsD3D11DeviceAcceptable( const CD3D11EnumAdapterInfo *AdapterInfo, UINT Output, const CD3D11EnumDeviceInfo *DeviceInfo,
                                       DXGI_FORMAT BackBufferFormat, bool bWindowed, void* pUserContext );
HRESULT CALLBACK OnD3D11CreateDevice( ID3D11Device* pd3dDevice, const DXGI_SURFACE_DESC* pBackBufferSurfaceDesc,
                                     void* pUserContext );
HRESULT CALLBACK OnD3D11ResizedSwapChain( ID3D11Device* pd3dDevice, IDXGISwapChain* pSwapChain,
                                         const DXGI_SURFACE_DESC* pBackBufferSurfaceDesc, void* pUserContext );
void CALLBACK OnD3D11ReleasingSwapChain( void* pUserContext );
void CALLBACK OnD3D11DestroyDevice( void* pUserContext );
void CALLBACK OnD3D11FrameRender( ID3D11Device* pd3dDevice, ID3D11DeviceContext* pd3dImmediateContext, double fTime,
                                 float fElapsedTime, void* pUserContext );
void InitApp();
void RenderText();
void OpenAMDExtensionInterfaces();
void CloseAMDExtensionInterfaces();
HRESULT AddShadersToCache();
void CreateGBuffers(ID3D11Device* pd3dDevice, const DXGI_SURFACE_DESC* pBackBufferSurfaceDesc );
void DestroyGBuffers();
void BuildGBuffers(ID3D11DeviceContext* pd3dContext);
void ShadingPasses(ID3D11DeviceContext* pd3dContext);
void ProcessRandomLights(XMMATRIX *pViewMatrix, XMMATRIX *pProjectionMatrix);
void PostProcessParticles(ID3D11DeviceContext* pd3dContext);
void SetDepthBoundsFromLightRadius(UINT i, XMVECTOR *pViewVec, XMMATRIX *pViewProjection);
void ExtractPlanesFromFrustum( XMFLOAT4* pPlaneEquation, const XMMATRIX* pMatrix, bool bNormalize=TRUE );
bool LightInFrustum(UINT i);


//--------------------------------------------------------------------------------------
// Entry point to the program. Initializes everything and goes into a message processing 
// loop. Idle time is used to render the scene.
//--------------------------------------------------------------------------------------
int WINAPI wWinMain( HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR lpCmdLine, int nCmdShow )
{
    // Enable run-time memory check for debug builds.
#if defined(DEBUG) || defined(_DEBUG)
    _CrtSetDbgFlag( _CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF );
#endif

    // DXUT will create and use the best device (either D3D9 or D3D11) 
    // that is available on the system depending on which D3D callbacks are set below

    // Set DXUT callbacks
    DXUTSetCallbackMsgProc( MsgProc );
    DXUTSetCallbackKeyboard( OnKeyboard );
    DXUTSetCallbackFrameMove( OnFrameMove );
    DXUTSetCallbackDeviceChanging( ModifyDeviceSettings );

    DXUTSetCallbackD3D11DeviceAcceptable( IsD3D11DeviceAcceptable );
    DXUTSetCallbackD3D11DeviceCreated( OnD3D11CreateDevice );
    DXUTSetCallbackD3D11SwapChainResized( OnD3D11ResizedSwapChain );
    DXUTSetCallbackD3D11SwapChainReleasing( OnD3D11ReleasingSwapChain );
    DXUTSetCallbackD3D11DeviceDestroyed( OnD3D11DestroyDevice );
    DXUTSetCallbackD3D11FrameRender( OnD3D11FrameRender );

    agsInit( &g_pAGSContext, nullptr, nullptr );

    InitApp();
    DXUTInit( true, true, NULL ); // Parse the command line, show msgboxes on error, no extra command line params
    DXUTSetCursorSettings( true, true );
    DXUTCreateWindow( L"DepthBoundsTest11 v1.2" );

    // Require D3D_FEATURE_LEVEL_11_0
    DXUTCreateDevice( D3D_FEATURE_LEVEL_11_0, true, 1920, 1080 );

    DXUTMainLoop(); // Enter into the DXUT render loop

	// Ensure the ShaderCache aborts if in a lengthy generation process
	g_ShaderCache.Abort();

    DXUTShutdown();

    agsDeInit( g_pAGSContext );
    g_pAGSContext = nullptr;

    return DXUTGetExitCode();
}


//--------------------------------------------------------------------------------------
// Creates a list of random light positions and ranges
//--------------------------------------------------------------------------------------
void GenerateRandomLights(XMFLOAT3* pReferencePoint, XMFLOAT3* pMaxExtents, float fMaxRange, float fMaxIntensity)
{
    srand(g_uRandomSeed);
    for (UINT i=0; i<MAX_NUMBER_OF_LIGHTS; i++)
    {
        g_pLightArray[i].vWorldSpacePosition.x = FLOAT_RANDOM( pMaxExtents->x ) + pReferencePoint->x;
        g_pLightArray[i].vWorldSpacePosition.y = FLOAT_RANDOM( pMaxExtents->y ) + pReferencePoint->y;
        g_pLightArray[i].vWorldSpacePosition.z = FLOAT_RANDOM( pMaxExtents->z ) + pReferencePoint->z;
        g_pLightArray[i].fRange = FLOAT_POSITIVE_RANDOM( fMaxRange );
		float r = FLOAT_POSITIVE_RANDOM( fMaxIntensity );
		float g = FLOAT_POSITIVE_RANDOM( fMaxIntensity );
		float b = FLOAT_POSITIVE_RANDOM( fMaxIntensity );
		g_pLightArray[i].vColor = XMVectorSet(r, g, b, 1.0f);
    }
}

//--------------------------------------------------------------------------------------
// Initialize the app 
//--------------------------------------------------------------------------------------
void InitApp()
{

    D3DCOLOR DlgColor = 0x88888888; // Semi-transparent background for the dialog

    g_SettingsDlg.Init( &g_DialogResourceManager );
    g_HUD.m_GUI.Init( &g_DialogResourceManager );
    g_HUD.m_GUI.SetBackgroundColors( DlgColor );
    g_HUD.m_GUI.SetCallback( OnGUIEvent );

    // This sample does not support MSAA
    g_SettingsDlg.GetDialogControl()->GetControl( DXUTSETTINGSDLG_D3D11_MULTISAMPLE_COUNT )->SetEnabled( false );
    g_SettingsDlg.GetDialogControl()->GetControl( DXUTSETTINGSDLG_D3D11_MULTISAMPLE_QUALITY )->SetEnabled( false );

    int iY = AMD::HUD::iElementDelta;

    g_HUD.m_GUI.AddButton( IDC_TOGGLEFULLSCREEN, L"Toggle full screen", AMD::HUD::iElementOffset, 
		iY, AMD::HUD::iElementWidth, AMD::HUD::iElementHeight );
    g_HUD.m_GUI.AddButton( IDC_TOGGLEREF, L"Toggle REF (F3)", AMD::HUD::iElementOffset, 
		iY += AMD::HUD::iElementDelta,AMD::HUD::iElementWidth, AMD::HUD::iElementHeight, VK_F3 );
    g_HUD.m_GUI.AddButton( IDC_CHANGEDEVICE, L"Change device (F2)", AMD::HUD::iElementOffset, 
		iY += AMD::HUD::iElementDelta, AMD::HUD::iElementWidth, AMD::HUD::iElementHeight, VK_F2 );
    iY += AMD::HUD::iGroupDelta;

 	g_HUD.m_GUI.AddCheckBox( IDC_DEPTHBOUNDS, L"Enable Depth Bounds Test", AMD::HUD::iElementOffset, 
		iY += AMD::HUD::iElementDelta, AMD::HUD::iElementWidth, AMD::HUD::iElementHeight, g_bDepthBoundsTest);
    iY += AMD::HUD::iElementDelta;

 	g_HUD.m_GUI.AddCheckBox( IDC_SHOWLIGHTS, L"Show Lights", AMD::HUD::iElementOffset, 
		iY += AMD::HUD::iElementDelta, AMD::HUD::iElementWidth, AMD::HUD::iElementHeight, g_bShowLights);
    iY += AMD::HUD::iElementDelta;

 	g_HUD.m_GUI.AddCheckBox( IDC_SHOWDISCARDEDPIXELS, L"Show Shaded Pixels", AMD::HUD::iElementOffset, 
		iY += AMD::HUD::iElementDelta, AMD::HUD::iElementWidth, AMD::HUD::iElementHeight, g_bShowDiscardedPixels);
    iY += AMD::HUD::iElementDelta;

	g_NumPointLightsSlider = new AMD::Slider( g_HUD.m_GUI, IDC_LIGHTCOUNTSLIDER, iY, L"Light Count", 1, MAX_NUMBER_OF_LIGHTS, (int&)g_uNumberOfLights );
}


//--------------------------------------------------------------------------------------
// Render the help and statistics text. This function uses the ID3DXFont interface for 
// efficient text rendering.
//--------------------------------------------------------------------------------------
void RenderText()
{
	if (!g_bRenderText)
		return;

    g_pTxtHelper->Begin();
    g_pTxtHelper->SetInsertionPos( 5, 5 );
    g_pTxtHelper->SetForegroundColor( XMVectorSet( 1.0f, 1.0f, 0.0f, 1.0f ) );
    g_pTxtHelper->DrawTextLine( DXUTGetFrameStats( DXUTIsVsyncEnabled() ) );
    g_pTxtHelper->DrawTextLine( DXUTGetDeviceStats() );

    float fEffectTime = (float)TIMER_GetTime( Gpu, L"Deferred Shading" ) * 1000.0f;
	WCHAR wcbuf[256];
	swprintf_s( wcbuf, 256, L"Deferred shading cost in milliseconds( Total = %.3f )", fEffectTime );
	g_pTxtHelper->DrawTextLine( wcbuf );

    g_pTxtHelper->SetInsertionPos( 5, DXUTGetDXGIBackBufferSurfaceDesc()->Height - AMD::HUD::iElementDelta );
	g_pTxtHelper->DrawTextLine( L"Toggle GUI    : F1" );

    g_pTxtHelper->End();
}


//--------------------------------------------------------------------------------------
// Reject any D3D11 devices that aren't acceptable by returning false
//--------------------------------------------------------------------------------------
bool CALLBACK IsD3D11DeviceAcceptable( const CD3D11EnumAdapterInfo *AdapterInfo, UINT Output, const CD3D11EnumDeviceInfo *DeviceInfo,
                                       DXGI_FORMAT BackBufferFormat, bool bWindowed, void* pUserContext )
{
    return true;
}


//--------------------------------------------------------------------------------------
// Create any D3D11 resources that aren't dependant on the back buffer
//--------------------------------------------------------------------------------------
HRESULT CALLBACK OnD3D11CreateDevice( ID3D11Device* pd3dDevice, const DXGI_SURFACE_DESC* pBackBufferSurfaceDesc,
                                     void* pUserContext )
{
    HRESULT hr;

    ID3D11DeviceContext* pd3dImmediateContext = DXUTGetD3D11DeviceContext();
    V_RETURN( g_DialogResourceManager.OnD3D11CreateDevice( pd3dDevice, pd3dImmediateContext ) );
    V_RETURN( g_SettingsDlg.OnD3D11CreateDevice( pd3dDevice ) );
    g_pTxtHelper = new CDXUTTextHelper( pd3dDevice, pd3dImmediateContext, &g_DialogResourceManager, 15 );

    // Initialize AGS's driver extensions - these are dependent on the D3D11 device
    g_ExtensionsSupported = 0;
    agsDriverExtensionsDX11_Init( g_pAGSContext, pd3dDevice, 7, &g_ExtensionsSupported );

	// Disable the UI if the DBT extension couldn't be initialized
	CDXUTCheckBox *cbPtr = g_HUD.m_GUI.GetCheckBox(IDC_DEPTHBOUNDS);
	if ( g_ExtensionsSupported & AGS_DX11_EXTENSION_DEPTH_BOUNDS_TEST )
	{
		cbPtr->SetEnabled(true);
		if (g_bDepthBoundsTest)
			cbPtr->SetChecked(true);
	}
    else
    {
        cbPtr->SetChecked(false);
		cbPtr->SetEnabled(false);
    }

	g_SceneMesh.Create( pd3dDevice, L"powerplant\\powerplant.sdkmesh", false );

	// Initialize point lights array
	UINT numMeshes = g_SceneMesh.GetNumMeshes();
	XMFLOAT3 LightExtents(-D3D11_FLOAT32_MAX,-D3D11_FLOAT32_MAX,-D3D11_FLOAT32_MAX);
	XMFLOAT3 meshExtents;
	XMFLOAT3 meshCenter;
	XMFLOAT3 center(0,0,0);
	for (UINT i = 0; i < numMeshes; i++)
	{
		XMStoreFloat3(&meshExtents, g_SceneMesh.GetMeshBBoxExtents(i));
		meshExtents.x = meshExtents.x * BACKGROUND_MESH_SCALE;
		meshExtents.y = meshExtents.y * BACKGROUND_MESH_SCALE;
		meshExtents.z = meshExtents.z * BACKGROUND_MESH_SCALE;
		LightExtents.x = MAX(LightExtents.x, meshExtents.x);
		LightExtents.y = MAX(LightExtents.y, meshExtents.y);
		LightExtents.z = MAX(LightExtents.z, meshExtents.z);
		XMStoreFloat3(&meshCenter, g_SceneMesh.GetMeshBBoxCenter(i));
		center.x += meshCenter.x;
		center.x += meshCenter.x;
		center.x += meshCenter.x;
	}
	// average of mesh centers
	center.x /= (float)numMeshes;
	center.y /= (float)numMeshes;
	center.z /= (float)numMeshes;

    GenerateRandomLights(&center, &LightExtents, POINT_LIGHT_MAX_RANGE, POINT_LIGHT_MAX_INTENSITY);
	

	// Create main constant buffer
    D3D11_BUFFER_DESC bd;
    bd.Usage = D3D11_USAGE_DYNAMIC;
    bd.ByteWidth = sizeof( MAIN_CB_STRUCT );
    bd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    bd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    bd.MiscFlags = 0;
    hr = pd3dDevice->CreateBuffer( &bd, NULL, &g_pMainCB );
    if( FAILED( hr ) )
    {
        OutputDebugString(L"Failed to create constant buffer.\n");
        return hr;
    }

	
    // Cretae point light array constant buffer
    bd.Usage = D3D11_USAGE_IMMUTABLE;
    bd.ByteWidth = sizeof( POINT_LIGHT_ARRAY_CB_STRUCT );
	bd.CPUAccessFlags = 0;
	D3D11_SUBRESOURCE_DATA lightData;
	lightData.pSysMem = new(POINT_LIGHT_ARRAY_CB_STRUCT);

	for (UINT i = 0; i < MAX_NUMBER_OF_LIGHTS; i++)
	{
		POINT_LIGHT_ARRAY_CB_STRUCT *pLightData = (POINT_LIGHT_ARRAY_CB_STRUCT *)lightData.pSysMem;
        pLightData->PointLight[i].vWorldSpacePositionAndRange.x = g_pLightArray[i].vWorldSpacePosition.x;
        pLightData->PointLight[i].vWorldSpacePositionAndRange.y = g_pLightArray[i].vWorldSpacePosition.y;
        pLightData->PointLight[i].vWorldSpacePositionAndRange.z = g_pLightArray[i].vWorldSpacePosition.z;
        pLightData->PointLight[i].vWorldSpacePositionAndRange.w = g_pLightArray[i].fRange;

        pLightData->PointLight[i].vColor.x  = XMVectorGetX(g_pLightArray[i].vColor);
        pLightData->PointLight[i].vColor.y  = XMVectorGetY(g_pLightArray[i].vColor);
        pLightData->PointLight[i].vColor.z  = XMVectorGetZ(g_pLightArray[i].vColor);
        pLightData->PointLight[i].vColor.w  = XMVectorGetW(g_pLightArray[i].vColor);
	}

    hr = pd3dDevice->CreateBuffer( &bd, &lightData, &g_pPointLightArrayCB );
	delete lightData.pSysMem;
    if( FAILED( hr ) )
    {
        OutputDebugString(L"Failed to create point light array constant buffer.\n");
        return hr;
    }

    // Create particle VB
    bd.Usage = D3D11_USAGE_DYNAMIC;
    bd.ByteWidth = MAX_NUMBER_OF_LIGHTS * sizeof( PARTICLE_DESCRIPTOR );
    bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    bd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    bd.MiscFlags = 0;
    hr = pd3dDevice->CreateBuffer( &bd, NULL, &g_pParticleVB );
    if( FAILED( hr ) )
    {
        OutputDebugString(L"Failed to create particle vertex buffer.\n");
        return hr;
    }

    // Create quad VB
    bd.Usage = D3D11_USAGE_DYNAMIC;
    bd.ByteWidth = MAX_NUMBER_OF_LIGHTS * 4 * sizeof( QUAD_DESCRIPTOR );
    bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    bd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    bd.MiscFlags = 0;
    hr = pd3dDevice->CreateBuffer( &bd, NULL, &g_pQuadVB );
    if( FAILED( hr ) )
    {
        OutputDebugString(L"Failed to create quad vertex buffer.\n");
        return hr;
    }
    
    // Create quad IB
    D3D11_SUBRESOURCE_DATA SubResourceData;
    SubResourceData.pSysMem = new WORD [ MAX_NUMBER_OF_LIGHTS * 6 ];
    WORD* pWord = (WORD *)SubResourceData.pSysMem;
    for (WORD i=0; i<MAX_NUMBER_OF_LIGHTS; i++)
    {
        pWord[6*i+0] = 4*i;
        pWord[6*i+1] = 4*i + 1;
        pWord[6*i+2] = 4*i + 2;
        
        pWord[6*i+3] = 4*i + 1;
        pWord[6*i+4] = 4*i + 3;
        pWord[6*i+5] = 4*i + 2;
    }
    bd.Usage = D3D11_USAGE_DYNAMIC;
    bd.ByteWidth = MAX_NUMBER_OF_LIGHTS * 6 * sizeof( WORD );
    bd.BindFlags = D3D11_BIND_INDEX_BUFFER;
    bd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    bd.MiscFlags = 0;
    hr = pd3dDevice->CreateBuffer( &bd, &SubResourceData, &g_pQuadIB );
    SAFE_DELETE(SubResourceData.pSysMem);
    if( FAILED( hr ) )
    {
        OutputDebugString(L"Failed to create quad index buffer.\n");
        return hr;
    }


    //
    // Load textures
    //

    // Default white texture for meshes with no diffuse textures
  	V( CreateDDSTextureFromFile( pd3dDevice, DEFAULT_DIFFUSE_TEXTURE_FILENAME, NULL, &g_pDefaultDiffuseTextureRV ) );

    // Default grey texture for meshes with no specular textures
  	V( CreateDDSTextureFromFile( pd3dDevice, DEFAULT_SPECULAR_TEXTURE_FILENAME, NULL, &g_pDefaultSpecularTextureRV ) );

	// Load light texture
    V( CreateDDSTextureFromFile( pd3dDevice, L"..\\media\\Particle.dds", NULL, &g_pLightTextureRV ) );

    //
    // Create state objects
    //

    // Create solid and wireframe rasterizer state objects
    D3D11_RASTERIZER_DESC RasterDesc;
    ZeroMemory( &RasterDesc, sizeof( D3D11_RASTERIZER_DESC ) );
    RasterDesc.FillMode = D3D11_FILL_SOLID;
    RasterDesc.CullMode = D3D11_CULL_NONE;  // No Culling
    RasterDesc.DepthClipEnable = TRUE;
    V_RETURN( pd3dDevice->CreateRasterizerState( &RasterDesc, &g_pRasterizerStateSolid_BFCOff ) );
    RasterDesc.CullMode = D3D11_CULL_BACK;  // Cull back faces
    V_RETURN( pd3dDevice->CreateRasterizerState( &RasterDesc, &g_pRasterizerStateSolid_BFCOn ) );
    
    // Create sampler state for heightmap and normal map
    D3D11_SAMPLER_DESC SSDesc;
    ZeroMemory( &SSDesc, sizeof( D3D11_SAMPLER_DESC ) );
    SSDesc.Filter =         D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    SSDesc.AddressU =       D3D11_TEXTURE_ADDRESS_WRAP;
    SSDesc.AddressV =       D3D11_TEXTURE_ADDRESS_WRAP;
    SSDesc.AddressW =       D3D11_TEXTURE_ADDRESS_WRAP;
    SSDesc.ComparisonFunc = D3D11_COMPARISON_NEVER;
    SSDesc.MaxAnisotropy =  16;
    SSDesc.MinLOD =         0;
    SSDesc.MaxLOD =         D3D11_FLOAT32_MAX;
    V_RETURN( pd3dDevice->CreateSamplerState( &SSDesc, &g_pSamplerStateLinear) );
    SSDesc.Filter =         D3D11_FILTER_MIN_MAG_MIP_POINT;
    V_RETURN( pd3dDevice->CreateSamplerState( &SSDesc, &g_pSamplerStatePoint) );
    SSDesc.Filter =         D3D11_FILTER_ANISOTROPIC;
    SSDesc.MaxAnisotropy =  4;
    V_RETURN( pd3dDevice->CreateSamplerState( &SSDesc, &g_pSamplerStateAnisotropic) );

    // Create blend states
    D3D11_BLEND_DESC BlendState;
    ZeroMemory(&BlendState, sizeof(D3D11_BLEND_DESC));
    BlendState.IndependentBlendEnable =                 FALSE;
    BlendState.RenderTarget[0].BlendEnable =            FALSE;
    BlendState.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
    BlendState.RenderTarget[1].BlendEnable =            FALSE;
    BlendState.RenderTarget[1].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
    hr = pd3dDevice->CreateBlendState( &BlendState, &g_pNoBlendBS);
    BlendState.RenderTarget[0].BlendEnable =            TRUE;
    BlendState.RenderTarget[0].BlendOp =                D3D11_BLEND_OP_ADD;
    BlendState.RenderTarget[0].SrcBlend =               D3D11_BLEND_ONE;
    BlendState.RenderTarget[0].DestBlend =              D3D11_BLEND_ONE;
    BlendState.RenderTarget[0].RenderTargetWriteMask =  D3D11_COLOR_WRITE_ENABLE_ALL;
    BlendState.RenderTarget[0].BlendOpAlpha =           D3D11_BLEND_OP_ADD;
    BlendState.RenderTarget[0].SrcBlendAlpha =          D3D11_BLEND_ZERO;
    BlendState.RenderTarget[0].DestBlendAlpha =         D3D11_BLEND_ZERO;
    hr = pd3dDevice->CreateBlendState(&BlendState, &g_pAdditiveBS);
    
    // Create depthstencil states
    D3D11_DEPTH_STENCIL_DESC DSDesc;
    DSDesc.DepthEnable =        TRUE;
    DSDesc.DepthFunc =          D3D11_COMPARISON_LESS_EQUAL;
    DSDesc.DepthWriteMask =     D3D11_DEPTH_WRITE_MASK_ALL;
    DSDesc.StencilEnable =      FALSE;
    hr = pd3dDevice->CreateDepthStencilState( &DSDesc, &g_pLessEqualDSS );
    DSDesc.DepthWriteMask =     D3D11_DEPTH_WRITE_MASK_ZERO;
    hr = pd3dDevice->CreateDepthStencilState( &DSDesc, &g_pLessEqualNoDepthWritesDSS );
    DSDesc.DepthFunc =          D3D11_COMPARISON_ALWAYS;
    hr = pd3dDevice->CreateDepthStencilState( &DSDesc, &g_pAlwaysDSS );
    DSDesc.DepthFunc =          D3D11_COMPARISON_GREATER;
    hr = pd3dDevice->CreateDepthStencilState( &DSDesc, &g_pGreaterDSS );

    
	static bool bFirstPass = true;

    // One-time setup
    if( bFirstPass )
    {
		// Setup the camera's view parameters
		g_vecEye = XMVectorSet(100.0f, 5.0f, 0.0f, 1.0f);	
		g_LightPosition = XMVectorSet(100.0f, 30.0f, -50.0f, 1.0f);
		g_vecAt = XMVectorSet(0.0f, 0.0f,0.0f, 0.0f);
		g_Camera.SetRotateButtons( true, false, false );
		g_Camera.SetEnablePositionMovement( true );
		g_Camera.SetViewParams( g_vecEye, g_vecAt );
		g_Camera.SetScalers( 0.0025f, 100.0f );

		// Add the applications shaders to the cache
		AddShadersToCache();
        g_ShaderCache.GenerateShaders( AMD::ShaderCache::CREATE_TYPE_COMPILE_CHANGES );    // Only compile shaders that have changed (development mode)
        bFirstPass = false;
	}

    // Create AMD_SDK resources here
    g_HUD.OnCreateDevice( pd3dDevice );
    TIMER_Init( pd3dDevice )

#ifdef MEM_DEBUG
	g_pMemDebugDevice = pd3dDevice;
#endif
    return S_OK;
}


//--------------------------------------------------------------------------------------
// Create any D3D11 resources that depend on the back buffer
//--------------------------------------------------------------------------------------
HRESULT CALLBACK OnD3D11ResizedSwapChain( ID3D11Device* pd3dDevice, IDXGISwapChain* pSwapChain,
                                         const DXGI_SURFACE_DESC* pBackBufferSurfaceDesc, void* pUserContext )
{
    HRESULT hr;

    V_RETURN( g_DialogResourceManager.OnD3D11ResizedSwapChain( pd3dDevice, pBackBufferSurfaceDesc ) );
    V_RETURN( g_SettingsDlg.OnD3D11ResizedSwapChain( pd3dDevice, pBackBufferSurfaceDesc ) );

    // Setup the camera's projection parameters
    float fAspectRatio = pBackBufferSurfaceDesc->Width / (FLOAT)pBackBufferSurfaceDesc->Height;
    g_Camera.SetProjParams( XM_PI/4, fAspectRatio, FRONT_CLIP_PLANE, FAR_CLIP_PLANE );

    // Set the location and size of the AMD standard HUD
    g_HUD.m_GUI.SetLocation( pBackBufferSurfaceDesc->Width - AMD::HUD::iDialogWidth, 0 );
    g_HUD.m_GUI.SetSize( AMD::HUD::iDialogWidth, pBackBufferSurfaceDesc->Height );
    g_HUD.OnResizedSwapChain( pBackBufferSurfaceDesc );

    // Create G-Buffers
    CreateGBuffers(pd3dDevice, pBackBufferSurfaceDesc);

    // Update global viewport settings
    g_uRenderWidth  = pBackBufferSurfaceDesc->Width;
    g_uRenderHeight = pBackBufferSurfaceDesc->Height;
	
    return S_OK;
}


//--------------------------------------------------------------------------------------
// Create G-Buffers
//--------------------------------------------------------------------------------------
void CreateGBuffers(ID3D11Device* pd3dDevice, const DXGI_SURFACE_DESC* pBackBufferSurfaceDesc )
{
    // Create depth stencil texture
	AMD::CreateDepthStencilSurface( &g_pMainDepthStencil, &g_pMainDepthStencilSRV, &g_pMainDSV, DXGI_FORMAT_D24_UNORM_S8_UINT, DXGI_FORMAT_R24_UNORM_X8_TYPELESS, 
                                   pBackBufferSurfaceDesc->Width, pBackBufferSurfaceDesc->Height, 1 );

	// create a read-only DSV
    D3D11_DEPTH_STENCIL_VIEW_DESC descDSV;
    descDSV.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
    descDSV.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
    descDSV.Texture2D.MipSlice = 0;
    descDSV.Flags = D3D11_DSV_READ_ONLY_DEPTH;
    pd3dDevice->CreateDepthStencilView( (ID3D11Resource*)g_pMainDepthStencil, &descDSV, &g_pMainReadOnlyDSV );

    // Create G-Buffer
	AMD::CreateSurface( &g_pGBuffer[0],  &g_pGBufferSRV[0], &g_pGBufferRTV[0], NULL, DXGI_FORMAT_R8G8B8A8_UNORM, 
						pBackBufferSurfaceDesc->Width, pBackBufferSurfaceDesc->Height, 1 );
	AMD::CreateSurface( &g_pGBuffer[1],  &g_pGBufferSRV[1], &g_pGBufferRTV[1], NULL, DXGI_FORMAT_R8G8B8A8_UNORM, 
						pBackBufferSurfaceDesc->Width, pBackBufferSurfaceDesc->Height, 1 );
}


//--------------------------------------------------------------------------------------
// Destroy G-Buffers
//--------------------------------------------------------------------------------------
void DestroyGBuffers()
{
    // Destroy G-Buffers
	SAFE_RELEASE(g_pGBuffer[0]);
	SAFE_RELEASE(g_pGBufferSRV[0]);
	SAFE_RELEASE(g_pGBufferRTV[0]);
    SAFE_RELEASE(g_pGBuffer[1]);
    SAFE_RELEASE(g_pGBufferSRV[1]);
    SAFE_RELEASE(g_pGBufferRTV[1]);

    // Destroy depth buffer
    SAFE_RELEASE(g_pMainDepthStencil);
    SAFE_RELEASE(g_pMainDepthStencilSRV);
    SAFE_RELEASE(g_pMainDSV);
    SAFE_RELEASE(g_pMainReadOnlyDSV);

}

//--------------------------------------------------------------------------------------
// Render the scene using the D3D11 device
//--------------------------------------------------------------------------------------
void CALLBACK OnD3D11FrameRender( ID3D11Device* pd3dDevice, ID3D11DeviceContext* pd3dImmediateContext, double fTime,
                                 float fElapsedTime, void* pUserContext )
{
    ID3D11SamplerState*         pSS[4];

	// Reset the timer at start of frame
    TIMER_Reset()

    // If the settings dialog is being shown, then render it instead of rendering the app's scene
    if( g_SettingsDlg.IsActive() )
    {
        g_SettingsDlg.OnRender( fElapsedTime );
        return;
    }       
       
    // Get the projection & view matrix from the camera class
    XMMATRIX mModelRotationX, mModelRotationY;
    mModelRotationX = XMMatrixRotationX( -XM_PI / 36 ); 
    mModelRotationY = XMMatrixRotationY(  XM_PI / 4 ); 
    g_mView       = g_Camera.GetViewMatrix();
    g_mProjection = g_Camera.GetProjMatrix();
    g_vCameraFrom = g_Camera.GetEyePt();
    g_vCameraTo   = g_Camera.GetLookAtPt();

    // Set sampler states
    pSS[0] = g_pSamplerStateLinear;
    pSS[1] = g_pSamplerStatePoint;
    pSS[2] = g_pSamplerStateAnisotropic;
    pd3dImmediateContext->VSSetSamplers(0, 3, pSS);
    pd3dImmediateContext->PSSetSamplers(0, 3, pSS);

    // Set states
    pd3dImmediateContext->OMSetBlendState( g_pNoBlendBS, 0, 0xffffffff );
    pd3dImmediateContext->OMSetDepthStencilState( g_pLessEqualDSS, 0 );


    //
    // Bind the constant buffers to the device for all stages
    //
    ID3D11Buffer* pBuffers[3];
    pBuffers[0] = g_pMainCB;
    pBuffers[1] = g_pMeshCB;
    pBuffers[2] = g_pPointLightArrayCB;
    pd3dImmediateContext->VSSetConstantBuffers( 0, 3, pBuffers );
    pd3dImmediateContext->GSSetConstantBuffers( 0, 3, pBuffers );
    pd3dImmediateContext->PSSetConstantBuffers( 0, 3, pBuffers );  

    if( g_ShaderCache.ShadersReady() )
    {
		//
		// G-Buffer building passes
		//
		BuildGBuffers(pd3dImmediateContext);

		TIMER_Begin( 0, L"Deferred Shading" )

		//
		// Shading passes
		//
		ShadingPasses(pd3dImmediateContext);

		TIMER_End() // Deferred Shading

		//
		// Pre-resolve post-process passes
		//
		if (g_bShowLights)
			PostProcessParticles(pd3dImmediateContext);
	}
 

    DXUT_BeginPerfEvent( DXUT_PERFEVENTCOLOR, L"HUD / Stats" );

    if( g_ShaderCache.ShadersReady() )
    {
        // Render the HUD
        if( g_bRenderHUD )
        {
            g_HUD.OnRender( fElapsedTime );
        }

        RenderText();
    }
    else
    {
        // Set render target to the back buffer
        ID3D11RenderTargetView* pRTV[1];
        pRTV[0] = DXUTGetD3D11RenderTargetView();
        pd3dImmediateContext->OMSetRenderTargets(1, pRTV, g_pMainReadOnlyDSV);
        float ClearColor[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
        pd3dImmediateContext->ClearRenderTargetView( pRTV[0], ClearColor );

        // Render shader cache progress if still processing
        g_ShaderCache.RenderProgress( g_pTxtHelper, 15, XMVectorSet( 1.0f, 1.0f, 0.0f, 1.0f ) );
    }
    
    DXUT_EndPerfEvent();

    static DWORD dwTimefirst = GetTickCount();
    if ( GetTickCount() - dwTimefirst > 5000 )
    {    
        OutputDebugString( DXUTGetFrameStats( DXUTIsVsyncEnabled() ) );
        OutputDebugString( L"\n" );
        dwTimefirst = GetTickCount();
    }
}


//--------------------------------------------------------------------------------------
// G-Buffer building
//--------------------------------------------------------------------------------------
void BuildGBuffers(ID3D11DeviceContext* pd3dContext)
{
    D3D11_MAPPED_SUBRESOURCE MappedSubResource;
    XMMATRIX	mWorld;
    XMMATRIX	mScale;
	XMMATRIX	mTranslation;
    XMMATRIX	mTWorld;
    XMVECTOR	vWhite;
    HRESULT		hr;

	vWhite = XMVectorSet( 1.0f, 1.0f, 1.0f, 1.0f );

    //
    // Mesh-independant matrix calculations
    //
    
    // Inverse of view matrix
    XMMATRIX mInvView = XMMatrixInverse( NULL, g_mView); 

    // ViewProjection matrix
    XMMATRIX mViewProjection = g_mView * g_mProjection;    

    // Inverse of ViewProjection matrix
    XMMATRIX mInvViewProjection = XMMatrixInverse(NULL, mViewProjection);

    // Inverse of Viewprojection matrix with viewport mapping
	XMMATRIX mViewport ( 2.0f / g_uRenderWidth, 0.0f,						0.0f, 0.0f,
						   0.0f,				  -2.0f / g_uRenderHeight,  0.0f, 0.0f,
						   0.0f,				  0.0f,						1.0f, 0.0f,
						   -1.0f,				  1.0f,						0.0f, 1.0f  );
    XMMATRIX mInvViewProjectionViewport;
    mInvViewProjectionViewport = mViewport * mInvViewProjection;

    mWorld = XMMatrixIdentity();
	mScale = XMMatrixScaling(BACKGROUND_MESH_SCALE, BACKGROUND_MESH_SCALE, BACKGROUND_MESH_SCALE);
    mWorld *= mScale;
	mTranslation = XMMatrixTranslation(g_vMeshCentre.x, g_vMeshCentre.y, g_vMeshCentre.z);
    mWorld *= mTranslation;
        
    // Transpose matrices
    XMMATRIX mTView;
    XMMATRIX mTProjection;
    XMMATRIX mTViewProjection;
    XMMATRIX mTInvView;
    XMMATRIX mTInvViewProjectionViewport;
	mTView = XMMatrixTranspose(g_mView);
	mTProjection = XMMatrixTranspose(g_mProjection);
	mTViewProjection = XMMatrixTranspose(mViewProjection);
	mTInvView = XMMatrixTranspose(mInvView);
	mTInvViewProjectionViewport = XMMatrixTranspose(mInvViewProjectionViewport);
	mTWorld = XMMatrixTranspose(mWorld);

    // Calculate plane equations of frustum in world space
    ExtractPlanesFromFrustum( g_pWorldSpaceFrustumPlaneEquation, &mViewProjection );

    // Set render targets to GBuffer RTs
    ID3D11RenderTargetView* RTViews[2];
    RTViews[0] = g_pGBufferRTV[0];
    RTViews[1] = g_pGBufferRTV[1];
    pd3dContext->OMSetRenderTargets(2, RTViews, g_pMainDSV);

 	float ClearColor[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
	pd3dContext->ClearRenderTargetView( RTViews[0], ClearColor );
	pd3dContext->ClearRenderTargetView( RTViews[1], ClearColor );

   // Clear depth stencil buffer
    pd3dContext->ClearDepthStencilView( g_pMainDSV, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0 );

    // Set default shader resources
    ID3D11ShaderResourceView* pSRV[4];
    pSRV[0] = g_pDefaultDiffuseTextureRV;
    pSRV[1] = g_pDefaultSpecularTextureRV;
    pd3dContext->VSSetShaderResources( 0, 2, pSRV );
    pd3dContext->PSSetShaderResources( 0, 2, pSRV );

    // Set Depth Stencil state
	pd3dContext->OMSetDepthStencilState(g_pLessEqualDSS, 0);
    
    // Set blend state
    pd3dContext->OMSetBlendState(g_pNoBlendBS, 0, 0xffffffff);

    //
    // Update main constant buffer
    //
    hr = pd3dContext->Map( g_pMainCB, 0, D3D11_MAP_WRITE_DISCARD, 0, &MappedSubResource );
    
    // Matrices
    ((MAIN_CB_STRUCT *)MappedSubResource.pData)->g_mView = mTView;
    ((MAIN_CB_STRUCT *)MappedSubResource.pData)->g_mProjection = mTProjection;
    ((MAIN_CB_STRUCT *)MappedSubResource.pData)->g_mViewProjection = mTViewProjection;
    ((MAIN_CB_STRUCT *)MappedSubResource.pData)->g_mInvView = mTInvView;
    ((MAIN_CB_STRUCT *)MappedSubResource.pData)->g_mInvViewProjectionViewport = mTInvViewProjectionViewport;
    ((MAIN_CB_STRUCT *)MappedSubResource.pData)->g_mWorld = mTWorld;

    // Camera
    ((MAIN_CB_STRUCT *)MappedSubResource.pData)->g_vEye.x = XMVectorGetX(g_vCameraFrom);
    ((MAIN_CB_STRUCT *)MappedSubResource.pData)->g_vEye.y = XMVectorGetY(g_vCameraFrom);
    ((MAIN_CB_STRUCT *)MappedSubResource.pData)->g_vEye.z = XMVectorGetZ(g_vCameraFrom);
    ((MAIN_CB_STRUCT *)MappedSubResource.pData)->g_vEye.w = 0.0f;

    XMVECTOR vViewVector = g_vCameraFrom - g_vCameraTo;
	vViewVector = XMVector3Normalize(vViewVector);
    ((MAIN_CB_STRUCT *)MappedSubResource.pData)->g_vCameraViewVector.x = XMVectorGetX(vViewVector);
    ((MAIN_CB_STRUCT *)MappedSubResource.pData)->g_vCameraViewVector.y = XMVectorGetY(vViewVector);
    ((MAIN_CB_STRUCT *)MappedSubResource.pData)->g_vCameraViewVector.z = XMVectorGetZ(vViewVector);
    ((MAIN_CB_STRUCT *)MappedSubResource.pData)->g_vCameraViewVector.w = 1.0f;

    ((MAIN_CB_STRUCT *)MappedSubResource.pData)->g_vLightPosition.x = XMVectorGetX(g_LightPosition);
    ((MAIN_CB_STRUCT *)MappedSubResource.pData)->g_vLightPosition.y = XMVectorGetY(g_LightPosition);
    ((MAIN_CB_STRUCT *)MappedSubResource.pData)->g_vLightPosition.z = XMVectorGetZ(g_LightPosition);
    ((MAIN_CB_STRUCT *)MappedSubResource.pData)->g_vLightPosition.w = g_fLightMaxRadius;

    ((MAIN_CB_STRUCT *)MappedSubResource.pData)->g_vLightDiffuse.x  = 0.0f;
    ((MAIN_CB_STRUCT *)MappedSubResource.pData)->g_vLightDiffuse.y  = 0.0f;
    ((MAIN_CB_STRUCT *)MappedSubResource.pData)->g_vLightDiffuse.z  = 0.0f;
    ((MAIN_CB_STRUCT *)MappedSubResource.pData)->g_vLightDiffuse.w  = 0.0f;

	((MAIN_CB_STRUCT *)MappedSubResource.pData)->g_vLightAmbient.x = 0.01f; 
	((MAIN_CB_STRUCT *)MappedSubResource.pData)->g_vLightAmbient.y = 0.01f; 
	((MAIN_CB_STRUCT *)MappedSubResource.pData)->g_vLightAmbient.z = 0.01f; 
	((MAIN_CB_STRUCT *)MappedSubResource.pData)->g_vLightAmbient.w = 0.0f; 
	
    ((MAIN_CB_STRUCT *)MappedSubResource.pData)->fShowDiscardedPixels = g_bShowDiscardedPixels;
    
    pd3dContext->Unmap( g_pMainCB, 0 );

    //
    // Render background model
    //
        
    // Set shaders
    pd3dContext->VSSetShader( g_pBuildingPass_StoreVS, NULL, 0 );
    pd3dContext->HSSetShader( NULL, NULL, 0);
    pd3dContext->DSSetShader( NULL, NULL, 0);
    pd3dContext->GSSetShader( NULL, NULL, 0 );
    pd3dContext->PSSetShader( g_pBuildingPass_StorePS, NULL, 0 ); 

    // Set input layout 
    pd3dContext->IASetInputLayout( g_pMeshLayout );

    // Render the scene mesh 
	g_SceneMesh.Render( pd3dContext, 0 );	

}



//--------------------------------------------------------------------------------------
// Shading passes
//--------------------------------------------------------------------------------------
void ShadingPasses(ID3D11DeviceContext* pd3dContext)
{

 	// Set render target to the back buffer
    ID3D11RenderTargetView* pRTV[1];
	pRTV[0] = DXUTGetD3D11RenderTargetView();
    pd3dContext->OMSetRenderTargets(1, pRTV, g_pMainReadOnlyDSV);
	float ClearColor[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
	pd3dContext->ClearRenderTargetView( pRTV[0], ClearColor );

	//
    // Fullscreen light
    //

	// Set up fullscreen triangle rendering
	UINT stride = 0;
	UINT offset = 0;
	ID3D11Buffer* pBuffer[1] = { NULL };
	pd3dContext->IASetVertexBuffers( 0, 1, pBuffer, &stride, &offset );
	pd3dContext->IASetPrimitiveTopology( D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
	pd3dContext->IASetInputLayout( NULL );

	// Set shaders
	pd3dContext->VSSetShader( g_pShadingPass_FullscreenQuadVS, NULL, 0 );
	pd3dContext->HSSetShader( NULL, NULL, 0 );
	pd3dContext->DSSetShader( NULL, NULL, 0 );
	pd3dContext->GSSetShader( NULL, NULL, 0 );
	pd3dContext->PSSetShader( g_pShadingPass_FullscreenLightPS, NULL, 0 ); 

	// Set texture inputs
	ID3D11ShaderResourceView*   pSRV[3];
	pSRV[0] = g_pGBufferSRV[0];
	pSRV[1] = g_pGBufferSRV[1];
	pSRV[2] = g_pMainDepthStencilSRV;
	pd3dContext->PSSetShaderResources(0, 3, pSRV);

	// Set Depth Stencil state
	pd3dContext->OMSetDepthStencilState(g_pLessEqualNoDepthWritesDSS, 0);

	// Set blend state
	pd3dContext->OMSetBlendState(g_pNoBlendBS, 0, 0xffffffff);
    
	// Draw fullscreen quad
	pd3dContext->Draw( 3, 0);

	//
	// Random Point Lights
	//

    // Set shaders
    pd3dContext->VSSetShader( g_pShadingPass_PointLightFromTileVS, NULL, 0 );
    pd3dContext->HSSetShader( NULL, NULL, 0);
    pd3dContext->DSSetShader( NULL, NULL, 0);
    pd3dContext->GSSetShader( NULL, NULL, 0 );
    pd3dContext->PSSetShader( g_pShadingPass_PointLightFromTilePS, NULL, 0 ); 

    // Set primitive topology
    pd3dContext->IASetPrimitiveTopology( D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST );

    // Set texture inputs
    pSRV[0] = g_pGBufferSRV[0];
    pSRV[1] = g_pGBufferSRV[1];
    pSRV[2] = g_pMainDepthStencilSRV;
    pd3dContext->PSSetShaderResources(0, 3, pSRV);

    // Process lights
    ProcessRandomLights( &g_mView, &g_mProjection );

	// Store point light positions into quad VB
    D3D11_MAPPED_SUBRESOURCE MappedSubresource;
    pd3dContext->Map( g_pQuadVB, 0, D3D11_MAP_WRITE_DISCARD, 0, &MappedSubresource );
    for (UINT i=0; i<g_uNumberOfLights; i++)
    {
        ((QUAD_DESCRIPTOR*)MappedSubresource.pData)[4*i+0].NDCPosition = XMFLOAT3(g_pLightArray[i].vNDCTile2DCoordinatesMin.x, 
                                                                                        g_pLightArray[i].vNDCTile2DCoordinatesMin.y, 
                                                                                        g_pLightArray[i].vNDCTile2DCoordinatesMax.z);
        ((QUAD_DESCRIPTOR*)MappedSubresource.pData)[4*i+1].NDCPosition = XMFLOAT3(g_pLightArray[i].vNDCTile2DCoordinatesMin.x, 
                                                                                        g_pLightArray[i].vNDCTile2DCoordinatesMax.y, 
                                                                                        g_pLightArray[i].vNDCTile2DCoordinatesMax.z);
        ((QUAD_DESCRIPTOR*)MappedSubresource.pData)[4*i+2].NDCPosition = XMFLOAT3(g_pLightArray[i].vNDCTile2DCoordinatesMax.x, 
                                                                                        g_pLightArray[i].vNDCTile2DCoordinatesMin.y, 
                                                                                        g_pLightArray[i].vNDCTile2DCoordinatesMax.z);
        ((QUAD_DESCRIPTOR*)MappedSubresource.pData)[4*i+3].NDCPosition = XMFLOAT3(g_pLightArray[i].vNDCTile2DCoordinatesMax.x, 
                                                                                        g_pLightArray[i].vNDCTile2DCoordinatesMax.y, 
                                                                                        g_pLightArray[i].vNDCTile2DCoordinatesMax.z);
    }
    pd3dContext->Unmap( g_pQuadVB, 0 );

    // Set vertex buffer
    stride = sizeof(QUAD_DESCRIPTOR);
    offset = 0;
    pd3dContext->IASetVertexBuffers( 0, 1, &g_pQuadVB, &stride, &offset );

    // Set index buffer
    pd3dContext->IASetIndexBuffer( g_pQuadIB, DXGI_FORMAT_R16_UINT, 0 );

    // Set input layout
    pd3dContext->IASetInputLayout( g_pQuadVertexLayout );
        
    // Additive blending
    pd3dContext->OMSetBlendState( g_pAdditiveBS, 0, 0xffffffff );

    // Solid rendering (not affected by global wireframe toggle)
    pd3dContext->RSSetState( g_pRasterizerStateSolid_BFCOn );

	// Set depth test to greater so that light tiles are only rendered if something is in front of them
	pd3dContext->OMSetDepthStencilState( g_pGreaterDSS, 0 );


    // Draw point lights
	if (!g_bDepthBoundsTest || !(g_ExtensionsSupported & AGS_DX11_EXTENSION_DEPTH_BOUNDS_TEST ))
	{
		pd3dContext->DrawIndexed( 6*g_uNumberOfLights, 0, 0 );
	}
	else
	{
		// Draw the light one tile at a time, using the depth bounds test
		// to avoid drawing unecssary pixels. Since the hardware depth bounds
		// test can only have one range per draw call, we need to draw the
		// lights one at a time instead of in one big batch.
		XMMATRIX mViewProjection = g_mView * g_mProjection;
		XMVECTOR viewVec = XMVectorSubtract(g_vCameraFrom,g_vCameraTo);
		viewVec = XMVector3Normalize(viewVec);

		for (UINT i=0; i<g_uNumberOfLights; i++)
		{
			if (!LightInFrustum(i))
				continue;
			SetDepthBoundsFromLightRadius(i, &viewVec, &mViewProjection);
			pd3dContext->DrawIndexed( 6, i*6, 0 );
		}
		// disable the depth bounds test
		if (g_bDepthBoundsTest)
            agsDriverExtensionsDX11_SetDepthBounds( g_pAGSContext, false, 0.0f, 1.0f );
	}

	pd3dContext->OMSetDepthStencilState( g_pLessEqualDSS, 0 );

    // To avoid debug problems
    pSRV[0] = NULL;
    pSRV[1] = NULL;
    pSRV[2] = NULL;
    pd3dContext->PSSetShaderResources(0, 3, pSRV);
}


//--------------------------------------------------------------------------------------
// LightInFrustum 
//
// Returns true if the light sphere is intersecting the frustum
//
//--------------------------------------------------------------------------------------
bool LightInFrustum(UINT lightIndex)
{
	XMVECTOR center = XMVectorSet(g_pLightArray[lightIndex].vWorldSpacePosition.x, 
						g_pLightArray[lightIndex].vWorldSpacePosition.y,
						g_pLightArray[lightIndex].vWorldSpacePosition.z,
						1.0f);
	float radius = g_pLightArray[lightIndex].fRange;
	float distance;

	for (int i = 0; i < 6; i++)
	{
		XMVECTOR planeEqn = XMVectorSet(g_pWorldSpaceFrustumPlaneEquation[i].x, g_pWorldSpaceFrustumPlaneEquation[i].y,
			g_pWorldSpaceFrustumPlaneEquation[i].z, g_pWorldSpaceFrustumPlaneEquation[i].w);
		XMVECTOR vDistance = XMVector4Dot(center, planeEqn);
		distance = XMVectorGetX(vDistance);
		// see if light is completely behind the frustum plane
		if ( distance < -radius)
			return false;
		// see if the light intersects the plane
		if (fabs(distance) < radius)
			return true;
	}

	// must be inside the frustum
	return true;
}


//--------------------------------------------------------------------------------------
// SetDepthBoundsFromLightRadius 
//
// Calculates the depth bounds in screen space using the world space center and radius
// of the light. It then sets the hardware depth bounds test to this range which causes
// pixels outside the range to be culled.
//
//--------------------------------------------------------------------------------------
void SetDepthBoundsFromLightRadius(UINT i, XMVECTOR *pViewVec, XMMATRIX *pViewProjection)
{
	XMVECTOR center = XMVectorSet(g_pLightArray[i].vWorldSpacePosition.x, g_pLightArray[i].vWorldSpacePosition.y, g_pLightArray[i].vWorldSpacePosition.z, 1.0);
	XMVECTOR pos, radiusVec;
	XMVECTOR tfmNear, tfmFar;

	// scale the view vector by the radius
	radiusVec = *pViewVec * g_pLightArray[i].fRange;

	// calculate the near z
	pos = center + radiusVec;
	pos = XMVectorSet(XMVectorGetX(pos), XMVectorGetY(pos), XMVectorGetZ(pos), 1.0f);

	// project the near coordinate into screen space
	tfmNear = XMVector4Transform(pos, *pViewProjection);
	float nearBound = XMVectorGetZ(tfmNear) / XMVectorGetW(tfmNear);

	// calculate the far z
	pos = center - radiusVec;
	pos = XMVectorSet(XMVectorGetX(pos), XMVectorGetY(pos), XMVectorGetZ(pos), 1.0f);

	// project the far coordinate into screen space
	tfmFar =  XMVector4Transform(pos, *pViewProjection);
	float farBound =  XMVectorGetZ(tfmFar) / XMVectorGetW(tfmFar);

	// clip values outside the frustum
	nearBound = CLIP(nearBound);
	farBound = CLIP(farBound);

	// set the depth bounds based on the near and far z of the light
	agsDriverExtensionsDX11_SetDepthBounds( g_pAGSContext, true, nearBound, farBound );
}

	
//--------------------------------------------------------------------------------------
// Post Process Particle rendering
//--------------------------------------------------------------------------------------
void PostProcessParticles(ID3D11DeviceContext* pd3dContext)
{
	// Set render target to the back buffer
    ID3D11RenderTargetView* pRTV[1];
	pRTV[0] = DXUTGetD3D11RenderTargetView();
    pd3dContext->OMSetRenderTargets(1, pRTV, g_pMainReadOnlyDSV);

    // Draw Point light sources

	// Set shaders
    pd3dContext->VSSetShader( g_pParticleVS, NULL, 0 );
    pd3dContext->HSSetShader( NULL, NULL, 0);
    pd3dContext->DSSetShader( NULL, NULL, 0);
    pd3dContext->GSSetShader( g_pParticleGS, NULL, 0 );
    pd3dContext->PSSetShader( g_pParticlePS, NULL, 0 ); 

    // Set shader resources
    ID3D11ShaderResourceView* pSRV[4];
    pSRV[0] = g_pLightTextureRV;
    pd3dContext->PSSetShaderResources( 0, 1, pSRV );

	// Store point light positions into particle's VB
    D3D11_MAPPED_SUBRESOURCE MappedSubresource;
	pd3dContext->Map( g_pParticleVB, 0, D3D11_MAP_WRITE_DISCARD, 0, &MappedSubresource );
	float increase = 1.0 / POINT_LIGHT_MAX_INTENSITY;
    for (UINT i=0; i<g_uNumberOfLights; i++)
    {
        ((PARTICLE_DESCRIPTOR*)MappedSubresource.pData)[i].WSPos   = g_pLightArray[i].vWorldSpacePosition;
        ((PARTICLE_DESCRIPTOR*)MappedSubresource.pData)[i].fRadius = g_pLightArray[i].fRange / 64.0f;
		XMVECTOR vColor = g_pLightArray[i].vColor * increase;
        ((PARTICLE_DESCRIPTOR*)MappedSubresource.pData)[i].vColor.x = XMVectorGetX(vColor);
        ((PARTICLE_DESCRIPTOR*)MappedSubresource.pData)[i].vColor.y = XMVectorGetY(vColor);
        ((PARTICLE_DESCRIPTOR*)MappedSubresource.pData)[i].vColor.z = XMVectorGetZ(vColor);
        ((PARTICLE_DESCRIPTOR*)MappedSubresource.pData)[i].vColor.w = XMVectorGetW(vColor);
    }
    pd3dContext->Unmap( g_pParticleVB, 0 );

    // Set vertex buffer
    UINT stride = sizeof(PARTICLE_DESCRIPTOR);
    UINT offset = 0;
	pd3dContext->IASetVertexBuffers( 0, 1, &g_pParticleVB, &stride, &offset );

    // Set primitive topology
    pd3dContext->IASetPrimitiveTopology( D3D11_PRIMITIVE_TOPOLOGY_POINTLIST );

	// Set input layout
    pd3dContext->IASetInputLayout( g_pParticleVertexLayout );

    // Additive blending
    pd3dContext->OMSetBlendState( g_pAdditiveBS, 0, 0xffffffff );

    // Solid rendering (not affected by global wireframe toggle)
    pd3dContext->RSSetState( g_pRasterizerStateSolid_BFCOn );

    // Draw light
	pd3dContext->Draw( g_uNumberOfLights, 0 );
}

//--------------------------------------------------------------------------------------
// Release D3D11 resources created in OnD3D11ResizedSwapChain 
//--------------------------------------------------------------------------------------
void CALLBACK OnD3D11ReleasingSwapChain( void* pUserContext )
{
    g_DialogResourceManager.OnD3D11ReleasingSwapChain();

    DestroyGBuffers();
}


//--------------------------------------------------------------------------------------
// Release D3D11 resources created in OnD3D11CreateDevice 
//--------------------------------------------------------------------------------------
void CALLBACK OnD3D11DestroyDevice( void* pUserContext )
{
    g_DialogResourceManager.OnD3D11DestroyDevice();
    g_SettingsDlg.OnD3D11DestroyDevice();
    DXUTGetGlobalResourceCache().OnDestroyDevice();

    SAFE_DELETE( g_pTxtHelper );

	SAFE_RELEASE( g_pLightTextureRV );

    SAFE_RELEASE( g_pQuadIB );
    SAFE_RELEASE( g_pQuadVB );
    SAFE_RELEASE( g_pParticleVB );

	SAFE_RELEASE( g_pDefaultSpecularTextureRV );
    SAFE_RELEASE( g_pDefaultDiffuseTextureRV );

    SAFE_RELEASE( g_pFSQuadVertexLayout );
    SAFE_RELEASE( g_pQuadVertexLayout );
    SAFE_RELEASE( g_pParticleVertexLayout );
    SAFE_RELEASE( g_pMeshLayout );

    SAFE_RELEASE( g_pBuildingPass_StorePS );
    SAFE_RELEASE( g_pBuildingPass_StoreVS );
    SAFE_RELEASE( g_pShadingPass_FullscreenLightPS );
    SAFE_RELEASE( g_pShadingPass_FullscreenQuadVS );
    SAFE_RELEASE( g_pShadingPass_PointLightFromTileVS );
    SAFE_RELEASE( g_pShadingPass_PointLightFromTilePS );
    SAFE_RELEASE( g_pParticleVS ); 
    SAFE_RELEASE( g_pParticleGS ); 
    SAFE_RELEASE( g_pParticlePS );


    SAFE_RELEASE( g_pMeshCB );
    SAFE_RELEASE( g_pMainCB );
    SAFE_RELEASE( g_pPointLightArrayCB );

    SAFE_RELEASE( g_pAlwaysDSS );
    SAFE_RELEASE( g_pLessEqualNoDepthWritesDSS );
    SAFE_RELEASE( g_pLessEqualDSS );
    SAFE_RELEASE( g_pGreaterDSS );

    SAFE_RELEASE( g_pAdditiveBS );
    SAFE_RELEASE( g_pNoBlendBS );

    SAFE_RELEASE( g_pRasterizerStateSolid_BFCOn );
    SAFE_RELEASE( g_pRasterizerStateSolid_BFCOff );

    SAFE_RELEASE( g_pSamplerStatePoint );
    SAFE_RELEASE( g_pSamplerStateLinear );
    SAFE_RELEASE( g_pSamplerStateAnisotropic );

	g_SceneMesh.Destroy();

    // Destroy AMD_SDK resources here
	g_ShaderCache.OnDestroyDevice();
	g_HUD.OnDestroyDevice();
    TIMER_Destroy()

    agsDriverExtensionsDX11_DeInit( g_pAGSContext );
	
#ifdef MEM_DEBUG
	ID3D11Debug *pd3dDebug;
	g_pMemDebugDevice->QueryInterface(__uuidof(ID3D11Debug) , (LPVOID *) &pd3dDebug);
	pd3dDebug->ReportLiveDeviceObjects(D3D11_RLDO_DETAIL);
#endif
}


//--------------------------------------------------------------------------------------
// Called right before creating a D3D9 or D3D11 device, allowing the app to modify the device settings as needed
//--------------------------------------------------------------------------------------
bool CALLBACK ModifyDeviceSettings( DXUTDeviceSettings* pDeviceSettings, void* pUserContext )
{
    // For the first device created if its a REF device, optionally display a warning dialog box
    static bool s_bFirstTime = true;
    if( s_bFirstTime )
    {
        s_bFirstTime = false;
        if( pDeviceSettings->d3d11.DriverType == D3D_DRIVER_TYPE_REFERENCE )
        {
            DXUTDisplaySwitchingToREFWarning();
        }
		
		// start with vsync disabled
		pDeviceSettings->d3d11.SyncInterval = 0;
    }

    // Don't auto create a depth buffer, as this sample requires a depth buffer 
    // be created such that it's bindable as a shader resource
    pDeviceSettings->d3d11.AutoCreateDepthStencil = false;

	// This sample does not support MSAA
	pDeviceSettings->d3d11.sd.SampleDesc.Count = 1;

     // Get debug info
#if defined( DEBUG ) || defined( _DEBUG )
	pDeviceSettings->d3d11.CreateFlags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

   return true;
}


//--------------------------------------------------------------------------------------
// Handle updates to the scene.  This is called regardless of which D3D API is used
//--------------------------------------------------------------------------------------
void CALLBACK OnFrameMove( double fTime, float fElapsedTime, void* pUserContext )
{
    // Update the camera's position based on user input 
    g_Camera.FrameMove( fElapsedTime );
}


//--------------------------------------------------------------------------------------
// Handle messages to the application
//--------------------------------------------------------------------------------------
LRESULT CALLBACK MsgProc( HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, bool* pbNoFurtherProcessing,
                          void* pUserContext )
{
    // Pass messages to dialog resource manager calls so GUI state is updated correctly
    *pbNoFurtherProcessing = g_DialogResourceManager.MsgProc( hWnd, uMsg, wParam, lParam );
    if( *pbNoFurtherProcessing )
        return 0;

    // Pass messages to settings dialog if its active
    if( g_SettingsDlg.IsActive() )
    {
        g_SettingsDlg.MsgProc( hWnd, uMsg, wParam, lParam );
        return 0;
    }

    // Give the dialogs a chance to handle the message first
    *pbNoFurtherProcessing = g_HUD.m_GUI.MsgProc( hWnd, uMsg, wParam, lParam );
    if( *pbNoFurtherProcessing )
        return 0;
    
    // Pass all remaining windows messages to camera so it can respond to user input
    g_Camera.HandleMessages( hWnd, uMsg, wParam, lParam );

    return 0;
}


//--------------------------------------------------------------------------------------
// Handle key presses
//--------------------------------------------------------------------------------------
void CALLBACK OnKeyboard( UINT nChar, bool bKeyDown, bool bAltDown, void* pUserContext )
{
    if( bKeyDown )
    {
        switch( nChar )
        {
			case VK_F1:
				g_bRenderHUD = !g_bRenderHUD;
				break;
			case VK_F5:
				g_bRenderText = !g_bRenderText;
				break;
		}
    }
}


//--------------------------------------------------------------------------------------
// Handles the GUI events
//--------------------------------------------------------------------------------------
void CALLBACK OnGUIEvent( UINT nEvent, int nControlID, CDXUTControl* pControl, void* pUserContext )
{
    switch( nControlID )
    {
        case IDC_TOGGLEFULLSCREEN:
            DXUTToggleFullScreen();
            break;
        case IDC_TOGGLEREF:
            DXUTToggleREF();
            break;
        case IDC_CHANGEDEVICE:
            g_SettingsDlg.SetActive( !g_SettingsDlg.IsActive() );
            break;
		case IDC_DEPTHBOUNDS:
			g_bDepthBoundsTest = ((CDXUTCheckBox*)pControl)->GetChecked();
			break;
		case IDC_SHOWLIGHTS:
			g_bShowLights = ((CDXUTCheckBox*)pControl)->GetChecked();
			break;
		case IDC_SHOWDISCARDEDPIXELS:
			g_bShowDiscardedPixels = ((CDXUTCheckBox*)pControl)->GetChecked();
			break;
		case IDC_LIGHTCOUNTSLIDER:
			g_NumPointLightsSlider->OnGuiEvent();
			break;
	}

}


//--------------------------------------------------------------------------------------
// Adds all shaders to the shader cache
//--------------------------------------------------------------------------------------
HRESULT AddShadersToCache()
{
    HRESULT hr = E_FAIL;

     // G-Buffer Layout
    const D3D11_INPUT_ELEMENT_DESC meshvertexlayout[] =
    {
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0,  0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "NORMAL",   0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,    0, 24, D3D11_INPUT_PER_VERTEX_DATA, 0 },
    };

	// G-buffer building shaders
    g_ShaderCache.AddShader( (ID3D11DeviceChild**)&g_pBuildingPass_StoreVS, AMD::ShaderCache::SHADER_TYPE_VERTEX, L"vs_5_0", L"VS_FillGBuffers",
        L"BuildGBuffers.hlsl", 0, NULL, &g_pMeshLayout, meshvertexlayout, ARRAYSIZE( meshvertexlayout ) );

    g_ShaderCache.AddShader( (ID3D11DeviceChild**)&g_pBuildingPass_StorePS, AMD::ShaderCache::SHADER_TYPE_PIXEL, L"ps_5_0", L"PS_FillGBuffers",
        L"BuildGBuffers.hlsl", 0, NULL, NULL, NULL, 0 );


    // Quad input layout
    const D3D11_INPUT_ELEMENT_DESC quadvertexlayout[] =
    {
        { "NDCPOSITION",   0, DXGI_FORMAT_R32G32B32_FLOAT, 0,  0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
    };

	// Shading pass shaders
    g_ShaderCache.AddShader( (ID3D11DeviceChild**)&g_pShadingPass_FullscreenQuadVS, AMD::ShaderCache::SHADER_TYPE_VERTEX, L"vs_5_0", L"VS_FullScreenQuad",
        L"ShadingPasses.hlsl", 0, NULL, &g_pFSQuadVertexLayout, quadvertexlayout, ARRAYSIZE( quadvertexlayout ) );

    g_ShaderCache.AddShader( (ID3D11DeviceChild**)&g_pShadingPass_FullscreenLightPS, AMD::ShaderCache::SHADER_TYPE_PIXEL, L"ps_5_0", L"PS_FullscreenLight",
        L"ShadingPasses.hlsl", 0, NULL, NULL, NULL, 0 );

    g_ShaderCache.AddShader( (ID3D11DeviceChild**)&g_pShadingPass_PointLightFromTileVS, AMD::ShaderCache::SHADER_TYPE_VERTEX, L"vs_5_0", L"VS_PointLightFromTile",
        L"ShadingPasses.hlsl", 0, NULL, &g_pQuadVertexLayout, quadvertexlayout, ARRAYSIZE( quadvertexlayout ) );

    g_ShaderCache.AddShader( (ID3D11DeviceChild**)&g_pShadingPass_PointLightFromTilePS, AMD::ShaderCache::SHADER_TYPE_PIXEL, L"ps_5_0", L"PS_PointLight",
        L"ShadingPasses.hlsl", 0, NULL, NULL, NULL, 0 );

    // Particle input layout
    const D3D11_INPUT_ELEMENT_DESC particlevertexlayout[] =
    {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT,    0,  0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "RADIUS",   0, DXGI_FORMAT_R32_FLOAT,          0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "COLOR",    0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 16, D3D11_INPUT_PER_VERTEX_DATA, 0 },
    };

	// Particle rendering shader
    g_ShaderCache.AddShader( (ID3D11DeviceChild**)&g_pParticleVS, AMD::ShaderCache::SHADER_TYPE_VERTEX, L"vs_5_0", L"VSPassThrough",
        L"Particle.hlsl", 0, NULL, &g_pParticleVertexLayout, particlevertexlayout, ARRAYSIZE( particlevertexlayout ) );

    g_ShaderCache.AddShader( (ID3D11DeviceChild**)&g_pParticleGS, AMD::ShaderCache::SHADER_TYPE_GEOMETRY, L"gs_5_0", L"GSPointSprite",
        L"Particle.hlsl", 0, NULL, NULL, NULL, 0 );

	g_ShaderCache.AddShader( (ID3D11DeviceChild**)&g_pParticlePS, AMD::ShaderCache::SHADER_TYPE_PIXEL, L"ps_5_0", L"PSConstantColor",
        L"Particle.hlsl", 0, NULL, NULL, NULL, 0 );


	return hr;
}


//--------------------------------------------------------------------------------------
// Calculates bounding rectangle in normalized device coordinates for
// the view-space sphere with center "center" and radius "r". "zoom"
// contains the first two diagonal entries of your projection matrix
// (which is assumed to be perspective). You should do a rough rejection
// test of the sphere against the frustum first.
// Taken from: http://blog.gmane.org/gmane.games.devel.algorithms/month=20100101
//--------------------------------------------------------------------------------------
static void CalcSphereBounds(const XMFLOAT3 center, float r, const float zoom[2], 
                             float minb[2], float maxb[2])
{
    // By default, assume that the full screen is covered
    minb[0] = minb[1] = -1.0f;
    maxb[0] = maxb[1] =  1.0f;

    // Once for x, once for y
    for (int i=0; i<2; i++)
    {
		float x;
		if (i == 0)
			x = center.x;
		else
			x = center.y;

        float z = center.z;
        float ds = x*x + z*z;
        float l = ds - r * r;

        if (l > 0.0f)
        {
            float s,c;
            l = sqrt(l);

            s = x * l - z * r;  // ds*sin(alpha)
            c = x * r + z * l;  // ds*cos(alpha)
            if (z*ds > -r*s)    // left/top intersection has positive z
                minb[i] = MAX(-1.0f, s*zoom[i]/c);

            s = z * r + x * l;  // ds*sin(beta)
            c = z * l - x * r;  // ds*cos(beta)
            if (z*ds > r*s)     // right/bottom intersection has positive z
                maxb[i] = MIN(1.0f, s*zoom[i]/c);
        }
    }
}

//--------------------------------------------------------------------------------------
// Transform all point lights to tile coordinates
//--------------------------------------------------------------------------------------
void ProcessRandomLights(XMMATRIX *pViewMatrix, XMMATRIX *pProjectionMatrix)
{
    // Transform point light position from world space to view space
    for (UINT i=0; i<MAX_NUMBER_OF_LIGHTS; i++)
    {
        // Transform point light position from world space to view space
		XMVECTOR pos = XMVectorSet(g_pLightArray[i].vWorldSpacePosition.x, g_pLightArray[i].vWorldSpacePosition.y, 
			g_pLightArray[i].vWorldSpacePosition.z, 1.0f);
        XMVECTOR TransformedVertex = XMVector4Transform(pos, *pViewMatrix);
        g_pLightArray[i].vViewSpacePosition.x = XMVectorGetX(TransformedVertex);
        g_pLightArray[i].vViewSpacePosition.y = XMVectorGetY(TransformedVertex);
        g_pLightArray[i].vViewSpacePosition.z = XMVectorGetZ(TransformedVertex);

        // Calculate 2D screen coordinate extents of sphere
        // This uses a correct method which works even if the camera is close to the sphere
        float zoom[2];
        zoom[0] = XMVectorGetX(pProjectionMatrix->r[0]);
        zoom[1] = XMVectorGetY(pProjectionMatrix->r[1]);
        float minb[2];
        float maxb[2];
        CalcSphereBounds(g_pLightArray[i].vViewSpacePosition, g_pLightArray[i].fRange, zoom, minb, maxb);

        // Write min and max 2D tile coordinates into light array
        g_pLightArray[i].vNDCTile2DCoordinatesMin.x = minb[0];
        g_pLightArray[i].vNDCTile2DCoordinatesMin.y = minb[1];
        
        g_pLightArray[i].vNDCTile2DCoordinatesMax.x = maxb[0];
        g_pLightArray[i].vNDCTile2DCoordinatesMax.y = maxb[1];


        // Transform the point on the sphere closest to the camera to retrieve MinZ
        XMVECTOR vClosest = XMVectorSet(g_pLightArray[i].vViewSpacePosition.x,
                              g_pLightArray[i].vViewSpacePosition.y,
                              g_pLightArray[i].vViewSpacePosition.z - g_pLightArray[i].fRange, 1.0f );
        XMVECTOR vTransformedClosest = XMVector4Transform(vClosest, *pProjectionMatrix);
		vTransformedClosest = vTransformedClosest / XMVectorGetW(vTransformedClosest);

        // Transform the point on the sphere furthest to the camera to retrieve MaxZ
        XMVECTOR vFurthest = XMVectorSet( g_pLightArray[i].vViewSpacePosition.x,
                               g_pLightArray[i].vViewSpacePosition.y,
                               g_pLightArray[i].vViewSpacePosition.z + g_pLightArray[i].fRange, 1.0f );
        XMVECTOR vTransformedFurthest = XMVector4Transform(vFurthest, *pProjectionMatrix);
		vTransformedFurthest = vTransformedFurthest / XMVectorGetW(vTransformedFurthest);

        // Check if sphere intersects the front clip plane
        if ( ( (g_pLightArray[i].vViewSpacePosition.z - g_pLightArray[i].fRange) < FRONT_CLIP_PLANE ) &&
             ( (g_pLightArray[i].vViewSpacePosition.z + g_pLightArray[i].fRange) > FRONT_CLIP_PLANE ) )
        {
            // Closest sphere point is behind clip plane but furthest point is not
            // We therefore need to clamp the closest sphere point
			vTransformedClosest = XMVectorSet(XMVectorGetX(vTransformedClosest), XMVectorGetY(vTransformedClosest),
				XMVectorGetZ(vTransformedClosest), 0.0f);
        }

        // Set min and max in tile coordinates
        g_pLightArray[i].vNDCTile2DCoordinatesMin.z = XMVectorGetZ(vTransformedClosest);
        g_pLightArray[i].vNDCTile2DCoordinatesMax.z = XMVectorGetZ(vTransformedFurthest);
    }
}


//--------------------------------------------------------------------------------------
// EOF.
//--------------------------------------------------------------------------------------
