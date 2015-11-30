// Example low level rendering Unity plugin
#include "RenderingPlugin.h"
#include "Unity/IUnityGraphics.h"

#include <math.h>
#include <stdio.h>
#include <vector>
#include <string>

// --------------------------------------------------------------------------
// Include headers for the graphics APIs we support

#if SUPPORT_D3D11
    #include <d3d11.h>
    #include "Unity/IUnityGraphicsD3D11.h"
#endif

// --------------------------------------------------------------------------
// Helper utilities


// Prints a string
static void DebugLog (const char* str)
{
    #if UNITY_WIN
    OutputDebugStringA (str);
    #else
    printf ("%s", str);
    #endif
}

// COM-like Release macro
#ifndef SAFE_RELEASE
#define SAFE_RELEASE(a) if (a) { a->Release(); a = NULL; }
#endif



// --------------------------------------------------------------------------
// SetTimeFromUnity, an example function we export which is called by one of the scripts.

static float g_Time;

extern "C" void UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API SetTimeFromUnity (float t) { g_Time = t; }


// --------------------------------------------------------------------------
// SetUnityStreamingAssetsPath, an example function we export which is called by one of the scripts.

static std::string s_UnityStreamingAssetsPath;
extern "C" void UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API SetUnityStreamingAssetsPath(const char* path)
{
    s_UnityStreamingAssetsPath = path;
}



// --------------------------------------------------------------------------
// UnitySetInterfaces

static void UNITY_INTERFACE_API OnGraphicsDeviceEvent(UnityGfxDeviceEventType eventType);

static IUnityInterfaces* s_UnityInterfaces = NULL;
static IUnityGraphics* s_Graphics = NULL;
static UnityGfxRenderer s_DeviceType = kUnityGfxRendererNull;

extern "C" void    UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API UnityPluginLoad(IUnityInterfaces* unityInterfaces)
{
    s_UnityInterfaces = unityInterfaces;
    s_Graphics = s_UnityInterfaces->Get<IUnityGraphics>();
    s_Graphics->RegisterDeviceEventCallback(OnGraphicsDeviceEvent);
    
    // Run OnGraphicsDeviceEvent(initialize) manually on plugin load
    OnGraphicsDeviceEvent(kUnityGfxDeviceEventInitialize);
}

extern "C" void UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API UnityPluginUnload()
{
    s_Graphics->UnregisterDeviceEventCallback(OnGraphicsDeviceEvent);
}



// --------------------------------------------------------------------------
// SetTextureFromUnity, an example function we export which is called by one of the scripts.

static void* g_TexturePointer = NULL;

extern "C" void UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API SetTextureFromUnity(void* texturePtr)
{
    // A script calls this at initialization time; just remember the texture pointer here.
    // Will update texture pixels each frame from the plugin rendering event (texture update
    // needs to happen on the rendering thread).
    g_TexturePointer = texturePtr;
}




// --------------------------------------------------------------------------
// GraphicsDeviceEvent

// Actual setup/teardown functions defined below
#if SUPPORT_D3D11
static void DoEventGraphicsDeviceD3D11(UnityGfxDeviceEventType eventType);
#endif

static void UNITY_INTERFACE_API OnGraphicsDeviceEvent(UnityGfxDeviceEventType eventType)
{
    UnityGfxRenderer currentDeviceType = s_DeviceType;

    switch (eventType)
    {
    case kUnityGfxDeviceEventInitialize:
        {
            DebugLog("OnGraphicsDeviceEvent(Initialize).\n");
            s_DeviceType = s_Graphics->GetRenderer();
            currentDeviceType = s_DeviceType;
            break;
        }

    case kUnityGfxDeviceEventShutdown:
        {
            DebugLog("OnGraphicsDeviceEvent(Shutdown).\n");
            s_DeviceType = kUnityGfxRendererNull;
            g_TexturePointer = NULL;
            break;
        }

    case kUnityGfxDeviceEventBeforeReset:
        {
            DebugLog("OnGraphicsDeviceEvent(BeforeReset).\n");
            break;
        }

    case kUnityGfxDeviceEventAfterReset:
        {
            DebugLog("OnGraphicsDeviceEvent(AfterReset).\n");
            break;
        }
    };

    #if SUPPORT_D3D11
    if (currentDeviceType == kUnityGfxRendererD3D11)
        DoEventGraphicsDeviceD3D11(eventType);
    #endif

}



