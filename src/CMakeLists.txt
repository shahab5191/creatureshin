add_executable(annshin
  main.cpp
  annshin/brain/network.cpp
  annshin/body/body.cpp
  annshin/world/world.cpp
  annshin/loop/simulation.cpp
)
target_include_directories(annshin PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

if(ANNSHIN_WINDOW)
  target_sources(annshin PRIVATE
    annshin/render/snapshot.cpp
    annshin/render/raylib_renderer.cpp
  )
  target_link_libraries(annshin PRIVATE raylib)
  target_compile_definitions(annshin PRIVATE ANNSHIN_HAVE_RAYLIB)
endif()
