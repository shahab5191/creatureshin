add_executable(annshin
  main.cpp
  annshin/brain/network.cpp
  annshin/body/body.cpp
)
target_include_directories(annshin PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
