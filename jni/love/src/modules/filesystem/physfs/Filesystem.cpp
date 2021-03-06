/**
 * Copyright (c) 2006-2014 LOVE Development Team
 *
 * This software is provided 'as-is', without any express or implied
 * warranty.  In no event will the authors be held liable for any damages
 * arising from the use of this software.
 *
 * Permission is granted to anyone to use this software for any purpose,
 * including commercial applications, and to alter it and redistribute it
 * freely, subject to the following restrictions:
 *
 * 1. The origin of this software must not be misrepresented; you must not
 *    claim that you wrote the original software. If you use this software
 *    in a product, an acknowledgment in the product documentation would be
 *    appreciated but is not required.
 * 2. Altered source versions must be plainly marked as such, and must not be
 *    misrepresented as being the original software.
 * 3. This notice may not be removed or altered from any source distribution.
 **/

#include "common/config.h"

#include <iostream>

#include "common/utf8.h"
#include "common/b64.h"

#include "Filesystem.h"

#include "SDL.h"
#include <string>

#ifdef LOVE_ANDROID
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <errno.h>
#include "common/android.h"
#endif

namespace
{
	size_t getDriveDelim(const std::string &input)
	{
		for (size_t i = 0; i < input.size(); ++i)
			if (input[i] == '/' || input[i] == '\\')
				return i;
		// Something's horribly wrong
		return 0;
	}

	std::string getDriveRoot(const std::string &input)
	{
		return input.substr(0, getDriveDelim(input)+1);
	}

	std::string skipDriveRoot(const std::string &input)
	{
		return input.substr(getDriveDelim(input)+1);
	}

#ifdef LOVE_ANDROID
	void androidDeleteAssetGameMemory (void *ptr) {
		SDL_Log ("freeing memory for in-memory game.love");
		char *game_love_data = static_cast<char*>(ptr);
		delete[] game_love_data;
	}

	bool androidMountGame (const char *filename) {
		SDL_Log ("Trying to mount %s", filename);
    SDL_RWops *asset_game_file = SDL_RWFromFile(filename, "rb");
		if (!asset_game_file) {
			SDL_Log ("Could not find %s", filename);
			return false;
		}

		Sint64 file_size = asset_game_file->size(asset_game_file);

		if (file_size == 0) {
			SDL_Log ("Could not find valid %s. File has zero size.", filename);
			return false;
		}

		char *game_love_data = new char[file_size];

		if (!game_love_data) {
			SDL_Log ("Could not allocate memory for in-memory game archive");
			return false;
		}

		size_t bytes_copied = asset_game_file->read(asset_game_file, game_love_data, sizeof(char), (size_t) file_size);

		if (bytes_copied != file_size) {
			SDL_Log ("Only copied %d of %d bytes into in-memory game archive!", bytes_copied, file_size);
			delete[] game_love_data;
			return false;
		}

		SDL_Log ("Copied %d of %d bytes into in-memory game archive", bytes_copied, file_size);

		bool result = false;
		if (PHYSFS_mountMemory (game_love_data, file_size, androidDeleteAssetGameMemory, "archive.zip", "/", 0)) {
			result = true;
			SDL_Log ("Mounting of in-memory game archive successful!");
		} else {
			SDL_Log ("Mounting of in-memory game archive failed!");
		}

		return result;
	}

	bool androidMountAssetGame () {
		return androidMountGame ("game.love");
	}

	bool androidMountSelectedGame () {
		return androidMountGame (love::android::getSelectedGameFile());
	}

	bool androidDirectoryExists(const char* path) {
		SDL_Log ("Checking directory exists for %s", path);
		struct stat s;
		int err = stat(path, &s);
		if (err == -1) {
			if (errno == ENOENT)
				return false;
			else {
				SDL_Log ("Error checking for directory %s errno = %d: %s", path, errno, strerror (errno));
				return false;
			}
		} else if(S_ISDIR(s.st_mode)) {
			SDL_Log ("Directory %s exists!", path);
			return true;
		}
		return false;
	}

	bool androidMkdir (const char* path) {
		int err = mkdir (path, 0770);
		if (err == -1) {
			SDL_Log ("Error: Could not create directory %s", path);
		    return false;
		}

		return true;
	}

