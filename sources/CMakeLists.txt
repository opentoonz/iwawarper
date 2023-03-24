# Specify the minimum version of CMake 
cmake_minimum_required(VERSION 3.0)

# Specify the project name
project(IwaWarper VERSION 1.0.0)

set(MOC_HEADERS
aboutpopup.h
dvwidgets.h
formatsettingspopups.h
freehanddialog.h
freehandtool.h
infodialog.h
iocommand.h
iwapp.h
iwlayer.h
iwlayerhandle.h
iwrendercommand.h
iwrenderinstance.h
iwprojecthandle.h
iwselection.h
iwselectionhandle.h
iwshapepairselection.h
iwtoolhandle.h
mainstatusbar.h
mainwindow.h
menubarcommand.h
myslider.h
outputsettingsdialog.h
renderprogresspopup.h
rendersettingsdialog.h
reshapetoolcontextmenu.h
sceneviewer.h
settingsdialog.h
shapeoptionsdialog.h
shapepair.h
timelinewindow.h
tooloptionpanel.h
transformdialog.h
ruler.h
layeroptionpanel.h
shapetreewindow.h
shapeinterpolationpopup.h
rendershapeimagedialog.h
)

set(HEADERS
    ${MOC_HEADERS}
    circletool.h
    colorsettings.h
    correspondencetool.h
    cursormanager.h
    cursors.h
    iwdialog.h
    iwimagecache.h
    iwmimedata.h
    iwproject.h
    iwtimelinekeyselection.h
    iwtimelineselection.h
    iwtool.h
    iwundomanager.h
    keycontainer.h
    keyframedata.h
    keyframeeditor.h
    menubarcommandids.h
    mypropertygroup.h
    outputsettings.h
    pentool.h
    personalsettingsids.h
    personalsettingsmanager.h
    pointdragtool.h
    preferences.h
    projectutils.h
    rendersettings.h
    reshapetool.h
    shapepairdata.h
    squaretool.h
    timelinedata.h
    toolbox.h
    toolcommandids.h
    transformdragtool.h
    transformtool.h
    viewsettings.h
    halfedge.h
    iwtrianglecache.h
    iwfolders.h
    keydragtool.h
    logger.h
    shapetagsettings.h
)

set(CONTROL_SOURCES
iwlayerhandle.cpp
iwprojecthandle.cpp
iwselectionhandle.cpp
iwtoolhandle.cpp
iwrendercommand.cpp
iwrenderinstance.cpp
iwselection.cpp
iwshapepairselection.cpp
iwtimelinekeyselection.cpp
iwtimelineselection.cpp
circletool.cpp
correspondencetool.cpp
freehandtool.cpp
iwtool.cpp
pentool.cpp
pointdragtool.cpp
reshapetool.cpp
reshapetoolcontextmenu.cpp
squaretool.cpp
transformdragtool.cpp
transformtool.cpp
iocommand.cpp
iwimagecache.cpp
iwundomanager.cpp
menubarcommand.cpp
personalsettingsmanager.cpp
projectutils.cpp
keydragtool.cpp
)

set(MODEL_SOURCES
iwmimedata.cpp
keyframedata.cpp
shapepairdata.cpp
timelinedata.cpp
colorsettings.cpp
iwapp.cpp
iwlayer.cpp
iwproject.cpp
keycontainer.cpp
mypropertygroup.cpp
outputsettings.cpp
rendersettings.cpp
shapepair.cpp
viewsettings.cpp
iwtrianglecache.cpp
iwfolders.cpp
halfedge.cpp
shapetagsettings.cpp
preferences.cpp
)

