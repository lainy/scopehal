/***********************************************************************************************************************
*                                                                                                                      *
* libscopeprotocols                                                                                                    *
*                                                                                                                      *
* Copyright (c) 2012-2022 Andrew D. Zonenberg and contributors                                                         *
* All rights reserved.                                                                                                 *
*                                                                                                                      *
* Redistribution and use in source and binary forms, with or without modification, are permitted provided that the     *
* following conditions are met:                                                                                        *
*                                                                                                                      *
*    * Redistributions of source code must retain the above copyright notice, this list of conditions, and the         *
*      following disclaimer.                                                                                           *
*                                                                                                                      *
*    * Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the       *
*      following disclaimer in the documentation and/or other materials provided with the distribution.                *
*                                                                                                                      *
*    * Neither the name of the author nor the names of any contributors may be used to endorse or promote products     *
*      derived from this software without specific prior written permission.                                           *
*                                                                                                                      *
* THIS SOFTWARE IS PROVIDED BY THE AUTHORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED   *
* TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL *
* THE AUTHORS BE HELD LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES        *
* (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR       *
* BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT *
* (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE       *
* POSSIBILITY OF SUCH DAMAGE.                                                                                          *
*                                                                                                                      *
***********************************************************************************************************************/

#include "scopehal.h"
#include "PipelineCacheManager.h"

#ifdef _WIN32
#include <windows.h>
#include <shlwapi.h>
#include <shlobj.h>
#else
#include <sys/stat.h>
#include <wordexp.h>
#endif

using namespace std;

unique_ptr<PipelineCacheManager> g_pipelineCacheMgr;

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Construction / destruction

/**
	@brief
 */
PipelineCacheManager::PipelineCacheManager()
{
	FindPath();
}

/**
	@brief Destroys the cache and writes it out to disk
 */
PipelineCacheManager::~PipelineCacheManager()
{
	SaveToDisk();
	Clear();
}

void PipelineCacheManager::FindPath()
{
#ifdef _WIN32
	wchar_t* stem;
	if(S_OK != SHGetKnownFolderPath(
		FOLDERID_RoamingAppData,
		KF_FLAG_CREATE,
		NULL,
		&stem))
	{
		throw std::runtime_error("failed to resolve %appdata%");
	}

	wchar_t directory[MAX_PATH];
	if(NULL == PathCombineW(directory, stem, L"glscopeclient"))
	{
		throw runtime_error("failed to build directory path");
	}

	// Ensure the directory exists
	const auto result = CreateDirectoryW(directory, NULL);
	m_cacheRootDir = NarrowPath(directory);

	if(!result && GetLastError() != ERROR_ALREADY_EXISTS)
	{
		throw runtime_error("failed to create preferences directory");
	}

	CoTaskMemFree(static_cast<void*>(stem));
#else
	// Ensure all directories in path exist
	CreateDirectory("~/.cache");
	CreateDirectory("~/.cache/glscopeclient");
	m_cacheRootDir = ExpandPath("~/.cache/glscopeclient");
#endif

	LogNotice("Cache root directory is %s\n", m_cacheRootDir.c_str());
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Actual cache logic

/**
	@brief Removes all content from the cache
 */
void PipelineCacheManager::Clear()
{
	lock_guard<mutex> lock(m_mutex);
	m_vkCache.clear();
	m_rawDataCache.clear();
}

/**
	@brief Look up a blob which may or may not be in the cache
 */
shared_ptr< vector<uint32_t> > PipelineCacheManager::LookupRaw(const string& key)
{
	lock_guard<mutex> lock(m_mutex);
	if(m_rawDataCache.find(key) != m_rawDataCache.end())
	{
		LogTrace("Hit for raw %s\n", key.c_str());
		return m_rawDataCache[key];
	}

	LogTrace("Miss for raw %s\n", key.c_str());
	return nullptr;
}

/**
	@brief Store a raw blob to the cache
 */
void PipelineCacheManager::StoreRaw(const string& key, shared_ptr< vector<uint32_t> > value)
{
	lock_guard<mutex> lock(m_mutex);
	m_rawDataCache[key] = value;

	LogTrace("Store raw: %s (%zu words)\n", key.c_str(), value->size());
}

/**
	@brief Returns a Vulkan pipeline cache object for the given path.

	If not found, a new cache object is created and returned.
 */
shared_ptr<vk::raii::PipelineCache> PipelineCacheManager::Lookup(const string& key)
{
	lock_guard<mutex> lock(m_mutex);

	//Already in the cache? Return that copy
	if(m_vkCache.find(key) != m_vkCache.end())
	{
		LogTrace("Hit for pipeline %s\n", key.c_str());
		return m_vkCache[key];
	}

	//Nope, make a new empty cache object and return it
	LogTrace("Miss for pipeline %s\n", key.c_str());
	vk::PipelineCacheCreateInfo info({},{});
	auto ret = make_shared<vk::raii::PipelineCache>(*g_vkComputeDevice, info);
	m_vkCache[key] = ret;
	return ret;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Serialization

/**
	@brief Loads cache content from disk
 */
void PipelineCacheManager::LoadFromDisk()
{
}

/**
	@brief Writes cache content out to disk
 */
void PipelineCacheManager::SaveToDisk()
{
	string rootDir = "";
}