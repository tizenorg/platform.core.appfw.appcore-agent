CMAKE_MINIMUM_REQUIRED(VERSION 2.6)
PROJECT(appcore-agent C)

SET(PREFIX ${CMAKE_INSTALL_PREFIX})
SET(EXEC_PREFIX "\${prefix}")
SET(LIBDIR "\${prefix}/lib")
SET(INCLUDEDIR "\${prefix}/include")
SET(VERSION_MAJOR 1)
SET(VERSION "${VERSION_MAJOR}.1")

INCLUDE_DIRECTORIES(${CMAKE_SOURCE_DIR}/include)

ADD_DEFINITIONS("-DPREFIX=\"${PREFIX}\"")

SET(EXTRA_CFLAGS "${EXTRA_CFLAGS} -fvisibility=hidden")
SET(EXTRA_CFLAGS "${EXTRA_CFLAGS} -g -Wall")
SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${EXTRA_CFLAGS}")

SET(CMAKE_SKIP_BUILD_RPATH TRUE)

#################################################################
# Build appcore-agent Library
# ------------------------------
SET(APPCORE_AGENT "appcore-agent")
SET(SRCS_agent src/appcore-agent.c
		)
SET(HEADERS_agent appcore-agent.h)

INCLUDE(FindPkgConfig)
#pkg_check_modules(pkg_agent REQUIRED pmapi vconf sensor aul rua dlog x11)
pkg_check_modules(pkg_agent REQUIRED aul dlog sysman capi-appfw-application)
FOREACH(flag ${pkg_agent_CFLAGS})
	SET(EXTRA_CFLAGS_agent "${EXTRA_CFLAGS_agent} ${flag}")
ENDFOREACH(flag)

ADD_LIBRARY(${APPCORE_AGENT} SHARED ${SRCS_agent})
SET_TARGET_PROPERTIES(${APPCORE_AGENT} PROPERTIES SOVERSION ${VERSION_MAJOR})
SET_TARGET_PROPERTIES(${APPCORE_AGENT} PROPERTIES VERSION ${VERSION})
SET_TARGET_PROPERTIES(${APPCORE_AGENT} PROPERTIES COMPILE_FLAGS ${EXTRA_CFLAGS_agent})
TARGET_LINK_LIBRARIES(${APPCORE_AGENT} ${pkg_agent_LDFLAGS} "-ldl")

CONFIGURE_FILE(${APPCORE_AGENT}.pc.in ${APPCORE_AGENT}.pc @ONLY)

INSTALL(TARGETS ${APPCORE_AGENT} DESTINATION lib COMPONENT RuntimeLibraries)
INSTALL(FILES ${CMAKE_CURRENT_BINARY_DIR}/${APPCORE_AGENT}.pc DESTINATION lib/pkgconfig)
FOREACH(hfile ${HEADERS_agent})
	INSTALL(FILES ${CMAKE_CURRENT_SOURCE_DIR}/include/${hfile} DESTINATION include/appcore-agent)
ENDFOREACH(hfile)
