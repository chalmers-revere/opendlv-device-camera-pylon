# Copyright (C) 2018  Christian Berger
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
# Find libpylon5.
FIND_PATH(PYLON5_INCLUDE_DIR
          NAMES pylon/PylonIncludes.h
          PATHS /opt/pylon5/include/)
MARK_AS_ADVANCED(PYLON5_INCLUDE_DIR)

FIND_LIBRARY(PYLON5_PYLONC
             NAMES pylonc
             PATHS ${LIBPYLON5DIR}/lib/
                    /opt/pylon5/lib/
                    /opt/pylon5/lib64/)
MARK_AS_ADVANCED(PYLON5_PYLONC)

###########################################################################
IF (PYLON5_INCLUDE_DIR
    AND PYLON5_PYLONC)
    SET(PYLON5_FOUND 1)
    SET(PYLON5_LIBRARIES ${PYLON5_PYLONC})
    SET(PYLON5_INCLUDE_DIRS ${PYLON5_INCLUDE_DIR})
ENDIF()

MARK_AS_ADVANCED(PYLON5_LIBRARIES)
MARK_AS_ADVANCED(PYLON5_INCLUDE_DIRS)

IF (PYLON5_FOUND)
    MESSAGE(STATUS "Found libpylon5: ${PYLON5_INCLUDE_DIRS}, ${PYLON5_LIBRARIES}")
ELSE ()
    MESSAGE(STATUS "Could not find libpylon5")
ENDIF()
