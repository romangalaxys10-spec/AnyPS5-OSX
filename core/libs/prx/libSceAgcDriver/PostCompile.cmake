set(agcColorTransferSource ${CMAKE_CURRENT_SOURCE_DIR}/Graphics/shaders/ColorTransfer.comp)
set(agcColorTransferSpv ${agcTextureDetileGeneratedDir}/ColorTransfer.spv)
set(agcColorTransferHeader ${agcTextureDetileGeneratedDir}/ColorTransfer_spv.h)
add_custom_command(
    OUTPUT "${agcColorTransferHeader}"
    COMMAND ${CMAKE_COMMAND} -E make_directory "${agcTextureDetileGeneratedDir}"
    COMMAND "$<TARGET_FILE:glslang-standalone>" -V --target-env vulkan1.1 -o "${agcColorTransferSpv}" "${agcColorTransferSource}"
    COMMAND ${CMAKE_COMMAND} -DINPUT=${agcColorTransferSpv} -DOUTPUT=${agcColorTransferHeader} -DSYMBOL=COLOR_TRANSFER_SPV -P "${CMAKE_CURRENT_SOURCE_DIR}/embed_spirv.cmake"
    DEPENDS "${agcColorTransferSource}" glslang-standalone
    VERBATIM
)

foreach(agcTarget IN ITEMS libSceAgcDriver agc_driver_visual_test agc_driver_graphics_tests agc_driver_bda_device_tests)
    if(TARGET ${agcTarget})
        target_sources(${agcTarget} PRIVATE Graphics/src/BufferPool.cpp Graphics/src/CommandCompletion.cpp)
    endif()
endforeach()

foreach(agcTarget IN ITEMS libSceAgcDriver agc_driver_visual_test agc_driver_graphics_tests agc_driver_bda_device_tests)
    if(TARGET ${agcTarget})
        target_sources(${agcTarget} PRIVATE Execution/src/GuestGpuRange.cpp)
    endif()
endforeach()

foreach(agcTarget IN ITEMS libSceAgcDriver agc_driver_visual_test agc_driver_graphics_tests)
    if(TARGET ${agcTarget})
        target_sources(${agcTarget} PRIVATE Graphics/src/TextureDetilerDescriptors.cpp Graphics/src/TextureCache.cpp Graphics/src/RegisteredGpuMemory.cpp)
    endif()
endforeach()

foreach(agcTarget IN ITEMS libSceAgcDriver agc_driver_visual_test agc_driver_graphics_tests agc_driver_bda_device_tests)
    if(TARGET ${agcTarget})
        target_sources(${agcTarget} PRIVATE Graphics/src/GpuColorTransfer.cpp Graphics/src/ColorMemoryWriteback.cpp ${agcColorTransferHeader})
    endif()
endforeach()

target_sources(agc_driver_bda_device_tests PRIVATE tests/ColorTransferTests.cpp Graphics/src/ColorTargetLayout.cpp Execution/src/GuestMemory.cpp)
target_include_directories(agc_driver_bda_device_tests PRIVATE ${agcTextureDetileGeneratedRoot})

if(TARGET agc_driver_visual_test)
    target_sources(agc_driver_visual_test PRIVATE Execution/src/DisplayBuffer.cpp)
endif()

foreach(agcTarget IN ITEMS libSceAgcDriver agc_driver_visual_test agc_driver_graphics_tests)
    if(TARGET ${agcTarget})
        target_sources(${agcTarget} PRIVATE Graphics/src/RenderCache.cpp Graphics/src/RenderMemoryOwnership.cpp Graphics/src/DrawQueue.cpp Graphics/src/DrawCompletion.cpp Graphics/src/RenderTexture.cpp Graphics/src/DescriptorCache.cpp)
    endif()
endforeach()

foreach(agcTarget IN ITEMS libSceAgcDriver agc_driver_visual_test)
    if(TARGET ${agcTarget})
        target_sources(${agcTarget} PRIVATE Execution/src/PresentationImage.cpp Execution/src/DeviceSynchronization.cpp Graphics/src/GraphicsPipelineCache.cpp)
    endif()
endforeach()