// --------------------------------------------------------------------------
// OnRenderEvent
// This will be called for GL.IssuePluginEvent script calls; eventID will
// be the integer passed to IssuePluginEvent. In this example, we just ignore
// that value.


struct MyVertex {
    float x, y, z;
    unsigned int color;
};
static void SetDefaultGraphicsState ();
static void DoRendering (const float* worldMatrix, const float* identityMatrix, float* projectionMatrix, const MyVertex* verts);

static void UNITY_INTERFACE_API OnRenderEvent(int eventID)
{
    // Unknown graphics device type? Do nothing.
    if (s_DeviceType == kUnityGfxRendererNull)
        return;


    // A colored triangle. Note that colors will come out differently
    // in D3D9/11 and OpenGL, for example, since they expect color bytes
    // in different ordering.
    MyVertex verts[3] = {
        { -0.5f, -0.25f,  0, 0xFFff0000 },
        {  0.5f, -0.25f,  0, 0xFF00ff00 },
        {  0,     0.5f ,  0, 0xFF0000ff },
    };


    // Some transformation matrices: rotate around Z axis for world
    // matrix, identity view matrix, and identity projection matrix.

    float phi = g_Time;
    float cosPhi = cosf(phi);
    float sinPhi = sinf(phi);

    float worldMatrix[16] = {
        cosPhi,-sinPhi,0,0,
        sinPhi,cosPhi,0,0,
        0,0,1,0,
        0,0,0.7f,1,
    };
    float identityMatrix[16] = {
        1,0,0,0,
        0,1,0,0,
        0,0,1,0,
        0,0,0,1,
    };
    float projectionMatrix[16] = {
        1,0,0,0,
        0,1,0,0,
        0,0,1,0,
        0,0,0,1,
    };

    // Actual functions defined below
    SetDefaultGraphicsState ();
    DoRendering (worldMatrix, identityMatrix, projectionMatrix, verts);
}

// --------------------------------------------------------------------------
// GetRenderEventFunc, an example function we export which is used to get a rendering event callback function.
extern "C" UnityRenderingEvent UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API GetRenderEventFunc()
{
    return OnRenderEvent;
}



// -------------------------------------------------------------------
// Shared code

#if SUPPORT_D3D11
typedef std::vector<unsigned char> Buffer;
bool LoadFileIntoBuffer(const std::string& fileName, Buffer& data)
{
    FILE* fp;
    fopen_s(&fp, fileName.c_str(), "rb");
    if (fp)
    {
        fseek (fp, 0, SEEK_END);
        int size = ftell (fp);
        fseek (fp, 0, SEEK_SET);
        data.resize(size);

        fread(&data[0], size, 1, fp);

        fclose(fp);

        return true;
    }
    else
    {
        std::string errorMessage = "Failed to find ";
        errorMessage += fileName;
        DebugLog(errorMessage.c_str());
        return false;
    }
}
#endif


// -------------------------------------------------------------------
//  Direct3D 11 setup/teardown code


#if SUPPORT_D3D11

static ID3D11Device* g_D3D11Device = NULL;
static ID3D11Buffer* g_D3D11VB = NULL; // vertex buffer
static ID3D11Buffer* g_D3D11CB = NULL; // constant buffer
static ID3D11VertexShader* g_D3D11VertexShader = NULL;
static ID3D11PixelShader* g_D3D11PixelShader = NULL;
static ID3D11InputLayout* g_D3D11InputLayout = NULL;
static ID3D11RasterizerState* g_D3D11RasterState = NULL;
static ID3D11BlendState* g_D3D11BlendState = NULL;
static ID3D11DepthStencilState* g_D3D11DepthState = NULL;

