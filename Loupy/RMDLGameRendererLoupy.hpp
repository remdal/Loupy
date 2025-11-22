//
//  RMDLGameRendererLoupy.hpp
//  Loupy
//
//  Created by Rémy on 22/11/2025.
//

#ifndef RMDLGAMECOORDINATORLOUPY_HPP
#define RMDLGAMECOORDINATORLOUPY_HPP

#include <MetalKit/MetalKit.hpp>
#include <string>
#include <unordered_map>

#include "RMDLMainRenderer_shared.h"
#include "RMDLCamera.hpp"
#include "RMDLUtils.hpp"

#define kMaxBuffersInFlight 3

class GameCoordinatorLoupy
{
public:
    GameCoordinatorLoupy( MTL::Device *pDevice, MTL::PixelFormat layerPixelFormat, NS:: UInteger w, NS:: UInteger h );
    ~GameCoordinatorLoupy();

    void resizeMtkView();

    void moveCamera( simd::float3 translation );
    void rotateCamera(float deltaYaw, float deltaPitch);
    void setCameraAspectRatio(float aspectRatio);

    void buildShaders();
    void buildComputePipeline();
    void buildDepthStencilStates();
    void buildTextures();
    void buildBuffers();
    void draw( MTK::View* _pView );

    void makeArgumentTable();
    void makeResidencySet();
    void compileRenderPipeline( MTL::PixelFormat );

private:
    MTL::PixelFormat                    _pPixelFormat;
    MTL4::CommandQueue*                 _pCommandQueue;
    MTL4::CommandBuffer*                _pCommandBuffer[3];
    MTL4::CommandAllocator*             _pCommandAllocator[kMaxBuffersInFlight];
    MTL4::ArgumentTable*                _pArgumentTable;
    MTL::ResidencySet*                  _pResidencySet;
    MTL::SharedEvent*                   _sharedEvent;
    dispatch_semaphore_t                _semaphore;
    MTL::Buffer*                        _pInstanceDataBuffer[kMaxBuffersInFlight];
    MTL::Buffer*                        _pTriangleDataBuffer[kMaxBuffersInFlight];
    MTL::Buffer*                        _pViewportSizeBuffer;
    MTL::Device*                        _pDevice;
    MTL::RenderPipelineState*           _pPSO;
    MTL::DepthStencilState*             _pDepthStencilState;
    MTL::DepthStencilState*             _pDepthStencilStateJDLV;
    MTL::Texture*                       _pTexture;
    MTL::TextureDescriptor*             _pDepthTextureDesc;
    uint8_t                             _uniformBufferIndex;
    uint64_t                            _currentFrameIndex;
    simd_uint2                          _pViewportSize;
    MTL::Library*                       _pShaderLibrary;
    int                                 _frame;
    NS::SharedPtr<MTL::SharedEvent>     _pPacingEvent;

    MTL::Buffer* _pJDLVStateBuffer[kMaxBuffersInFlight];
    MTL::Buffer* _pGridBuffer_A[kMaxBuffersInFlight];
    MTL::Buffer*            _pGridBuffer_B[kMaxBuffersInFlight];
    MTL::ComputePipelineState*  _pJDLVComputePSO;
    MTL::RenderPipelineState*   _pJDLVRenderPSO;
    MTL4::ArgumentTable*                _pArgumentTableJDLV;
    bool _useBufferAAsSource;
        
    void initGrid();
    void buildJDLVPipelines();
};

#endif /* RMDLGAMECOORDINATORLOUPY_HPP */
