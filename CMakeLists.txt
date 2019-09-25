cmake_minimum_required(VERSION 3.0.2)
project(entt_example)

set(CMAKE_CXX_STANDARD 17)

find_package(Protobuf REQUIRED)

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
)

protobuf_generate_cpp(PROTO_SRCS PROTO_HDRS
        src/messages/entity.proto
        src/messages/frame.proto
        src/messages/text_message.proto
        src/messages/message.proto
)

set(BUILD_PATH ${CMAKE_SOURCE_DIR}/bin)

add_library(entt_example SHARED
        ${SPACESHIP_SRC}
        ${PROTO_SRCS} ${PROTO_HDRS}
)

target_include_directories(entt_example PUBLIC
        ${Protobuf_INCLUDE_DIRS}
        ${CMAKE_CURRENT_BINARY_DIR}
)

target_link_libraries(entt_example
    ${GODOT_CPP_LIB}
    godot-cpp
    EnTT
    GameNetworkingSockets::shared
    ${Protobuf_LIBRARIES}
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
