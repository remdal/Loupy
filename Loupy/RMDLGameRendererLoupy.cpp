//
//  RMDLGameRendererLoupy.cpp
//  Loupy
//
//  Created by Rémy on 22/11/2025.

#define NS_PRIVATE_IMPLEMENTATION
#define CA_PRIVATE_IMPLEMENTATION
#define MTL_PRIVATE_IMPLEMENTATION
#define MTK_PRIVATE_IMPLEMENTATION
#define MTLFX_PRIVATE_IMPLEMENTATION

#include <Foundation/Foundation.hpp>
#include <QuartzCore/QuartzCore.hpp>
#include <Metal/Metal.hpp>
#include <MetalFX/MetalFX.hpp>

#include <simd/simd.h>
#include <utility>
#include <variant>
#include <vector>
#include <cmath>
#include <stdio.h>
#include <iostream>
#include <memory>
#include <thread>
#include <sys/sysctl.h>
#include <stdlib.h>
#include <string.h>

#include "RMDLGameRendererLoupy.hpp"

#define kMaxFramesInFlight 3

GameCoordinatorLoupy::GameCoordinatorLoupy( MTL::Device* pDevice, MTL::PixelFormat layerPixelFormat, NS::UInteger w, NS::UInteger h )
    : _pPixelFormat(layerPixelFormat)
    , _pDevice(pDevice->retain())
    , _frame(0)
    , _pShaderLibrary(nullptr)
{
    _pCommandQueue = _pDevice->newMTL4CommandQueue();
    _pShaderLibrary = _pDevice->newDefaultLibrary();
    
    _sharedEvent = _pDevice->newSharedEvent();
    _sharedEvent->setSignaledValue(_currentFrameIndex);
    
    _semaphore = dispatch_semaphore_create( kMaxFramesInFlight );

    NS::Error* pError = nullptr;
    
    for (uint8_t i = 0; i < kMaxFramesInFlight; i++)
    {
        //        _pTriangleDataBuffer[i] = _pDevice->newBuffer( sizeof(TriangleData), MTL::ResourceStorageModeShared );
        //        std::string name = "_pTriangleDataBuffer[" + std::to_string(i) + "]";
        //        _pTriangleDataBuffer[i]->setLabel( NS::String::string( name.c_str(), NS::ASCIIStringEncoding ) );
        //
        _pCommandAllocator[i] = _pDevice->newCommandAllocator();
        //
        //        _pJDLVStateBuffer[i] = _pDevice->newBuffer( sizeof(JDLVState), MTL::ResourceStorageModeManaged );
        //        _pGridBuffer_A[i] = _pDevice->newBuffer( gridSize, MTL::ResourceStorageModeManaged );
        //        _pGridBuffer_B[i] = _pDevice->newBuffer( gridSize, MTL::ResourceStorageModeManaged );
        //        ft_memset(_pGridBuffer_A[i]->contents(), 0, gridSize);
        //        ft_memset(_pGridBuffer_B[i]->contents(), 0, gridSize);
        //        _pGridBuffer_A[i]->didModifyRange( NS::Range(0, gridSize) );
        //        _pGridBuffer_B[i]->didModifyRange( NS::Range(0, gridSize) );
    }
    //
    //    initGrid();
    //    buildJDLVPipelines();
    //    buildDepthStencilStates();
    //
    const NS::UInteger nativeWidth = (NS::UInteger)(w/1.2);
    const NS::UInteger nativeHeight = (NS::UInteger)(h/1.2);
    _pViewportSize.x = (float)w;
    _pViewportSize.y = (float)h;
    _pViewportSizeBuffer = _pDevice->newBuffer(sizeof(_pViewportSize), MTL::ResourceStorageModeShared);
    ft_memcpy(_pViewportSizeBuffer->contents(), &_pViewportSize, sizeof(_pViewportSize));

    
    _brushSize = 1000.0f;
    {
        static const NS::UInteger shadowWidth = 1024;
        _pTextureDesc = MTL::TextureDescriptor::texture2DDescriptor(MTL::PixelFormatDepth32Float, shadowWidth, shadowWidth, false);
        _pTextureDesc->setTextureType(MTL::TextureType2DArray);
        _pTextureDesc->setArrayLength(3);
        _pTextureDesc->setUsage( MTL::TextureUsageRenderTarget) ;
        _pTextureDesc->setStorageMode( MTL::StorageModePrivate );
        _shadowMap = _pDevice->newTexture(_pTextureDesc);
        _shadowMap->setLabel(MTLSTR("Shadow"));
    }
    MTL::DepthStencilDescriptor* depthStateDesc = MTL::DepthStencilDescriptor::alloc()->init();
    depthStateDesc->setDepthCompareFunction( MTL::CompareFunctionLess );
    depthStateDesc->setDepthWriteEnabled( true );
    
    _shadowPassDesc->depthAttachment()->setTexture(_shadowMap);
    _shadowPassDesc->depthAttachment()->setClearDepth( 1.f );
    _shadowPassDesc->depthAttachment()->setLoadAction( MTL::LoadActionClear );
    _shadowPassDesc->depthAttachment()->setStoreAction( MTL::StoreActionStore );
    _shadowDepthState = _pDevice->newDepthStencilState( depthStateDesc );

    _gBufferPassDesc->depthAttachment()->setClearDepth( 1.0f );
    _gBufferPassDesc->depthAttachment()->setTexture( _shadowMap );
    _gBufferPassDesc->depthAttachment()->setLoadAction( MTL::LoadActionClear );
    _gBufferPassDesc->depthAttachment()->setStoreAction( MTL::StoreActionStore );

    _gBufferPassDesc->colorAttachments()->object(0)->setLoadAction( MTL::LoadActionDontCare );
    _gBufferPassDesc->colorAttachments()->object(0)->setStoreAction( MTL::StoreActionStore );
    _gBufferPassDesc->colorAttachments()->object(1)->setLoadAction( MTL::LoadActionDontCare );
    _gBufferPassDesc->colorAttachments()->object(1)->setStoreAction( MTL::StoreActionStore );

    depthStateDesc->setDepthCompareFunction( MTL::CompareFunctionLess );
    depthStateDesc->setDepthWriteEnabled( true );
    _gBufferDepthState = _pDevice->newDepthStencilState(depthStateDesc);

    _gBufferWithLoadPassDesc = _gBufferPassDesc;
    _gBufferWithLoadPassDesc->depthAttachment()->setLoadAction( MTL::LoadActionLoad );
    _gBufferWithLoadPassDesc->colorAttachments()->object(0)->setLoadAction( MTL::LoadActionLoad );
    _gBufferWithLoadPassDesc->colorAttachments()->object(1)->setLoadAction( MTL::LoadActionLoad );

    _lightingPassDesc->colorAttachments()->object(0)->setLoadAction( MTL::LoadActionDontCare );
    _lightingPassDesc->colorAttachments()->object(0)->setStoreAction( MTL::StoreActionStore );

    depthStateDesc->setDepthCompareFunction( MTL::CompareFunctionAlways );
    depthStateDesc->setDepthWriteEnabled( false );
    _lightingDepthState = _pDevice->newDepthStencilState(depthStateDesc);
    {
        MTL4::RenderPipelineDescriptor* pRenderPipDesc = MTL4::RenderPipelineDescriptor::alloc()->init();
        pRenderPipDesc->setLabel(MTLSTR("Lighting"));
        
    }
}

