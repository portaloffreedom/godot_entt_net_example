cmake_minimum_required(VERSION 3.0.2)
project(entt_example)

set(CMAKE_CXX_STANDARD 17)

find_package(Flatbuffers REQUIRED)

add_subdirectory(thirdparty/godot-cpp/)
add_subdirectory(thirdparty/entt/)
add_subdirectory(thirdparty/GameNetworkingSockets/)

if(MSVC)
    # Force to always compile with W4
    if(CMAKE_CXX_FLAGS MATCHES "/W[0-4]")
        string(REGEX REPLACE "/W[0-4]" "/W4" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
    else()
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /W4")
    endif()
elseif(CMAKE_COMPILER_IS_GNUCC OR CMAKE_COMPILER_IS_GNUCXX)
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall")
endif()

set(SPACESHIP_SRC
    src/gdlibrary.cpp
    src/Entity.cpp
    src/EntityManager.cpp
    src/Server.cpp
    src/Client.cpp
    src/Network.cpp
)
set(SPACESHIP_HDRS
    src/Entity.h
    src/EntityManager.h
    src/Server.h
    src/Client.h
    src/Network.h
    src/MessageContainer.h
)

function(FLATBUFFERS_GENERATE_C_HEADERS Name)
    set(FLATC_OUTPUTS)
    foreach(FILE ${ARGN})
      get_filename_component(FLATC_OUTPUT ${FILE} NAME_WE)
      set(FLATC_OUTPUT
        "${CMAKE_CURRENT_BINARY_DIR}/${FLATC_OUTPUT}_generated.h")
      list(APPEND FLATC_OUTPUTS ${FLATC_OUTPUT})
      add_custom_command(OUTPUT ${FLATC_OUTPUT}
        COMMAND flatbuffers::flatc
        ARGS -c -o "${CMAKE_CURRENT_BINARY_DIR}/generated/" ${FILE}
        COMMENT "Building C++ header for ${FILE}"
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
    endforeach()
    set(${Name}_OUTPUTS ${FLATC_OUTPUTS} PARENT_SCOPE)
endfunction()

FLATBUFFERS_GENERATE_C_HEADERS(FlatBuffers
        src/messages/entity.fbs
        src/messages/frame.fbs
        src/messages/text_message.fbs
        src/messages/message.fbs
)

set(BUILD_PATH ${CMAKE_SOURCE_DIR}/bin)

add_library(entt_example SHARED
        ${SPACESHIP_SRC}
        ${SPACESHIP_HDRS}
        ${FlatBuffers_OUTPUTS}
)

target_include_directories(entt_example PUBLIC
        "${CMAKE_CURRENT_BINARY_DIR}/generated/"
)

target_link_libraries(entt_example
    ${GODOT_CPP_LIB}
    godot-cpp
    EnTT
    GameNetworkingSockets::shared
    flatbuffers::flatbuffers_shared
)

target_compile_definitions(entt_example
        PUBLIC
        STEAMNETWORKINGSOCKETS_OPENSOURCE=1
)

# create the library directly in the right folder
set_target_properties(entt_example PROPERTIES
	LIBRARY_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bin)


add_executable(server
        src/Server.cpp src/main_server.cpp)
add_executable(client
        src/Client.cpp src/main_client.cpp)

target_link_libraries(server entt_example)
target_link_libraries(client entt_example)
