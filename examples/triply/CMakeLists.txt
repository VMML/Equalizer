
# Copyright (c) 2011-2015 Stefan Eilemann <eile@eyescale.ch>
#                    2015 Enrique G. Paredes <egparedes@ifi.uzh.ch>

set(TRIPLY_PUBLIC_HEADERS
  typedefs.h
  ply.h
  mmap.h
  lruCache.h
  meshData.h
  modelTreeBase.h
  modelTreeData.h
  modelTreeDist.h
  modelTreeLeaf.h
  modelTreeNode.h
  modelTreeRoot.h
  treeGenerator.h
  zTreeGenerator.h
  mkdTreeGenerator.h
  renderState.h)

set(TRIPLY_SOURCES
  plyfile.cpp
  mmap.cpp
  meshData.cpp
  modelTreeBase.cpp
  modelTreeData.cpp
  modelTreeDist.cpp
  modelTreeLeaf.cpp
  modelTreeNode.cpp
  modelTreeRoot.cpp
  zTreeGenerator.cpp
  mkdTreeGenerator.cpp
  renderState.cpp)


set(TRIPLY_LINK_LIBRARIES
  Collage vmmlib ${EQ_GL_LIBRARIES} ${GLEW_LIBRARY})
set(TRIPLY_INCLUDE_NAME triply)

list(APPEND CPPCHECK_EXTRA_ARGS
  --suppress=*:${CMAKE_CURRENT_SOURCE_DIR}/plyfile.cpp)

set(TRIPLY_OMIT_LIBRARY_HEADER ON)
set(TRIPLY_OMIT_EXPORT ON)
set(TRIPLY_NAMESPACE triply)
common_library(triply)
