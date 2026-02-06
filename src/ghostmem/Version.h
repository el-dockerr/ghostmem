/*******************************************************************************
 * GhostMem - Virtual RAM through Transparent Compression
 * 
 * Copyright (C) 2026 Swen Kalski
 * 
 * This file is part of GhostMem.
 * 
 * GhostMem is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * GhostMem is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with GhostMem. If not, see <https://www.gnu.org/licenses/>.
 * 
 * Contact: kalski.swen@gmail.com
 ******************************************************************************/

/**
 * @file Version.h
 * @brief Version information for GhostMem library
 * 
 * This file contains version constants and helper functions to query
 * the GhostMem library version at runtime.
 * 
 * @author Swen Kalski
 * @date 2026
 */

#pragma once

#include <string>

// Version numbers
#define GHOSTMEM_VERSION_MAJOR 1
#define GHOSTMEM_VERSION_MINOR 0
#define GHOSTMEM_VERSION_PATCH 1

// Version string
#define GHOSTMEM_VERSION_STRING "1.0.1"

// Namespace for version info
namespace GhostMem {
    
    /**
     * @brief Get the major version number
     * @return Major version number
     */
    inline constexpr int GetVersionMajor() {
        return GHOSTMEM_VERSION_MAJOR;
    }
    
    /**
     * @brief Get the minor version number
     * @return Minor version number
     */
    inline constexpr int GetVersionMinor() {
        return GHOSTMEM_VERSION_MINOR;
    }
    
    /**
     * @brief Get the patch version number
     * @return Patch version number
     */
    inline constexpr int GetVersionPatch() {
        return GHOSTMEM_VERSION_PATCH;
    }
    
    /**
     * @brief Get the full version string
     * @return Version string in format "major.minor.patch"
     */
    inline std::string GetVersionString() {
        return GHOSTMEM_VERSION_STRING;
    }
    
    /**
     * @brief Get the full version as a single integer
     * @return Version encoded as (major * 10000 + minor * 100 + patch)
     */
    inline constexpr int GetVersion() {
        return GHOSTMEM_VERSION_MAJOR * 10000 + 
               GHOSTMEM_VERSION_MINOR * 100 + 
               GHOSTMEM_VERSION_PATCH;
    }
}
