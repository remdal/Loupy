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
#include "RMDLUtilities.h"

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
        _pCommandAllocator[i] = _pDevice->newCommandAllocator();

        _pShadowPassDataBuffer[i] = _pDevice->newBuffer( sizeof(simd::float4x4), MTL::ResourceStorageModeShared );
    }
    //    buildJDLVPipelines();
    //    buildDepthStencilStates();
//    const NS::UInteger nativeWidth = (NS::UInteger)(w/1.2);
//    const NS::UInteger nativeHeight = (NS::UInteger)(h/1.2);
    _pViewportSize.x = (float)w;
    _pViewportSize.y = (float)h;
    _pViewportSizeBuffer = _pDevice->newBuffer(sizeof(_pViewportSize), MTL::ResourceStorageModeShared);
    ft_memcpy(_pViewportSizeBuffer->contents(), &_pViewportSize, sizeof(_pViewportSize));

    _brushSize = 1000.0f;
    {
        static const NS::UInteger shadowWidth = 1024;
        _pTextureDesc = MTL::TextureDescriptor::texture2DDescriptor( MTL::PixelFormatDepth32Float, shadowWidth, shadowWidth, false );
        _pTextureDesc->setTextureType( MTL::TextureType2DArray );
        _pTextureDesc->setArrayLength(3);
        _pTextureDesc->setUsage( MTL::TextureUsageRenderTarget) ;
        _pTextureDesc->setStorageMode( MTL::StorageModePrivate );
        _shadowMap = _pDevice->newTexture(_pTextureDesc);
        _shadowMap->setLabel(MTLSTR("Shadow"));
    }

    NS::SharedPtr<MTL::DepthStencilDescriptor> depthStateDesc = NS::TransferPtr( MTL::DepthStencilDescriptor::alloc()->init() );
    depthStateDesc->setDepthCompareFunction( MTL::CompareFunctionLess );
    depthStateDesc->setDepthWriteEnabled( true );

    
    _shadowPassDesc->depthAttachment()->setTexture(_shadowMap);
    _shadowPassDesc->depthAttachment()->setClearDepth( 1.f );
    _shadowPassDesc->depthAttachment()->setLoadAction( MTL::LoadActionClear );
    _shadowPassDesc->depthAttachment()->setStoreAction( MTL::StoreActionStore );

    _shadowDepthState = _pDevice->newDepthStencilState(depthStateDesc.get());

    _gBufferPassDesc->depthAttachment()->setClearDepth( 1.0f );
    _gBufferPassDesc->depthAttachment()->setLevel(0);
    _gBufferPassDesc->depthAttachment()->setSlice(0);
    _gBufferPassDesc->depthAttachment()->setTexture( _shadowMap );
    _gBufferPassDesc->depthAttachment()->setLoadAction( MTL::LoadActionClear );
    _gBufferPassDesc->depthAttachment()->setStoreAction( MTL::StoreActionStore );

    _gBufferPassDesc->colorAttachments()->object(0)->setLoadAction( MTL::LoadActionDontCare );
    _gBufferPassDesc->colorAttachments()->object(0)->setStoreAction( MTL::StoreActionStore );
    _gBufferPassDesc->colorAttachments()->object(1)->setLoadAction( MTL::LoadActionDontCare );
    _gBufferPassDesc->colorAttachments()->object(1)->setStoreAction( MTL::StoreActionStore );

    depthStateDesc->setDepthCompareFunction( MTL::CompareFunctionLess );
    depthStateDesc->setDepthWriteEnabled( true );

    _gBufferDepthState = _pDevice->newDepthStencilState(depthStateDesc.get());

    _gBufferWithLoadPassDesc = _gBufferPassDesc;
    _gBufferWithLoadPassDesc->depthAttachment()->setLoadAction( MTL::LoadActionLoad );
    _gBufferWithLoadPassDesc->colorAttachments()->object(0)->setLoadAction( MTL::LoadActionLoad );
    _gBufferWithLoadPassDesc->colorAttachments()->object(1)->setLoadAction( MTL::LoadActionLoad );

    _lightingPassDesc->colorAttachments()->object(0)->setLoadAction( MTL::LoadActionDontCare );
    _lightingPassDesc->colorAttachments()->object(0)->setStoreAction( MTL::StoreActionStore );

    depthStateDesc->setDepthCompareFunction( MTL::CompareFunctionAlways );
    depthStateDesc->setDepthWriteEnabled( false );
    _lightingDepthState = _pDevice->newDepthStencilState(depthStateDesc.get());
    {
        NS::SharedPtr<MTL4::RenderPipelineDescriptor> pRenderPipDesc = NS::TransferPtr( MTL4::RenderPipelineDescriptor::alloc()->init() );
        pRenderPipDesc->setLabel(MTLSTR("Lighting"));

        NS::SharedPtr<MTL4::LibraryFunctionDescriptor> vertexFunction = NS::TransferPtr( MTL4::LibraryFunctionDescriptor::alloc()->init() );
        vertexFunction->setName(MTLSTR("LightingVs"));
        vertexFunction->setLibrary(_pShaderLibrary);
        pRenderPipDesc->setVertexFunctionDescriptor(vertexFunction.get());

        NS::SharedPtr<MTL4::LibraryFunctionDescriptor> fragmentFunction = NS::TransferPtr( MTL4::LibraryFunctionDescriptor::alloc()->init() );
        fragmentFunction->setName( NS::String::string( "LightingPs", NS::ASCIIStringEncoding ) );
        fragmentFunction->setLibrary(_pShaderLibrary);
        pRenderPipDesc->setFragmentFunctionDescriptor(fragmentFunction.get());

        pRenderPipDesc->setRasterSampleCount(1);
        pRenderPipDesc->colorAttachments()->object(0)->setPixelFormat( MTL::PixelFormatRGBA16Float );

        NS::SharedPtr<MTL4::Compiler> compiler = NS::TransferPtr(_pDevice->newCompiler( MTL4::CompilerDescriptor::alloc()->init(), &pError ));
        _pPSO = compiler->newRenderPipelineState(pRenderPipDesc.get(), nullptr, &pError);


        simd::float4 initialMouseWorldPos = (simd::float4){ 0.f, 0.f, 0.f, 0.f };
        _mouseBuffer = _pDevice->newBuffer( &initialMouseWorldPos, sizeof(initialMouseWorldPos), MTL::ResourceStorageModeManaged );

        MTL4::ComputePipelineDescriptor *pPipStateDesc = MTL4::ComputePipelineDescriptor::alloc()->init();
        pPipStateDesc->setThreadGroupSizeIsMultipleOfThreadExecutionWidth(true);


        NS::SharedPtr<MTL::Function> computeFunction = NS::TransferPtr(_pShaderLibrary->newFunction(MTLSTR("mousePositionUpdate")));
        _mousePositionComputeKnl = _pDevice->newComputePipelineState(computeFunction.get(), 0, nullptr, &pError);



        NS::SharedPtr<MTL::ResidencySetDescriptor> residencySetDescriptor =
            NS::TransferPtr( MTL::ResidencySetDescriptor::alloc()->init() );
        _pResidencySet = _pDevice->newResidencySet(residencySetDescriptor.get(), &pError);
        _pResidencySet->requestResidency();
        for (uint8_t i = 0u; i < kMaxFramesInFlight; ++i)
        {
        }
        _pResidencySet->addAllocation(_pViewportSizeBuffer);
        _pResidencySet->commit();
        _pCommandQueue->addResidencySet(_pResidencySet); // .get() is for struct
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
    _pTextureDesc->release();
    _pDepthStencilState->release();
    _pDepthStencilStateJDLV->release();
    _pPSO->release();
    _pShaderLibrary->release();
    _gBuffer0->release();
    _gBuffer1->release();
    _shadowMap->release();
    _depth->release();
    _gBufferPassDesc->release();
    _shadowPassDesc->release();
    _lightingPassDesc->release();
    _gBufferWithLoadPassDesc->release();
    
    
    
    _shadowMap->release();


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

void GameCoordinatorLoupy::updateUniforms()
{
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
    {
        const simd::float3 sunDir = simd::normalize((simd::float3){ 1, -0.7, 0.5 });
        float tan_half_angle = tanf(_pCamera->viewAngle() * 0.5f) * sqrtf(2.0);
        float half_angle = atanf(tan_half_angle);
        float sine_half_angle = sinf(half_angle);
        std::cout << tan_half_angle << half_angle << sine_half_angle << std::endl;
        AAPL_PRINT(tan_half_angle + half_angle + sine_half_angle);
        
        
        float cascade_sizes[3] = { 400.0f, 1600.0f, 6400.0f };
        float cascade_distances[3];
        cascade_distances[0] = 2 * cascade_sizes[0] * (1.0f - sine_half_angle * sine_half_angle);
        cascade_distances[1] = sqrtf(cascade_sizes[1] * cascade_sizes[1] - cascade_distances[0] * cascade_distances[0] * tan_half_angle * tan_half_angle) * cascade_distances[0];
        cascade_distances[2] = sqrtf(cascade_sizes[2] * cascade_sizes[2] - cascade_distances[1] * cascade_distances[1] * tan_half_angle * tan_half_angle) * cascade_distances[1];
        std::cout << cascade_distances[0] << cascade_distances[1] << cascade_distances[2] << std::endl;

        for (uint c = 0; c < 3; c++)
        {
            simd::float3 center = _pCamera->position() + _pCamera->direction() * cascade_distances[c];
            float size = cascade_sizes[c];
            float stepsize = size / 64.0f;
            RMDLCamera* shadow_cam = nullptr;
            shadow_cam->initParallelWithPosition(center - sunDir * size,
                                                 sunDir,
                                                 simd::float3{ 0.0f, 1.0f, 0.0f },
                                                 size * 2.0f,
                                                 size * 2.0f,
                                                 0.0f,
                                                 size * 2.0f);
            auto positionP = Fract( simd::dot(center, shadow_cam->up() ) / stepsize) * shadow_cam->up() * stepsize;
            auto positionN = Fract( simd::dot(center, shadow_cam->right() ) / stepsize) * shadow_cam->right() * stepsize;
            shadow_cam->setPosition(positionP - positionN);
            _uniforms_cpu->shadowCameraUniforms[c] = shadow_cam->uniforms();
        }
    }
}

void GameCoordinatorLoupy::makeArgumentTable()
{
    NS::Error* pError = nullptr;

    NS::SharedPtr<MTL4::ArgumentTableDescriptor> argumentTableDescriptor = NS::TransferPtr( MTL4::ArgumentTableDescriptor::alloc()->init() );
    argumentTableDescriptor->setMaxBufferBindCount(2);

    _pArgumentTable = _pDevice->newArgumentTable(argumentTableDescriptor.get(), &pError);
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
    viewPort.width = (double)_shadowMap->width();
    viewPort.height = (double)_shadowMap->height();

    updateUniforms();
    
    MTL4::CommandAllocator* pFrameAllocator = _pCommandAllocator[frameIndex];
    pFrameAllocator->reset();

    _pCommandBuffer[0] = _pDevice->newCommandBuffer();
    _pCommandBuffer[0]->beginCommandBuffer(pFrameAllocator);

    for (uint32_t i = 0; i < 3; i++)
    {
        _shadowPassDesc->depthAttachment()->setSlice(i);
        MTL4::RenderCommandEncoder* renderPassEncoder = _pCommandBuffer[0]->renderCommandEncoder(_shadowPassDesc);
        renderPassEncoder->setCullMode( MTL::CullModeFront );
        renderPassEncoder->setDepthClipMode( MTL::DepthClipModeClamp );
        renderPassEncoder->setDepthStencilState(_shadowDepthState);

        renderPassEncoder->setViewport(viewPort);
        renderPassEncoder->setScissorRect( MTL::ScissorRect {0, 0, _shadowMap->width(), _shadowMap->height()} );
        ft_memcpy(_pShadowPassDataBuffer[i], &_uniforms_cpu->shadowCameraUniforms[i].viewProjectionMatrix, sizeof(_uniforms_cpu->shadowCameraUniforms[i].viewProjectionMatrix));
    }
    _pCommandBuffer[0]->endCommandBuffer();

    _pCommandBuffer[1] = _pDevice->newCommandBuffer();
    _pCommandBuffer[1]->beginCommandBuffer(_pCommandAllocator[frameIndex]);
    {
        MTL4::RenderCommandEncoder* renderEncoder = _pCommandBuffer[1]->renderCommandEncoder(_gBufferPassDesc);
        renderEncoder->setCullMode( MTL::CullModeBack );
        renderEncoder->setDepthStencilState(_gBufferDepthState);
    }
    {
        MTL4::ComputeCommandEncoder* computeEncoder = _pCommandBuffer[2]->computeCommandEncoder();
        computeEncoder->setLabel(MTLSTR("CCE2knlMousePos"));
        computeEncoder->setComputePipelineState(_mousePositionComputeKnl);
    }
    {
        MTL4::RenderCommandEncoder* renderEncoderDepth = _pCommandBuffer[3]->renderCommandEncoder(_lightingPassDesc);
        
    }

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
    CA::MetalDrawable* currentDrawable = _pView->currentDrawable();
    _pCommandQueue->wait(currentDrawable);
    _pCommandQueue->commit(_pCommandBuffer, 0);
    _pCommandQueue->signalDrawable(currentDrawable);
    _pCommandQueue->signalEvent(_sharedEvent, _currentFrameIndex);
    currentDrawable->present();
    pPool->release();
}
