set(SRCS
	LUA.h LUA.cpp
	LUAFunctions.h LUAFunctions.cpp
)
set(LIB commonlua)
engine_add_module(TARGET ${LIB} SRCS ${SRCS} DEPENDENCIES core lua53)

gtest_suite_sources(tests
	tests/LUAFunctionsTest.cpp
)
gtest_suite_deps(tests ${LIB})
