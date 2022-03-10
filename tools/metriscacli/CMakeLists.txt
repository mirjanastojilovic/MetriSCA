# ---------------------------------------
# MetriSCACli Tool
# ---------------------------------------

include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}
)

add_executable(metriscacli
    src/main.cpp
)

target_link_libraries(metriscacli PUBLIC
    metrisca
)