GameCoordinatorLoupy::~GameCoordinatorLoupy()
{
    for (uint8_t i = 0; i < kMaxFramesInFlight; ++i)
    {
        _pTriangleDataBuffer[i]->release();
        _pCommandAllocator[i]->release();
        _pInstanceDataBuffer[i]->release();
        _pJDLVStateBuffer[i]->release();
        _pGridBuffer_A[i]->release();
        _pGridBuffer_B[i]->release();
    }
    _pJDLVComputePSO->release();
    _pJDLVRenderPSO->release();
    _pTexture->release();
    _pDepthStencilState->release();
    _pDepthStencilStateJDLV->release();
    _pPSO->release();
    _pShaderLibrary->release();
//    _pCommandBuffer->release();
    _pCommandQueue->release();
    _pResidencySet->release();
    _pArgumentTable->release();
    _pArgumentTableJDLV->release();
    _sharedEvent->release();
    _pViewportSizeBuffer->release();
    dispatch_release(_semaphore);
    _pDevice->release();
}

void GameCoordinatorLoupy::draw( MTK::View* _pView )
{
    NS::AutoreleasePool *pPool = NS::AutoreleasePool::alloc()->init();

    _currentFrameIndex += 1;

    const uint32_t frameIndex = _currentFrameIndex % kMaxFramesInFlight;
    
    if (_currentFrameIndex > kMaxFramesInFlight)
    {
        uint64_t const timeStampToWait = _currentFrameIndex - kMaxFramesInFlight;
        _sharedEvent->waitUntilSignaledValue(timeStampToWait, DISPATCH_TIME_FOREVER);
    }

    MTL::Viewport viewPort;
    viewPort.originX = 0.0;
    viewPort.originY = 0.0;
    viewPort.znear = 0.0;
    viewPort.zfar = 1.0;
    viewPort.width = (double)_pViewportSize.x;
    viewPort.height = (double)_pViewportSize.y;

#if !USE_CONST_GAME_TIME
    _uniforms_cpu->gameTime                     = 0.f;
#endif
    _uniforms_cpu->cameraUniforms               = _pCamera->uniforms();
    float gameTime                              = _frame * (1.0 / 60.f);
    _uniforms_cpu->frameTime                    = simd_max(0.001f, gameTime - 0);
    _uniforms_cpu->mouseState                   = (simd::float3){ _cursorPosition.x, _cursorPosition.y, float(_mouseButtonMask) };
    _uniforms_cpu->invScreenSize                = (simd::float2){ 1.f / _gBuffer0->width(), 1.f / _gBuffer0->height() };
    _uniforms_cpu->projectionYScale             = 1.73205066;
    _uniforms_cpu->ambientOcclusionContrast     = 3;
    _uniforms_cpu->ambientOcclusionScale        = 0.800000011;
    _uniforms_cpu->ambientLightScale            = 0.699999988;
    _uniforms_cpu->brushSize                    = _brushSize;

    
//    MTL4::CommandAllocator* pFrameAllocator = _pCommandAllocator[frameIndex];
//    pFrameAllocator->reset();
//
//    _pCommandBuffer[0] = _pDevice->newCommandBuffer();
//    _pCommandBuffer[0]->beginCommandBuffer(pFrameAllocator);
//
//    MTL4::RenderPassDescriptor* pRenderPassDescriptor = _pView->currentMTL4RenderPassDescriptor();
//    MTL::RenderPassColorAttachmentDescriptor* color0 = pRenderPassDescriptor->colorAttachments()->object(0);
//    color0->setLoadAction( MTL::LoadActionClear );
//    color0->setStoreAction( MTL::StoreActionStore );
////    color0->setClearColor( MTL::ClearColor(0.1, 0.1, 0.1, 1.0) );
//
//    MTL4::RenderCommandEncoder* renderPassEncoder = _pCommandBuffer[0]->renderCommandEncoder(pRenderPassDescriptor);
//    renderPassEncoder->setRenderPipelineState(_pPSO);
//    renderPassEncoder->setDepthStencilState( _pDepthStencilState );
//    renderPassEncoder->setViewport(viewPort);
//    
//    _pArgumentTable->setAddress(_pTriangleDataBuffer[frameIndex]->gpuAddress(), 0);
//    _pArgumentTable->setAddress(_pViewportSizeBuffer->gpuAddress(), 1);
//
//    renderPassEncoder->setArgumentTable(_pArgumentTable, MTL::RenderStageVertex);
//    renderPassEncoder->drawPrimitives( MTL::PrimitiveTypeTriangle, NS::UInteger(0), NS::UInteger(3) );
//    renderPassEncoder->endEncoding();
//    _pCommandBuffer[0]->endCommandBuffer();
//
//    _pCommandBuffer[1] = _pDevice->newCommandBuffer();
//    _pCommandBuffer[1]->beginCommandBuffer(pFrameAllocator);
//
//    JDLVState* jdlvState = static_cast<JDLVState*>(_pJDLVStateBuffer[frameIndex]->contents());
//    jdlvState->width = kGridWidth;
//    jdlvState->height = kGridHeight;
//    _pJDLVStateBuffer[frameIndex]->didModifyRange( NS::Range(0, sizeof(JDLVState)) );
//    MTL::Buffer* sourceGrid = _useBufferAAsSource ? _pGridBuffer_A[frameIndex] : _pGridBuffer_B[frameIndex];
//    MTL::Buffer* destGrid = _useBufferAAsSource ? _pGridBuffer_B[frameIndex] : _pGridBuffer_A[frameIndex];
//
//    _pArgumentTableJDLV->setAddress(sourceGrid->gpuAddress(), 0);
//    _pArgumentTableJDLV->setAddress(destGrid->gpuAddress(), 1);
//    _pArgumentTableJDLV->setAddress(_pJDLVStateBuffer[frameIndex]->gpuAddress(), 2);
////    renderPassEncoder->setArgumentTable(_pArgumentTableJDLV, MTL::RenderStageVertex);
//
//    MTL4::ComputeCommandEncoder* computeEncoder = _pCommandBuffer[1]->computeCommandEncoder();
//    computeEncoder->setLabel(MTLSTR("JDLV Compute"));
//
//    computeEncoder->setComputePipelineState(_pJDLVComputePSO);
//
//    computeEncoder->setArgumentTable(_pArgumentTableJDLV);
//
//    MTL::Size gridSize = MTL::Size(kGridWidth, kGridHeight, 1);
//    MTL::Size threadgroupSize = MTL::Size(16, 16, 1);
//    MTL::Size threadgroups = MTL::Size((gridSize.width + threadgroupSize.width - 1) / threadgroupSize.width, (gridSize.height + threadgroupSize.height - 1) / threadgroupSize.height, 1);
//
//    computeEncoder->dispatchThreadgroups(threadgroups, threadgroupSize);
//    computeEncoder->endEncoding();
//
//    _pCommandBuffer[1]->endCommandBuffer();
//    _pCommandBuffer[2] = _pDevice->newCommandBuffer();
//    _pCommandBuffer[2]->beginCommandBuffer(pFrameAllocator);
//
//    MTL4::RenderCommandEncoder* gridRenderPassEncoder = _pCommandBuffer[2]->renderCommandEncoder(pRenderPassDescriptor);
//
//    gridRenderPassEncoder->setRenderPipelineState(_pJDLVRenderPSO);
//    gridRenderPassEncoder->setDepthStencilState( _pDepthStencilStateJDLV );
//    gridRenderPassEncoder->setViewport(viewPortJDLV);
//
//    _pArgumentTableJDLV->setAddress(destGrid->gpuAddress(), 0);
//    _pArgumentTableJDLV->setAddress(_pJDLVStateBuffer[frameIndex]->gpuAddress(), 1);
//    gridRenderPassEncoder->setArgumentTable( _pArgumentTableJDLV, MTL::RenderStageVertex );
//    gridRenderPassEncoder->setArgumentTable( _pArgumentTableJDLV, MTL::RenderStageFragment );
//
//    gridRenderPassEncoder->drawPrimitives( MTL::PrimitiveTypeTriangle, NS::UInteger(0), NS::UInteger(6) );
//    gridRenderPassEncoder->endEncoding();
//
//    _pCommandBuffer[2]->endCommandBuffer();
//
//    _useBufferAAsSource = !_useBufferAAsSource;
//
//    CA::MetalDrawable* currentDrawable = _pView->currentDrawable();
//    _pCommandQueue->wait(currentDrawable);
//    _pCommandQueue->commit(_pCommandBuffer, 3);
//    _pCommandQueue->signalDrawable(currentDrawable);
//    _pCommandQueue->signalEvent(_sharedEvent, _currentFrameIndex);
//    currentDrawable->present();
    pPool->release();
}
