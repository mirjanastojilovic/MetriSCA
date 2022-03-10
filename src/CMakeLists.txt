# ---------------------------------------
# MetriSCA Library
# ---------------------------------------

include_directories(
	${CMAKE_CURRENT_SOURCE_DIR}
    ${SPAN_LITE_INCLUDE_DIRS}
)

set(INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../includes")

set(FILES
    ${INCLUDE_DIR}/forward.hpp
    ${INCLUDE_DIR}/metrisca.hpp
    ${INCLUDE_DIR}/version.hpp

    app/cli_application.cpp ${INCLUDE_DIR}/app/cli_application.hpp
    app/cli_model.cpp       ${INCLUDE_DIR}/app/cli_model.hpp
    app/cli_view.cpp        ${INCLUDE_DIR}/app/cli_view.hpp

                            ${INCLUDE_DIR}/core/application.hpp
    core/csv_writer.cpp     ${INCLUDE_DIR}/core/csv_writer.hpp
                            ${INCLUDE_DIR}/core/distinguisher.hpp
                            ${INCLUDE_DIR}/core/errors.hpp
                            ${INCLUDE_DIR}/core/fileloader.hpp
                            ${INCLUDE_DIR}/core/matrix.hpp
                            ${INCLUDE_DIR}/core/metric.hpp
                            ${INCLUDE_DIR}/core/model.hpp
                            ${INCLUDE_DIR}/core/profiler.hpp
    core/trace_dataset.cpp  ${INCLUDE_DIR}/core/trace_dataset.hpp

                            ${INCLUDE_DIR}/utils/crypto.hpp
                            ${INCLUDE_DIR}/utils/math.hpp
                            ${INCLUDE_DIR}/utils/numerics.hpp

    distinguishers/pearson_distinguisher.cpp distinguishers/pearson_distinguisher.hpp

    metrics/guess_metric.cpp metrics/guess_metric.hpp
    metrics/guessing_entropy_metric.cpp metrics/guessing_entropy_metric.hpp
    metrics/mi_metric.cpp metrics/mi_metric.hpp
    metrics/pi_metric.cpp metrics/pi_metric.hpp
    metrics/rank_metric.cpp metrics/rank_metric.hpp
    metrics/score_metric.cpp metrics/score_metric.hpp
    metrics/success_rate_metric.cpp metrics/success_rate_metric.hpp
    metrics/ttest_metric.cpp metrics/ttest_metric.hpp

    models/hamming_distance_model.cpp models/hamming_distance_model.hpp
    models/hamming_weight_model.cpp models/hamming_weight_model.hpp
    models/identity_model.cpp models/identity_model.hpp

    profilers/standard_profiler.cpp profilers/standard_profiler.hpp
)

add_library(metrisca STATIC
    ${FILES}
)

set_target_properties(metrisca PROPERTIES
    POSITION_INDEPENDENT_CODE 
        ON
)

target_include_directories(metrisca PUBLIC
    ${INCLUDE_DIR}
    ${SPAN_LITE_INCLUDE_DIRS}
)

# Sort source files into IDE folders
foreach(FILE ${FILES}) 
    # Get the directory of the source file
    get_filename_component(PARENT_DIR "${FILE}" DIRECTORY)

    # Remove common directory prefix to make the group
    string(REPLACE "${CMAKE_CURRENT_SOURCE_DIR}" "" GROUP "${PARENT_DIR}")
    string(REPLACE "${INCLUDE_DIR}" "" GROUP "${PARENT_DIR}")

    # Make sure we are using windows slashes
    if (WIN32)
        string(REPLACE "/" "\\" GROUP "${GROUP}")
    endif()

    source_group("${GROUP}" FILES "${FILE}")
endforeach()
