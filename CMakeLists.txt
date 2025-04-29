cmake_minimum_required(VERSION 3.16)

# プロジェクト名とバージョン
set(PROJECT_NAME KrkrVoice)
set(PROJECT_VERSION 1.00)

project(${PROJECT_NAME} VERSION ${PROJECT_VERSION})

# ソースファイル
set(SOURCES
    src/main.cpp
    src/krkrsapi.cpp
)

# ヘッダーファイル（.h や .hpp）をグロブで指定
file(GLOB HEADERS "src/*.h" "src/*.hpp")

# UTF-8 ソースコード対応 (MSVC専用)
add_compile_options("$<$<AND:$<C_COMPILER_ID:MSVC>,$<COMPILE_LANGUAGE:C>>:/utf-8>")
add_compile_options("$<$<AND:$<CXX_COMPILER_ID:MSVC>,$<COMPILE_LANGUAGE:CXX>>:/utf-8>")
add_compile_options("$<$<AND:$<CXX_COMPILER_ID:MSVC>,$<COMPILE_LANGUAGE:CXX>>:/Zc:__cplusplus>")

# MSVCのスタティックランタイム使用設定 (/MT または /MTd)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

# コマンドライン引数を取得してEXEかDLLかを切り替え
option(BUILD_EXE "Build executable instead of library" OFF)

if(BUILD_EXE)
    # EXEを作成
    add_executable(${PROJECT_NAME} ${SOURCES} ${HEADERS})
else()
    # DLLを作成
    add_library(${PROJECT_NAME} SHARED ${SOURCES} ${HEADERS})
endif()

# ncbind がターゲットとしてまだ追加されていない場合
if(NOT TARGET ncbind)
    add_subdirectory(../ncbind ${CMAKE_CURRENT_BINARY_DIR}/ncbind)
endif()

# ncbind ライブラリをリンク
target_link_libraries(${PROJECT_NAME} PUBLIC
    ncbind
)

# （オプション）C++17 設定
set_target_properties(${PROJECT_NAME} PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED YES
)