add_library(nordend_svg src/xml.cpp src/css.cpp src/geometry.cpp src/parse.cpp src/paint.cpp src/raster.cpp src/render.cpp src/filter.cpp src/image.cpp)
add_library(nordend::svg ALIAS nordend_svg)
target_include_directories(nordend_svg PUBLIC include)
target_link_libraries(nordend_svg PRIVATE PkgConfig::FREETYPE nordend::picture nordend::warnings)
# The public API returns picture::Image.
target_include_directories(nordend_svg PUBLIC $<TARGET_PROPERTY:nordend_picture,INTERFACE_INCLUDE_DIRECTORIES>)
