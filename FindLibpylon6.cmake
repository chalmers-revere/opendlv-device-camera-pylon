# Copyright (C) 2020  Christian Berger
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.

###########################################################################
# Find libpylon6.
FIND_PATH(PYLON6_INCLUDE_DIR
          NAMES pylon/PylonBase.h
          PATHS /opt/pylon6/include/)
MARK_AS_ADVANCED(PYLON6_INCLUDE_DIR)

FIND_LIBRARY(PYLON6_PYLONBASE
             NAMES pylonbase
             PATHS ${LIBPYLON6DIR}/lib/
                    /opt/pylon6/lib/)
MARK_AS_ADVANCED(PYLON6_PYLONBASE)
FIND_LIBRARY(PYLON6_PYLONUTILITY
             NAMES pylonutility
             PATHS ${LIBPYLON6DIR}/lib/
                    /opt/pylon6/lib/)
MARK_AS_ADVANCED(PYLON6_PYLONUTILITY)
FIND_LIBRARY(PYLON6_GENAPI
             NAMES GenApi_gcc_v3_1_Basler_pylon
             PATHS ${LIBPYLON6DIR}/lib/
                    /opt/pylon6/lib/)
MARK_AS_ADVANCED(PYLON6_GENAPI)
FIND_LIBRARY(PYLON6_GCBASE
             NAMES GCBase_gcc_v3_1_Basler_pylon
             PATHS ${LIBPYLON6DIR}/lib/
                    /opt/pylon6/lib/)
MARK_AS_ADVANCED(PYLON6_GCBASE)
###########################################################################
IF (PYLON6_INCLUDE_DIR
    AND PYLON6_PYLONBASE
    AND PYLON6_PYLONUTILITY
    AND PYLON6_GENAPI
    AND PYLON6_GCBASE)
    SET(PYLON6_FOUND 1)
    SET(PYLON6_LIBRARIES ${PYLON6_PYLONBASE} ${PYLON6_PYLONUTILITY} ${PYLON6_GENAPI} ${PYLON6_GCBASE})
    SET(PYLON6_INCLUDE_DIRS ${PYLON6_INCLUDE_DIR})
ENDIF()

MARK_AS_ADVANCED(PYLON6_LIBRARIES)
MARK_AS_ADVANCED(PYLON6_INCLUDE_DIRS)

IF (PYLON6_FOUND)
    MESSAGE(STATUS "Found libpylon6: ${PYLON6_INCLUDE_DIRS}, ${PYLON6_LIBRARIES}")
ELSE ()
    MESSAGE(STATUS "Could not find libpylon6")
ENDIF()