set(VIEW_SOURCES
aboutpopup.cpp
freehanddialog.cpp
infodialog.cpp
iwdialog.cpp
outputsettingsdialog.cpp
rendersettingsdialog.cpp
settingsdialog.cpp
shapeoptionsdialog.cpp
transformdialog.cpp
mainstatusbar.cpp
mainwindow.cpp
sceneviewer.cpp
toolbox.cpp
tooloptionpanel.cpp
dvwidgets.cpp
formatsettingspopups.cpp
renderprogresspopup.cpp
timelinewindow.cpp
cursormanager.cpp
myslider.cpp
ruler.cpp
layeroptionpanel.cpp
shapetreewindow.cpp
shapeinterpolationpopup.cpp
rendershapeimagedialog.cpp
)

set(SOURCES
${CONTROL_SOURCES}
${MODEL_SOURCES}
${VIEW_SOURCES}
main.cpp
)

#-----------------------------------------------------------------------------
# Select build environment
set(BUILD_ENV_MSVC     OFF)
set(BUILD_ENV_APPLE    OFF)
if(MSVC)
    set(BUILD_ENV_MSVC ON)
    message(STATUS "Build environment is MSVC")
elseif(CMAKE_HOST_APPLE)
    set(BUILD_ENV_APPLE ON)
    message(STATUS "Build environment is APPLE")
else()
    message(ERROR "Unsupported build environment, only MSVC and APPLE are supported")
endif()

#-----------------------------------------------------------------------------
# Build Options
option(WITH_TRANSLATION "Generate translation projects as well" ON)

#-----------------------------------------------------------------------------
# Third Party & System Libs
get_filename_component(SDKROOT ../thirdparty/ ABSOLUTE)
message(STATUS "SDK Root:" ${SDKROOT})

# specify qtdir
if(BUILD_ENV_MSVC)
    set(QTDIR $ENV{QTDIR} CACHE PATH "Qt installation directory")
    if(NOT EXISTS ${QTDIR})
        message("Specify QTDIR properly")
        return()
    endif()
elseif(BUILD_ENV_APPLE)
    set(QTDIR $ENV{QTDIR} CACHE PATH "Qt installation directory")
    add_definitions(
        -DMACOSX
        -Di386
        -D__MACOS__
    )
endif()
set(CMAKE_PREFIX_PATH "${CMAKE_PREFIX_PATH};${QTDIR};${QTDIR}/lib/cmake")

if(BUILD_ENV_APPLE)
    set(CMAKE_OSX_DEPLOYMENT_TARGET 10.10)
endif()

# Add Qt libraries. QtCore and QtGui libraries will be 
# added accordingly as the Widgets depends on them. 
find_package( Qt5 REQUIRED
    Widgets
    LinguistTools
    OpenGL
    Svg
)

# ビルドディレクトリを include ディレクトリに追加するオプションを有効にする。
set(CMAKE_INCLUDE_CURRENT_DIR ON)

set(MOC_SOURCES)

qt5_wrap_cpp(MOC_SOURCES ${MOC_HEADERS})
# qt5_wrap_cpp(SOURCES ${MOC_HEADERS})

source_group("Source Files\\0_Moc Files" FILES ${MOC_SOURCES})

source_group("Source Files\\1_Model" FILES ${MODEL_SOURCES})
source_group("Source Files\\2_View" FILES ${VIEW_SOURCES})
source_group("Source Files\\3_Control" FILES ${CONTROL_SOURCES})

set(SOURCES
    ${SOURCES}
    ${MOC_SOURCES}
)

# resource
set(QRC_FILE IwaWarper_resources.qrc)
# set(QRC_FILE IwaWarper_resources.qrc "./Resources/style_dark/style.qrc")
qt5_add_resources(RESOURCES ${QRC_FILE})

if(BUILD_ENV_MSVC)
    set(RC_FILE IwaWarper.rc)
endif()

