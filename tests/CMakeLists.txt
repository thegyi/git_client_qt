find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Test)

if (MSVC)
  set(MSVC_STDEXT_SHIM
      "/FI\"${CMAKE_SOURCE_DIR}/src/msvc_stdext_shim.h\"")
endif()

add_executable(TestGitExecutor TestGitExecutor.cpp
               ${CMAKE_SOURCE_DIR}/src/GitExecutor.cpp
               ${CMAKE_SOURCE_DIR}/src/GitExecutor.h)
target_include_directories(TestGitExecutor PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(TestGitExecutor PRIVATE
                      Qt${QT_VERSION_MAJOR}::Core
                      Qt${QT_VERSION_MAJOR}::Test)
if (MSVC)
  target_compile_options(TestGitExecutor PRIVATE ${MSVC_STDEXT_SHIM})
endif()

add_test(NAME TestGitExecutor COMMAND TestGitExecutor)

add_executable(TestGitModels TestGitModels.cpp
               ${CMAKE_SOURCE_DIR}/src/GitRepository.cpp
               ${CMAKE_SOURCE_DIR}/src/GitRepository.h
               ${CMAKE_SOURCE_DIR}/src/CommitModel.cpp
               ${CMAKE_SOURCE_DIR}/src/CommitModel.h
               ${CMAKE_SOURCE_DIR}/src/DiffPresenter.cpp
               ${CMAKE_SOURCE_DIR}/src/DiffPresenter.h
               ${CMAKE_SOURCE_DIR}/src/GitExecutor.cpp
               ${CMAKE_SOURCE_DIR}/src/GitExecutor.h)
target_include_directories(TestGitModels PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(TestGitModels PRIVATE
                      Qt${QT_VERSION_MAJOR}::Core
                      Qt${QT_VERSION_MAJOR}::Test)
if (MSVC)
  target_compile_options(TestGitModels PRIVATE ${MSVC_STDEXT_SHIM})
endif()

add_test(NAME TestGitModels COMMAND TestGitModels)

add_executable(TestWorkingTreeModel TestWorkingTreeModel.cpp
               ${CMAKE_SOURCE_DIR}/src/WorkingTreeModel.cpp
               ${CMAKE_SOURCE_DIR}/src/WorkingTreeModel.h)
target_include_directories(TestWorkingTreeModel PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(TestWorkingTreeModel PRIVATE
                      Qt${QT_VERSION_MAJOR}::Core
                      Qt${QT_VERSION_MAJOR}::Test)
if (MSVC)
  target_compile_options(TestWorkingTreeModel PRIVATE ${MSVC_STDEXT_SHIM})
endif()

add_test(NAME TestWorkingTreeModel COMMAND TestWorkingTreeModel)