static D3D11_INPUT_ELEMENT_DESC s_DX11InputElementDesc[] = {
    { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
    { "COLOR", 0, DXGI_FORMAT_R8G8B8A8_UNORM, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
};

static bool EnsureD3D11ResourcesAreCreated()
{
    if (g_D3D11VertexShader)
        return true;

    // D3D11 has to load resources. Wait for Unity to provide the streaming assets path first.
    if (s_UnityStreamingAssetsPath.empty())
        return false;

    D3D11_BUFFER_DESC desc;
    memset (&desc, 0, sizeof(desc));

    // vertex buffer
    desc.Usage = D3D11_USAGE_DEFAULT;
    desc.ByteWidth = 1024;
    desc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    g_D3D11Device->CreateBuffer (&desc, NULL, &g_D3D11VB);

    // constant buffer
    desc.Usage = D3D11_USAGE_DEFAULT;
    desc.ByteWidth = 64; // hold 1 matrix
    desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    desc.CPUAccessFlags = 0;
    g_D3D11Device->CreateBuffer (&desc, NULL, &g_D3D11CB);


    HRESULT hr = -1;
    Buffer vertexShader;
    Buffer pixelShader;
    std::string vertexShaderPath(s_UnityStreamingAssetsPath);
    vertexShaderPath += "/Shaders/DX11_9_1/SimpleVertexShader.cso";
    std::string fragmentShaderPath(s_UnityStreamingAssetsPath);
    fragmentShaderPath += "/Shaders/DX11_9_1/SimplePixelShader.cso";
    LoadFileIntoBuffer(vertexShaderPath, vertexShader);
    LoadFileIntoBuffer(fragmentShaderPath, pixelShader);

    if (vertexShader.size() > 0 && pixelShader.size() > 0)
    {
        hr = g_D3D11Device->CreateVertexShader(&vertexShader[0], vertexShader.size(), nullptr, &g_D3D11VertexShader);
        if (FAILED(hr)) DebugLog("Failed to create vertex shader.\n");
        hr = g_D3D11Device->CreatePixelShader(&pixelShader[0], pixelShader.size(), nullptr, &g_D3D11PixelShader);
        if (FAILED(hr)) DebugLog("Failed to create pixel shader.\n");
    }
    else
    {
        DebugLog("Failed to load vertex or pixel shader.\n");
    }
    // input layout
    if (g_D3D11VertexShader && vertexShader.size() > 0)
    {
        g_D3D11Device->CreateInputLayout (s_DX11InputElementDesc, 2, &vertexShader[0], vertexShader.size(), &g_D3D11InputLayout);
    }

    // render states
    D3D11_RASTERIZER_DESC rsdesc;
    memset (&rsdesc, 0, sizeof(rsdesc));
    rsdesc.FillMode = D3D11_FILL_SOLID;
    rsdesc.CullMode = D3D11_CULL_NONE;
    rsdesc.DepthClipEnable = TRUE;
    g_D3D11Device->CreateRasterizerState (&rsdesc, &g_D3D11RasterState);

    D3D11_DEPTH_STENCIL_DESC dsdesc;
    memset (&dsdesc, 0, sizeof(dsdesc));
    dsdesc.DepthEnable = TRUE;
    dsdesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
    dsdesc.DepthFunc = D3D11_COMPARISON_LESS_EQUAL;
    g_D3D11Device->CreateDepthStencilState (&dsdesc, &g_D3D11DepthState);

    D3D11_BLEND_DESC bdesc;
    memset (&bdesc, 0, sizeof(bdesc));
    bdesc.RenderTarget[0].BlendEnable = FALSE;
    bdesc.RenderTarget[0].RenderTargetWriteMask = 0xF;
    g_D3D11Device->CreateBlendState (&bdesc, &g_D3D11BlendState);

    return true;
}

static void ReleaseD3D11Resources()
{
    SAFE_RELEASE(g_D3D11VB);
    SAFE_RELEASE(g_D3D11CB);
    SAFE_RELEASE(g_D3D11VertexShader);
    SAFE_RELEASE(g_D3D11PixelShader);
    SAFE_RELEASE(g_D3D11InputLayout);
    SAFE_RELEASE(g_D3D11RasterState);
    SAFE_RELEASE(g_D3D11BlendState);
    SAFE_RELEASE(g_D3D11DepthState);
}

static void DoEventGraphicsDeviceD3D11(UnityGfxDeviceEventType eventType)
{
    if (eventType == kUnityGfxDeviceEventInitialize)
    {
        IUnityGraphicsD3D11* d3d11 = s_UnityInterfaces->Get<IUnityGraphicsD3D11>();
        g_D3D11Device = d3d11->GetDevice();
        
        EnsureD3D11ResourcesAreCreated();
    }
    else if (eventType == kUnityGfxDeviceEventShutdown)
    {
        ReleaseD3D11Resources();
    }
}

#endif // #if SUPPORT_D3D11


// --------------------------------------------------------------------------
// SetDefaultGraphicsState
//
// Helper function to setup some "sane" graphics state. Rendering state
// upon call into our plugin can be almost completely arbitrary depending
// on what was rendered in Unity before.
// Before calling into the plugin, Unity will set shaders to null,
// and will unbind most of "current" objects (e.g. VBOs in OpenGL case).
//
// Here, we set culling off, lighting off, alpha blend & test off, Z
// comparison to less equal, and Z writes off.

static void SetDefaultGraphicsState ()
{
    #if SUPPORT_D3D11
    // D3D11 case
    if (s_DeviceType == kUnityGfxRendererD3D11)
    {
        ID3D11DeviceContext* ctx = NULL;
        g_D3D11Device->GetImmediateContext (&ctx);
        ctx->OMSetDepthStencilState (g_D3D11DepthState, 0);
        ctx->RSSetState (g_D3D11RasterState);
        ctx->OMSetBlendState (g_D3D11BlendState, NULL, 0xFFFFFFFF);
        ctx->Release();
    }
    #endif
}


static void FillTextureFromCode (int width, int height, int stride, unsigned char* dst)
{
    const float t = g_Time * 4.0f;

    for (int y = 0; y < height; ++y)
    {
        unsigned char* ptr = dst;
        for (int x = 0; x < width; ++x)
        {
            // Simple oldskool "plasma effect", a bunch of combined sine waves
            int vv = int(
                (127.0f + (127.0f * sinf(x/7.0f+t))) +
                (127.0f + (127.0f * sinf(y/5.0f-t))) +
                (127.0f + (127.0f * sinf((x+y)/6.0f-t))) +
                (127.0f + (127.0f * sinf(sqrtf(float(x*x + y*y))/4.0f-t)))
                ) / 4;

            // Write the texture pixel
            ptr[0] = vv;
            ptr[1] = vv;
            ptr[2] = vv;
            ptr[3] = vv;

            // To next pixel (our pixels are 4 bpp)
            ptr += 4;
        }

        // To next image row
        dst += stride;
    }
}


static void DoRendering (const float* worldMatrix, const float* identityMatrix, float* projectionMatrix, const MyVertex* verts)
{
    // Does actual rendering of a simple triangle

    #if SUPPORT_D3D11
    // D3D11 case
    if (s_DeviceType == kUnityGfxRendererD3D11 && EnsureD3D11ResourcesAreCreated())
    {
        ID3D11DeviceContext* ctx = NULL;
        g_D3D11Device->GetImmediateContext (&ctx);

        // update constant buffer - just the world matrix in our case
        ctx->UpdateSubresource (g_D3D11CB, 0, NULL, worldMatrix, 64, 0);

        // set shaders
        ctx->VSSetConstantBuffers (0, 1, &g_D3D11CB);
        ctx->VSSetShader (g_D3D11VertexShader, NULL, 0);
        ctx->PSSetShader (g_D3D11PixelShader, NULL, 0);

        // update vertex buffer
        ctx->UpdateSubresource (g_D3D11VB, 0, NULL, verts, sizeof(verts[0])*3, 0);

        // set input assembler data and draw
        ctx->IASetInputLayout (g_D3D11InputLayout);
        ctx->IASetPrimitiveTopology (D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        UINT stride = sizeof(MyVertex);
        UINT offset = 0;
        ctx->IASetVertexBuffers (0, 1, &g_D3D11VB, &stride, &offset);
        ctx->Draw (3, 0);

        // update native texture from code
        if (g_TexturePointer)
        {
            ID3D11Texture2D* d3dtex = (ID3D11Texture2D*)g_TexturePointer;
            D3D11_TEXTURE2D_DESC desc;
            d3dtex->GetDesc (&desc);

            unsigned char* data = new unsigned char[desc.Width*desc.Height*4];
            FillTextureFromCode (desc.Width, desc.Height, desc.Width*4, data);
            ctx->UpdateSubresource (d3dtex, 0, NULL, data, desc.Width*4, 0);
            delete[] data;
        }

        ctx->Release();
    }
    #endif
}