if(WITH_TRANSLATION)
    # generate Qt translations and messages
    set(LANGUAGES ja)
    # 翻訳対象のファイルを設定する。
    SET(TRANSLATION_TARGET_FILES
        ${HEADERS}
        ${SOURCES}
    )

    set(translation)
    foreach(lang ${LANGUAGES})
        set(name "${CMAKE_SOURCE_DIR}/translations/IwaWarper_${lang}.ts")
        list(APPEND translation ${name})
        set_source_files_properties(${name} PROPERTIES
            OUTPUT_LOCATION "${CMAKE_SOURCE_DIR}/loc"
        )
    endforeach()

    qt5_create_translation(message ${translation} ${TRANSLATION_TARGET_FILES})

    add_custom_target("translation_IwaWarper" DEPENDS ${message})
    set_target_properties("translation_IwaWarper" PROPERTIES
        EXCLUDE_FROM_DEFAULT_BUILD TRUE
    )
endif()

include_directories(
    ${SDKROOT}/OpenToonz/include
)

# ライブラリパスの指定
if(BUILD_ENV_MSVC)
    set(GLUT_LIB ${SDKROOT}/glut/3.7.6/lib/glut64.lib)
    set(GL_LIB opengl32.lib)
elseif(BUILD_ENV_APPLE)
    find_library(GLUT_LIB GLUT)
    find_library(GL_LIB OpenGL)
endif()

macro(_find_toonz_library OUT_LIB_LIST IN_LIB_LIST)
    set(OUTLIST "")
    foreach(arg ${IN_LIB_LIST})
        if(BUILD_ENV_MSVC)
            set(TOONZLIBPATH "${SDKROOT}/OpenToonz/lib/${arg}.lib")
        elseif(BUILD_ENV_APPLE)
            set(TOONZLIBPATH "${SDKROOT}/OpenToonz/dylib/lib${arg}.dylib")
        endif()
        set(OUTLIST ${OUTLIST} ${TOONZLIBPATH})
    endforeach()
    set(${OUT_LIB_LIST} ${OUTLIST})
endmacro()
_find_toonz_library(TOONZ_LIBS "tnzcore;tnzbase;toonzlib;image;colorfx")

# 生成する実行ファイルを追加する
if(BUILD_ENV_MSVC)
    add_executable(${PROJECT_NAME} WIN32 ${HEADERS} ${SOURCES} ${RESOURCES} ${RC_FILE})
else()
    set(MACOSX_BUNDLE_ICON_FILE "IwaWarper.icns")
    set_source_files_properties(IwaWarper.icns PROPERTIES MACOSX_PACKAGE_LOCATION "Resources")
    add_executable(${PROJECT_NAME} MACOSX_BUNDLE IwaWarper.icns ${HEADERS} ${SOURCES} ${RESOURCES})
endif()

# link Qt library. Core and Gui will be linked accordingly
target_link_libraries(${PROJECT_NAME} Qt5::Widgets Qt5::OpenGL Qt5::Svg
${GL_LIB} ${GLUT_LIB} ${TOONZ_LIBS}
)

add_definitions(
    -DNOMINMAX
    -D_USE_MATH_DEFINES
    -DTNZ_LITTLE_ENDIAN=1
)

if(BUILD_ENV_APPLE)
# info plists
    set_target_properties(${PROJECT_NAME} PROPERTIES MACOSX_BUNDLE_INFO_PLIST
        ${CMAKE_SOURCE_DIR}/../macOS/BundleInfo.plist.in)

# resources
    set_source_files_properties(${MACOSX_RESOURCE_FILES} PROPERTIES MACOSX_PACKAGE_LOCATION Resources)
    find_program(IBTOOL ibtool HINTS "/usr/bin" "${OSX_DEVELOPER_ROOT}/usr/bin")
    add_custom_command (TARGET ${PROJECT_NAME} PRE_BUILD COMMAND mkdir -p ${PROJECT_NAME}/Contents/Resources)
endif()

# Set a default build type to Release
if (NOT EXISTS ${CMAKE_BINARY_DIR}/CMakeCache.txt)
    if (NOT CMAKE_BUILD_TYPE)
        set(CMAKE_BUILD_TYPE "Release" CACHE STRING "" FORCE)
    endif()
endif()
