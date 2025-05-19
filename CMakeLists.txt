cmake_minimum_required(VERSION 3.16)
project(KrkrVoice VERSION 1.00)

# ── ビルド種別オプション ───────────────────────────────
option(BUILD_EXE "Build executable instead of library" OFF)

# ── 共通 / 個別ソース ────────────────────────────────
set(COMMON_SRC  src/krkrvoice.cpp  src/krkrvoice_win.cpp)
if(BUILD_EXE)
    list(APPEND COMMON_SRC src/main.cpp)
else()
    list(APPEND COMMON_SRC src/plugin.cpp)
endif()
file(GLOB HEADERS "src/*.h" "src/*.hpp")

# ── ncbind サブディレクトリ ───────────────────────────
if (NOT TARGET ncbind)
    add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/../ncbind
                     ${CMAKE_CURRENT_BINARY_DIR}/ncbind)
endif() これでいいの?

# ── ターゲット作成 ───────────────────────────────────
if(BUILD_EXE)
    add_executable(KrkrVoice ${COMMON_SRC} ${HEADERS})
else()
    add_library   (KrkrVoice SHARED ${COMMON_SRC} ${HEADERS})
endif()

target_link_libraries(KrkrVoice PUBLIC ncbind winmm)

# ── ランタイムを “ターゲット単位” で振り分ける ──────
if(BUILD_EXE)
    # EXE → /MT or /MTd
    set_property(TARGET KrkrVoice
                 PROPERTY MSVC_RUNTIME_LIBRARY
                 "MultiThreaded$<$<CONFIG:Debug>:Debug>")
else()
    # DLL → /MD or /MDd
    set_property(TARGET KrkrVoice
                 PROPERTY MSVC_RUNTIME_LIBRARY
                 "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

# （サブディレクトリの ncbind / tp_stub も同じ規則にしたい場合は
#  target_link_libraries 後に同様の set_property() を追加で呼ぶ）

# ── 共通のコンパイルオプションなど ──────────────────
target_compile_features(KrkrVoice PUBLIC cxx_std_17)
target_compile_options(KrkrVoice PRIVATE
    "$<$<CXX_COMPILER_ID:MSVC>:/utf-8;/Zc:__cplusplus>")
