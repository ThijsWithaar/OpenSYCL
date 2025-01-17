if(DEFINED ENV{DESTDIR})
	# When CPACK is run
	set(ACPP_SYSCONFDIR $ENV{DESTDIR}/etc)
	set(ACPP_INSTALL_PREFIX ${CMAKE_INSTALL_PREFIX})
	message(DEBUG "InstallSysConf: CPACK")
else()
	# When 'cmake install' is run
	set(ACPP_SYSCONFDIR ${CMAKE_INSTALL_PREFIX}/etc) #${CMAKE_INSTALL_FULL_SYSCONFDIR})
	set(ACPP_INSTALL_PREFIX ${CMAKE_INSTALL_PREFIX})
	message(DEBUG "InstallSysConf: INSTALL")
endif()
message(DEBUG "InstallSysConf: PROJECT_BINARY_DIR ${PROJECT_BINARY_DIR}")
message(DEBUG "InstallSysConf: ACPP_SYSCONFDIR ${ACPP_SYSCONFDIR}")
message(DEBUG "InstallSysConf: ACPP_INSTALL_PREFIX ${ACPP_INSTALL_PREFIX}")
message(DEBUG "InstallSysConf: PROJECT_BINARY_DIR ${PROJECT_BINARY_DIR}")
message(DEBUG "InstallSysConf: CMAKE_COMMAND ${CMAKE_COMMAND}")

if(UNIX)
	file(WRITE ${PROJECT_BINARY_DIR}/etc/30-adaptivecpp.conf "${ACPP_INSTALL_PREFIX}/lib/hipSYCL\n")
endif()

execute_process(COMMAND ${CMAKE_COMMAND} -E copy_directory ${PROJECT_BINARY_DIR}/etc ${ACPP_SYSCONFDIR})