	bool androidCreateStorageDirectories () {
	    std::string internal_storage_path = SDL_AndroidGetInternalStoragePath();

	    std::string save_directory = internal_storage_path + "/save";
	    if (!androidDirectoryExists (save_directory.c_str())) {
	    	if (!androidMkdir(save_directory.c_str()))
	    		return false;
	    }

	    std::string game_directory = internal_storage_path + "/game";
   	    if (!androidDirectoryExists (game_directory.c_str())) {
	    	if (!androidMkdir(game_directory.c_str()))
	    		return false;
   	    }

   	    SDL_Log ("Creating storage directories successful!");

		return true;
	}
#endif
}

namespace love
{
namespace filesystem
{
namespace physfs
{

Filesystem::Filesystem()
	: initialized(false)
	, fused(false)
	, fusedSet(false)
{
}

Filesystem::~Filesystem()
{
	if (initialized)
		PHYSFS_deinit();
}

const char *Filesystem::getName() const
{
	return "love.filesystem.physfs";
}

void Filesystem::init(const char *arg0)
{
	if (!PHYSFS_init(arg0))
		throw Exception(PHYSFS_getLastError());

#ifdef LOVE_ANDROID
	PHYSFS_permitSymbolicLinks (1);
#endif

	initialized = true;
}

void Filesystem::setFused(bool fused)
{
	if (fusedSet)
		return;
	this->fused = fused;
	fusedSet = true;
}

bool Filesystem::isFused() const
{
	if (!fusedSet)
		return false;
	return fused;
}

bool Filesystem::setIdentity(const char *ident, bool appendToPath)
{
	if (!initialized)
		return false;

	std::string old_save_path = save_path_full;

	// Store the save directory.
	save_identity = std::string(ident);

	// Generate the relative path to the game save folder.
	save_path_relative = std::string(LOVE_APPDATA_PREFIX LOVE_APPDATA_FOLDER LOVE_PATH_SEPARATOR) + save_identity;

	// Generate the full path to the game save folder.
	save_path_full = std::string(getAppdataDirectory()) + std::string(LOVE_PATH_SEPARATOR);
	if (fused)
		save_path_full += std::string(LOVE_APPDATA_PREFIX) + save_identity;
	else
		save_path_full += save_path_relative;

#ifdef LOVE_ANDROID
	std::string internal_storage_path = SDL_AndroidGetInternalStoragePath();

	std::string save_directory = internal_storage_path + "/save";

	save_path_full = std::string(SDL_AndroidGetInternalStoragePath()) + std::string("/save/") + save_identity;

	if (androidDirectoryExists (save_path_full.c_str())) {
		SDL_Log ("dir exists");
	} else {
		SDL_Log ("does not exist");
	}

	if (!androidDirectoryExists (save_path_full.c_str())) {
		if (!androidMkdir (save_path_full.c_str())) {
			SDL_Log ("Error: Could not create save directory %s!", save_path_full.c_str());
		} else {
			SDL_Log ("Save directory %s successfuly created!", save_path_full.c_str());
		}
	} else {
		SDL_Log ("Save directory %s exists!", save_path_full.c_str());
	}
#endif
	// We now have something like:
	// save_identity: game
	// save_path_relative: ./LOVE/game
	// save_path_full: C:\Documents and Settings\user\Application Data/LOVE/game

	// We don't want old read-only save paths to accumulate when we set a new
	// identity.
	if (!old_save_path.empty())
		PHYSFS_removeFromSearchPath(old_save_path.c_str());

	// Try to add the save directory to the search path.
	// (No error on fail, it means that the path doesn't exist).
	PHYSFS_addToSearchPath(save_path_full.c_str(), appendToPath);

	return true;
}

const char *Filesystem::getIdentity() const
{
	return save_identity.c_str();
}

bool Filesystem::setSource(const char *source)
{
	if (!initialized)
		return false;

	// Check whether directory is already set.
	if (!game_source.empty())
		return false;

	std::string new_search_path = source;

#ifdef LOVE_ANDROID
	if (!androidCreateStorageDirectories ()) {
		SDL_Log ("Error creating storage directories!");
	}

	if (!androidMountSelectedGame() && !androidMountAssetGame()) {
		SDL_RWops *sdcard_main = SDL_RWFromFile("/sdcard/lovegame/main.lua", "rb");

		if (sdcard_main) {
			SDL_Log ("using game from /sdcard/lovegame");
			new_search_path = "/sdcard/lovegame";
			sdcard_main->close(sdcard_main);

			if (!PHYSFS_addToSearchPath(new_search_path.c_str(), 1)) {
				SDL_Log ("mounting of %s failed", new_search_path.c_str());
				return false;
			}
		} else {
			// Neither assets/game.love or /sdcard/lovegame was mounted
			// sucessfully, therefore simply fail.
			return false;
		}
	}
#else
	// Add the directory.
	if (!PHYSFS_addToSearchPath(new_search_path.c_str(), 1)) {
		SDL_Log ("mounting of %s failed", new_search_path.c_str());
		return false;
	}
#endif

	// Save the game source.
	game_source = new_search_path;

	return true;
}

const char *Filesystem::getSource() const
{
	return game_source.c_str();
}

bool Filesystem::setupWriteDirectory()
{
	if (!initialized)
		return false;

	// These must all be set.
	if (save_identity.empty() || save_path_full.empty() || save_path_relative.empty())
		return false;

	// Set the appdata folder as writable directory.
	// (We must create the save folder before mounting it).
	if (!PHYSFS_setWriteDir(getDriveRoot(save_path_full).c_str()))
		return false;

	// Create the save folder. (We're now "at" %APPDATA%).
	if (!createDirectory(skipDriveRoot(save_path_full).c_str()))
	{
		// Clear the write directory in case of error.
		PHYSFS_setWriteDir(0);
		return false;
	}

	// Set the final write directory.
	if (!PHYSFS_setWriteDir(save_path_full.c_str()))
		return false;

	// Add the directory. (Will not be readded if already present).
	if (!PHYSFS_addToSearchPath(save_path_full.c_str(), 0))
	{
		PHYSFS_setWriteDir(0); // Clear the write directory in case of error.
		return false;
	}

	return true;
}

bool Filesystem::mount(const char *archive, const char *mountpoint, bool appendToPath)
{
	if (!initialized || !archive)
		return false;

	std::string realPath;
	std::string sourceBase = getSourceBaseDirectory();

	if (isFused() && sourceBase.compare(archive) == 0)
	{
		// Special case: if the game is fused and the archive is the source's
		// base directory, mount it even though it's outside of the save dir.
		realPath = sourceBase;
	}
	else
	{
		// Not allowed for safety reasons.
		if (strlen(archive) == 0 || strstr(archive, "..") || strcmp(archive, "/") == 0)
			return false;

		const char *realDir = PHYSFS_getRealDir(archive);
		if (!realDir)
			return false;

		realPath = realDir;

		// Always disallow mounting of files inside the game source, since it
		// won't work anyway if the game source is a zipped .love file.
		if (realPath.find(game_source) == 0)
			return false;

		realPath += LOVE_PATH_SEPARATOR;
		realPath += archive;
	}

	if (realPath.length() == 0)
		return false;

	return PHYSFS_mount(realPath.c_str(), mountpoint, appendToPath);
}

bool Filesystem::unmount(const char *archive)
{
	if (!initialized || !archive)
		return false;

	std::string realPath;
	std::string sourceBase = getSourceBaseDirectory();

	if (isFused() && sourceBase.compare(archive) == 0)
	{
		// Special case: if the game is fused and the archive is the source's
		// base directory, unmount it even though it's outside of the save dir.
		realPath = sourceBase;
	}
	else
	{
		// Not allowed for safety reasons.
		if (strlen(archive) == 0 || strstr(archive, "..") || strcmp(archive, "/") == 0)
			return false;

		const char *realDir = PHYSFS_getRealDir(archive);
		if (!realDir)
			return false;

		realPath = realDir;
		realPath += LOVE_PATH_SEPARATOR;
		realPath += archive;
	}

	const char *mountPoint = PHYSFS_getMountPoint(realPath.c_str());
	if (!mountPoint)
		return false;

	return PHYSFS_removeFromSearchPath(realPath.c_str());
}

File *Filesystem::newFile(const char *filename) const
{
	return new File(filename);
}

FileData *Filesystem::newFileData(void *data, unsigned int size, const char *filename) const
{
	FileData *fd = new FileData(size, std::string(filename));

	// Copy the data into FileData.
	memcpy(fd->getData(), data, size);

	return fd;
}

FileData *Filesystem::newFileData(const char *b64, const char *filename) const
{
	int size = strlen(b64);
	int outsize = 0;
	char *dst = b64_decode(b64, size, outsize);
	FileData *fd = new FileData(outsize, std::string(filename));

	// Copy the data into FileData.
	memcpy(fd->getData(), dst, outsize);
	delete [] dst;

	return fd;
}

const char *Filesystem::getWorkingDirectory()
{
	if (cwd.empty())
	{
#ifdef LOVE_WINDOWS

		WCHAR w_cwd[LOVE_MAX_PATH];
		_wgetcwd(w_cwd, LOVE_MAX_PATH);
		cwd = to_utf8(w_cwd);
		replace_char(cwd, '\\', '/');
#else
		char *cwd_char = new char[LOVE_MAX_PATH];

		if (getcwd(cwd_char, LOVE_MAX_PATH))
			cwd = cwd_char; // if getcwd fails, cwd_char (and thus cwd) will still be empty

		delete [] cwd_char;
#endif
	}

	return cwd.c_str();
}

const char *Filesystem::getUserDirectory()
{
	return PHYSFS_getUserDir();
}

const char *Filesystem::getAppdataDirectory()
{
#ifdef LOVE_WINDOWS
	if (appdata.empty())
	{
		wchar_t *w_appdata = _wgetenv(L"APPDATA");
		appdata = to_utf8(w_appdata);
		replace_char(appdata, '\\', '/');
	}
	return appdata.c_str();
#elif defined(LOVE_MACOSX)
	if (appdata.empty())
	{
		std::string udir = getUserDirectory();
		udir.append("/Library/Application Support");
		appdata = udir;
	}
	return appdata.c_str();
#elif defined(LOVE_LINUX)
	if (appdata.empty())
	{
		char *xdgdatahome = getenv("XDG_DATA_HOME");
		if (!xdgdatahome)
			appdata = std::string(getUserDirectory()) + "/.local/share/";
		else
			appdata = xdgdatahome;
	}
	return appdata.c_str();
#else
	return getUserDirectory();
#endif
}


const char *Filesystem::getSaveDirectory()
{
	return save_path_full.c_str();
}

std::string Filesystem::getSourceBaseDirectory() const
{
	size_t source_len = game_source.length();

	if (source_len == 0)
		return "";

	// FIXME: This doesn't take into account parent and current directory
	// symbols (i.e. '..' and '.')
#ifdef LOVE_WINDOWS
	// In windows, delimiters can be either '/' or '\'.
	size_t base_end_pos = game_source.find_last_of("/\\", source_len - 2);
#else
	size_t base_end_pos = game_source.find_last_of('/', source_len - 2);
#endif

	if (base_end_pos == std::string::npos)
		return "";

	// If the source is in the unix root (aka '/'), we want to keep the '/'.
	if (base_end_pos == 0)
		base_end_pos = 1;

	return game_source.substr(0, base_end_pos);
}

bool Filesystem::exists(const char *file) const
{
	return PHYSFS_exists(file);
}

bool Filesystem::isDirectory(const char *file) const
{
	return PHYSFS_isDirectory(file);
}

bool Filesystem::isFile(const char *file) const
{
	return exists(file) && !isDirectory(file);
}

bool Filesystem::createDirectory(const char *dir)
{
	if (PHYSFS_getWriteDir() == 0 && !setupWriteDirectory())
		return false;

	if (!PHYSFS_mkdir(dir))
		return false;
	return true;
}

bool Filesystem::remove(const char *file)
{
	if (PHYSFS_getWriteDir() == 0 && !setupWriteDirectory())
		return false;

	if (!PHYSFS_delete(file))
		return false;
	return true;
}

Data *Filesystem::read(const char *filename, int64 size) const
{
	File file(filename);

	file.open(File::READ);

	// close() is called in the File destructor.
	return file.read(size);
}

void Filesystem::write(const char *filename, const void *data, int64 size) const
{
	File file(filename);

	file.open(File::WRITE);

	// close() is called in the File destructor.
	if (!file.write(data, size))
		throw love::Exception("Data could not be written.");
}

void Filesystem::append(const char *filename, const void *data, int64 size) const
{
	File file(filename);

	file.open(File::APPEND);

	// close() is called in the File destructor.
	if (!file.write(data, size))
		throw love::Exception("Data could not be written.");
}

int Filesystem::getDirectoryItems(lua_State *L)
{
	const char *dir = luaL_checkstring(L, 1);

	char **rc = PHYSFS_enumerateFiles(dir);
	int index = 1;

	lua_newtable(L);

	for (char **i = rc; *i != 0; i++)
	{
		lua_pushstring(L, *i);
		lua_rawseti(L, -2, index);
		index++;
	}

	PHYSFS_freeList(rc);

	return 1;
}

int Filesystem::lines_i(lua_State *L)
{
	const int bufsize = 1024;
	char buf[bufsize];
	int linesize = 0;
	bool newline = false;

	File *file = luax_checktype<File>(L, lua_upvalueindex(1), "File", FILESYSTEM_FILE_T);

	// Only accept read mode at this point.
	if (file->getMode() != File::READ)
		return luaL_error(L, "File needs to stay in read mode.");

	int64 pos = file->tell();
	int64 userpos = -1;

	if (lua_isnoneornil(L, lua_upvalueindex(2)) == 0)
	{
		// User may have changed the file position.
		userpos = pos;
		pos = (int64) lua_tonumber(L, lua_upvalueindex(2));
		if (userpos != pos)
			file->seek(pos);
	}

	while (!newline && !file->eof())
	{
		// This 64-bit to 32-bit integer cast should be safe as it never exceeds bufsize.
		int read = (int) file->read(buf, bufsize);
		if (read < 0)
			return luaL_error(L, "Could not read from file.");

		linesize += read;

		for (int i = 0; i < read; i++)
		{
			if (buf[i] == '\n')
			{
				linesize -= read - i;
				newline = true;
				break;
			}
		}
	}

	if (newline || (file->eof() && linesize > 0))
	{
		if (linesize < bufsize)
		{
			// We have the line in the buffer on the stack. No 'new' and 'read' needed.
			lua_pushlstring(L, buf, linesize > 0 && buf[linesize - 1] == '\r' ? linesize - 1 : linesize);
			if (userpos < 0)
				file->seek(pos + linesize + 1);
		}
		else
		{
			char *str = 0;
			try
			{
				str = new char[linesize + 1];
			}
			catch(std::bad_alloc &)
			{
				// Can't lua_error (longjmp) in exception handlers.
			}

			if (!str)
				return luaL_error(L, "Out of memory.");

			file->seek(pos);

			// Read the \n anyway and save us a call to seek.
			if (file->read(str, linesize + 1) == -1)
			{
				delete [] str;
				return luaL_error(L, "Could not read from file.");
			}

			lua_pushlstring(L, str, str[linesize - 1] == '\r' ? linesize - 1 : linesize);
			delete [] str;
		}

		if (userpos >= 0)
		{
			// Save new position in upvalue.
			lua_pushnumber(L, (lua_Number)(pos + linesize + 1));
			lua_replace(L, lua_upvalueindex(2));
			file->seek(userpos);
		}

		return 1;
	}

	// EOF reached.
	if (userpos >= 0 && luax_toboolean(L, lua_upvalueindex(3)))
		file->seek(userpos);
	else
		file->close();

	return 0;
}

int64 Filesystem::getLastModified(const char *filename) const
{
	PHYSFS_sint64 time = PHYSFS_getLastModTime(filename);

	if (time == -1)
		throw love::Exception("Could not determine file modification date.");

	return time;
}

int64 Filesystem::getSize(const char *filename) const
{
	File file(filename);
	int64 size = file.getSize();
	return size;
}

} // physfs
} // filesystem
} // love
